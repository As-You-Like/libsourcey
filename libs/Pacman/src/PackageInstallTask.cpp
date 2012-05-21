//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
// LibSourcey is is distributed under a dual license that allows free, 
// open source use and closed source use under a standard commercial
// license.
//
// Non-Commercial Use:
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// Commercial Use:
// Please contact mail@sourcey.com
//


#include "Sourcey/Pacman/PackageInstallTask.h"
#include "Sourcey/Pacman/PackageManager.h"
#include "Sourcey/Pacman/Package.h"
#include "Sourcey/Logger.h"

#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/Format.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/Delegate.h"
#include "Poco/Zip/Decompress.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/Net/HTTPBasicCredentials.h"


using namespace std;
using namespace Poco;


namespace Sourcey { 
namespace Pacman {


PackageInstallTask::PackageInstallTask(PackageManager& manager, LocalPackage* local, RemotePackage* remote) :
	_manager(manager),
	_local(local),
	_remote(remote),
	_cancelled(false)
{
	assert(valid());
}


PackageInstallTask::~PackageInstallTask()
{
}


void PackageInstallTask::start()
{
	Log("debug", this) << "Starting" << endl;

	_thread.start(*this);
}


void PackageInstallTask::cancel()
{
	Log("debug", this) << "Cancelling" << endl;

	_cancelled = true;
	_transaction.cancel();
	_thread.join();
}


void PackageInstallTask::run()
{
	try {
		// If the package failed previously we might need
		// to clear the file cache.
		if (_manager.options().clearFailedCache)
			_manager.clearPackageCache(*_local);		

		// Kick off the state machine. If any errors are
		// encountered an exception will be thrown and the
		// task will fail.
		doDownload();
		doUnpack();
		doFinalize();
	}
	catch (Exception& exc) {		
		Log("error", this) << "Install Failed: " << exc.message() << endl; 
		setState(this, PackageInstallState::Failed, exc.message());
	}

	setComplete();
	delete this;
}


void PackageInstallTask::onStateChange(PackageInstallState& state, const PackageInstallState& oldState)
{
	Log("debug", this) << state.toString() << endl; 

	// Check for cancellation each time the state is transitioned
	// and throw an exception to break out of the current scope.
	if (_cancelled)
		throw Exception("The installation task was cancelled by the user.");

	// Set the package install task so we know from which state to
	// resume installation.
	// TODO: Should this be reset by the clearFailedCache option?
	_local->setInstallState(state.toString());

	switch (state.id()) {
	case PackageInstallState::Installed:
		_local->setState("Installed");
		_local->clearErrors();
		_local->setVersion(_local->latestAsset().version());
		break;

	case PackageInstallState::Failed:
		_local->setState("Failed");
		if (!state.message().empty())
			_local->addError(state.message());
		break;

	default: 
		_local->setState("Installing");	
	}

	StatefulSignal<PackageInstallState>::onStateChange(state, oldState);
}


void PackageInstallTask::doDownload()
{
	setState(this, PackageInstallState::Downloading);

	//if (_local->isLatestVersion(_remote->latestAsset()))
	//	throw Exception("This local package is already up to date");
	
	Package::Asset localAsset = _local->latestAsset();
	Package::Asset remoteAsset = _remote->latestAsset();

	string uri = remoteAsset.url();
	string filename = remoteAsset.fileName();
	if (uri.empty() || filename.empty())
		throw Exception("Package download failed: No mirrors");
	
	// If the local package manifest already has a listing of
	// the latest remote asset, and the file already exists in
	// the cache we can skip the download.
	if (_manager.hasCachedFile(localAsset.fileName()) && 
		localAsset == remoteAsset) {
		Log("debug", this) << "File exists, skipping download" << endl;		
		setState(this, PackageInstallState::Unpacking);
		return;
	}
	
	// Copy the new remote asset to our local manifest.
	localAsset = _local->copyAsset(remoteAsset);

	// Initialize a HTTP transaction to download the file.
	// If the transaction fails an exception will be thrown.
	HTTP::Request* request = new HTTP::Request("GET", uri);	
	if (!_manager.options().httpUsername.empty()) {
		Poco::Net::HTTPBasicCredentials cred(
			_manager.options().httpUsername, 
			_manager.options().httpPassword);
		cred.authenticate(*request); 
	}
	
	Log("debug", this) << "Initializing Download: Ready" << endl;

	_transaction.setRequest(request);
	_transaction.setOutputPath(_manager.getCacheFilePath(localAsset.fileName()).toString());
	if (!_transaction.send())
		throw Exception(format("Failed to download package files: HTTP Error: %d %s", 
			static_cast<int>(_transaction.response().getStatus()), 
			_transaction.response().getReason()));	
	
	Log("debug", this) << "Initializing Download: OK" << endl; 
	
	// Transition the internal state since the HTTP
	// transaction was a success.
	setState(this, PackageInstallState::Unpacking);
}


void PackageInstallTask::doUnpack()
{
	setState(this, PackageInstallState::Unpacking);
		
	Package::Asset localAsset = _local->latestAsset();
	if (!localAsset.valid())
		throw Exception("The local package can't be extracted");
	
	// Get the input file and check veracity
	Path filePath(_manager.getCacheFilePath(localAsset.fileName()));
	if (!File(filePath).exists())
		throw Exception("The local package file does not exist: " + filePath.toString());	
	if (!_manager.isSupportedFileType(localAsset.fileName()))
		throw Exception("The local package has an unrecognized file extension: " + filePath.getExtension());
	
	// Create the output directory
	Path outputDir(_manager.getIntermediatePackageDir(_local->name()));
	
	Log("debug", this) << "Unpacking: " 
		<< filePath.toString() << " to "
		<< outputDir.toString() << endl;

	// Clear the local installation manifest before extraction
	_local->manifest().root.clear();
	
	// Extract the archive filed. An exception will be thrown
	// if any errors are encountered and the task will fail.
	ifstream in(filePath.toString().c_str(), ios::binary);
	Poco::Zip::Decompress c(in, outputDir);
	c.EError += Poco::Delegate<PackageInstallTask, pair<const Poco::Zip::ZipLocalFileHeader, const string> >(this, &PackageInstallTask::onDecompressionError);
	c.EOk +=Poco::Delegate<PackageInstallTask, pair<const Poco::Zip::ZipLocalFileHeader, const Poco::Path> >(this, &PackageInstallTask::onDecompressionOk);

	c.decompressAllFiles();
	c.EError -= Poco::Delegate<PackageInstallTask, pair<const Poco::Zip::ZipLocalFileHeader, const string> >(this, &PackageInstallTask::onDecompressionError);
	c.EOk -=Poco::Delegate<PackageInstallTask, pair<const Poco::Zip::ZipLocalFileHeader, const Poco::Path> >(this, &PackageInstallTask::onDecompressionOk);
	
	// Transition the internal state on success
	setState(this, PackageInstallState::Finalizing);
}


void PackageInstallTask::onDecompressionError(const void*, pair<const Poco::Zip::ZipLocalFileHeader, const string>& info)
{
	Log("error", this) << "Decompression Error: " << info.second << endl;

	// Extraction failed, throw an exception
	throw Exception("Archive Error: Extraction failed: " + info.second);
}


void PackageInstallTask::onDecompressionOk(const void*, pair<const Poco::Zip::ZipLocalFileHeader, const Poco::Path>& info)
{
	Log("debug", this) << "Decompression Success: " 
		<< info.second.toString() << endl; 
	
	// Add the extracted file to out package manifest
	_local->manifest().addFile(info.second.toString());
}


void PackageInstallTask::doFinalize() 
{
		Log("debug", this) << "doFinalize" << endl;
	setState(this, PackageInstallState::Finalizing);
		Log("debug", this) << "doFinalize 1" << endl;
		Log("debug", this) << "doFinalize 11: " << _local->name() << endl;

	bool errors = false;
	Path outputDir(_manager.getIntermediatePackageDir(_local->name()));
	
		Log("debug", this) << "doFinalize 2" << endl;

	// Move all extracted files to the installation path
	DirectoryIterator fIt(outputDir);
	DirectoryIterator fEnd;
	while (fIt != fEnd)
	{
		try
		{
			Log("debug", this) << "Moving: " << fIt.path().toString() << endl;
			File(fIt.path()).moveTo(_manager.options().installDir);
		}
		catch (Exception& exc)
		{
			// The previous version files may be currently in use,
			// in which case PackageManager::finalizeInstallations()
			// must be called from an external process before the
			// installation can be completed.
			errors = true;
			Log("error", this) << "Error: " << exc.message() << endl;
			_local->addError(exc.message());
		}
		
		++fIt;
	}

		Log("debug", this) << "doFinalize 3" << endl;

	// The package requires finalizing at a later date. 
	// The current task will be terminated.
	if (errors) {
		Log("debug", this) << "Finalization failed" << endl;
		_cancelled = true;
		return;
	}
		Log("debug", this) << "doFinalize 4" << endl;
	
	// Remove the temporary output folder if the installation
	// was successfully finalized.
	try
	{		
		Log("debug", this) << "Removing temp directory: " << outputDir.toString() << endl;

		// FIXME: How to remove a folder properly?
		File(outputDir).remove(true);
	}
	catch (Exception& exc)
	{
		// While testing on a windows system this fails regularly
		// with a file sharing error, but since the package is already
		// installed we can just swallow it.
		Log("warn", this) << "Cannot remove temp directory: " << exc.message() << endl;
	}

	Log("debug", this) << "Finalization Complete" << endl;
	
	// Transition the internal state if finalization was a success.
	// This will complete the installation process.
	setState(this, PackageInstallState::Installed);
}


void PackageInstallTask::setComplete()
{
	{
		FastMutex::ScopedLock lock(_mutex);

		Log("info", this) << "Package Install Complete:" 
			<< "\n\tName: " << _local->name()
			<< "\n\tVersion: " << _local->version()
			<< "\n\tPackage State: " << _local->state()
			<< "\n\tPackage Install State: " << _local->installState()
			<< endl;
#ifdef _DEBUG
		_local->print(cout);	
#endif
	}
	
	// The task will be destroyed
	// as a result of this signal.
	TaskComplete.dispatch(this);
}


bool PackageInstallTask::valid() const
{
	FastMutex::ScopedLock lock(_mutex);
	return !stateEquals(PackageInstallState::Failed) 
		&& _local->valid() 
		&& (!_remote || _remote->valid());
}


bool PackageInstallTask::cancelled() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _cancelled;
}


bool PackageInstallTask::failed() const
{
	return stateEquals(PackageInstallState::Failed);
}


bool PackageInstallTask::success() const
{
	return stateEquals(PackageInstallState::Installed);
}


LocalPackage* PackageInstallTask::local() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _local;
}


RemotePackage* PackageInstallTask::remote() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _remote;
}


} } // namespace Sourcey::Pacman



	/*
	case PackageInstallState::Downloading:
		doDownload();
		break;

	case PackageInstallState::Unpacking:
		doUnpack();
		break;

	case PackageInstallState::Finalizing:
		doFinalize();
		break;
	*/


		/*
void PackageInstallTask::printLog(std::ostream& ost) const
{
	ost << "["
		<< className()
		<< ":"
		<< local()->name()
		<< ":"
		<< state()
		<< "] ";
}


		_local->setState("Installed");
		_local->clearErrors();
		_local->setVersion(_local->latestAsset().version());

		_local->setState("Failed");
		if (!state.message().empty())
			_local->addError(state.message());
	//ITask(manager.runner(), false, false, local->name()),
			*/

/*
	//transaction->TransactionComplete += delegate(this, &PackageInstallTask::onTransactionComplete);
void PackageInstallTask::onTransactionComplete(void* sender, HTTP::Response& response)
{
	Log("debug", this) << "Download Complete: " << response.success() << endl;
	
	if (response.success())		
		setState(this, PackageInstallState::Unpacking);
	else
		setState(this, PackageInstallState::Failed, response.error);
}
*/

		/*
		case PackageInstallState::Incomplete:
			_local->setState("Incomplete");

			Log("info", this) << "Package Incomplete:" 
				<< "\n\tName: " << _local->name()
				<< "\n\tVersion: " << _local->version()
				<< endl;
#ifdef _DEBUG
			_local->print(cout);	
#endif
			break;

			Log("info", this) << "Package Installed:" 
				<< "\n\tName: " << _local->name()
				<< "\n\tVersion: " << _local->version()
				<< endl;
#ifdef _DEBUG
			_local->print(cout);	
#endif
			*/

			/*
			_local->setState("Installing");
			_local->setInstallState("Downloading");
			_local->setInstallState("Finalizing");
			setComplete(format("%s successfully updated to version %s", 
				_local->name(), 
				_remote->latestAsset().version()));
				*/
/*



void PackageInstallTask::setFailed(const string& message) 
{ 
	if (!stateEquals(PackageInstallState::Failed)) {		
		Log("error", this) << "Package Install Failed: " << message << endl;
		if (message.length)
			_local->addError(message);
		_local->setState("Failed");
		_local->setInstallState("Failed");		
		_manager.saveLocalPackage(*local);
		setState(this, PackageInstallState::Installed, message);
	}
};


void PackageInstallTask::setComplete(const string& message)
{	
	if (!stateEquals(PackageInstallState::Incomplete)) {
	if (!stateEquals(PackageInstallState::Installed)) {
		_local->setState("Installed");
		_local->setInstallState("Installed");
		_local->setVersion(_local->latestAsset().version());
		_manager.saveLocalPackage(*local);
		setState(this, PackageInstallState::Installed, message);		

		Log("info", this) << "Package Installed:" 
			<< "\n\tName: " << _local->name()
			<< "\n\tVersion: " << _local->version()
			<< endl;
#ifdef _DEBUG
		_local->print(cout);	
#endif
	}
}


void PackageInstallTask::setIncomplete(const string& message)
{	
	if (!stateEquals(PackageInstallState::Incomplete)) {
		_local->setState("Incomplete");
		_local->setInstallState("Incomplete");
		//_local->setVersion(_local->latestAsset().version());
		_manager.saveLocalPackage(*local);
		setState(this, PackageInstallState::Incomplete, message);		

		Log("info", this) << "Package Incomplete:" 
			<< "\n\tName: " << _local->name()
			<< "\n\tVersion: " << _local->version()
			<< endl;
#ifdef _DEBUG
		_local->print(cout);	
#endif
	}
}


	virtual void onStateChange(T& state, const T& oldState);
void PackageInstallTask::transitionState()
{
	
		// If any errors are enountered we set the overall
		// state to failed.
		//if (state.error)
		//	throw Exception(state.message());

	try {
		switch (state().id()) {	
		case PackageInstallState::None:
			_local->setState("Installing");
			_local->setInstallState("Installing");
			doDownload();
			break;

		case PackageInstallState::Downloading:
			_local->setInstallState("Downloading");
			doUnpack();
			break;

		case PackageInstallState::Unpacking:
			_local->setInstallState("Finalizing");
			doFinalize();
			break;

		case PackageInstallState::Finalizing:
			setComplete(format("%s successfully updated to version %s", 
				_local->name(), 
				_remote->latestAsset().version()));
			break;

		default: assert(false);
		}
	}
	catch (Exception& exc) {
		setFailed(exc.message());
	}
}
*/



/*
bool PackageInstallTask::setState(unsigned int id, const string& message) 
{
	if (Stateful<PackageInstallState>::setState(id, message)) {

		try {
			switch (id) {	
			case PackageInstallState::Downloading:
				doDownload();
				break;

			case PackageInstallState::Unpacking:
				doUnpack();
				break;

			case PackageInstallState::Finalizing:
				doFinalize();
				break;

			case PackageInstallState::FinalizingInstalled:
				setComplete(format("%s successfully updated to version %s", 
					manifest.name(), 
					string(manifest.latestAsset().attribute("version").value())));
				break;
			}
		}
		catch (Exception& exc) {
			setFailed(exc.message());
		}

		return true;
	}
	return false;
}
*/


/*
void PackageInstallTask::parsePackageResponse(JSON::Document& response)
{
	Log("debug", this) << "Parsing Updated Package" << endl; 
	
	JSON::Node updatedPackage = response.select_single_node(format("//package[@name='%s']", manifest.name()).data()).node();
	if (updatedPackage.empty())
		setFailed("No manifest was returned from the server");
	else if (updatedPackage.child("update").empty())
		setComplete("No updates are available");
	else {
		manifest.remove_child("package");
		ostringstream ss;
		updatedPackage.print(ss);
		manifest.load(ss.str().data());
		setState(this, PackageInstallState::QueryingPackageListInstalled);
	}
}
*/