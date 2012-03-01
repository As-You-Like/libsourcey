//
// This software is copyright by Sourcey <mail@sourcey.com> and is distributed under a dual license:
// Copyright (C) 2005 Sourcey
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


#include "Sourcey/Pacman/PackageManager.h"
#include "Sourcey/Pacman/Package.h"
#include "Sourcey/JSON/JSON.h"

#include "Sourcey/Util.h"

#include "Poco/Format.h"
#include "Poco/StreamCopier.h"
#include "Poco/DirectoryIterator.h"

#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPStreamFactory.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/FilePartSource.h"
#include "Poco/Net/NetException.h"


using namespace std;
using namespace Poco;
using namespace Poco::Net;


namespace Sourcey { 
namespace Pacman {


//
// TODO: 
// 1) Check the integrity and version of package manifest
//		files before overwriting.
// 2) Ensure there is only a single manifest file per folder.
// 3) Generate update reports.
// 4) Generate a package profile JSON document for keeping
//	track of installed packages.
// 5) Ensure no updates are active for current package before updaing.
//
	

PackageManager::PackageManager(const Options& options) :
	_options(options)
{
}


PackageManager::~PackageManager()
{
}

	
// ---------------------------------------------------------------------
//
//	Initialization Methods
//
// ---------------------------------------------------------------------
void PackageManager::initialize()
{
	createDirectories();
	loadLocalPackages();
}


bool PackageManager::initialized() const
{
	Mutex::ScopedLock lock(_mutex);
	return !_remotePackages.empty()
		|| !_localPackages.empty();
}


void PackageManager::uninitialize()
{	
	abortAllTasks();
	
	Mutex::ScopedLock lock(_mutex);
	_remotePackages.clear();
	_localPackages.clear();
}


void PackageManager::abortAllTasks()
{
	Mutex::ScopedLock lock(_mutex);
	PackageInstallTaskList::iterator it = _tasks.begin();
	while (it != _tasks.end()) {
		(*it)->cancel();
		it = _tasks.erase(it);
	}
}


void PackageManager::createDirectories()
{
	File(_options.cacheDir).createDirectories();
	File(_options.interDir).createDirectories();
	File(_options.installDir).createDirectories();
}


void PackageManager::queryRemotePackages()
{	
	Mutex::ScopedLock lock(_mutex);
	
	Log("debug") << "[PackageManager] Querying Packages: " << _options.endpoint << endl;

	if (!_tasks.empty())
		throw Exception("Please cancel all package tasks before re-loading packages");	

	try {

		// Make the API call to retreive the remote manifest
		URI uri(_options.endpoint);
		HTTPClientSession session(uri.getHost(), uri.getPort());
		HTTPRequest request("GET", _options.indexURI, HTTPMessage::HTTP_1_1);
		if (!_options.httpUsername.empty()) {
			HTTPBasicCredentials cred(_options.httpUsername, _options.httpPassword);
			cred.authenticate(request);
		}
		request.write(cout);
		session.setTimeout(Timespan(10,0));
		session.sendRequest(request);
		HTTPResponse response;
		istream& rs = session.receiveResponse(response);
		string data;
		StreamCopier::copyToString(rs, data);

		Log("debug") << "[PackageManager] Package Query Response:" 
			<< "\n\tStatus: " << response.getStatus()
			<< "\n\tReason: " << response.getReason()
			<< "\n\tResponse: " << data
			<< endl;
		
		JSON::Value root;		
		JSON::Reader reader;
		bool res = reader.parse(data, root);
		if (!res)
			throw Exception(reader.getFormatedErrorMessages());
		
		for (JSON::ValueIterator it = root.begin(); it != root.end(); it++) {		
			RemotePackage* package = new RemotePackage(*it);
			if (!package->valid()) {
				Log("error") << "[PackageManager] Unable to parse package: " << package->name() << endl;
				delete package;
				continue;
			}
			_remotePackages.add(package->name(), package);
		}
	}
	catch (Exception& exc) {
		Log("error") << "[PackageManager] Package Query Error: " << exc.displayText() << endl;
	}
}


void PackageManager::loadLocalPackages()
{
	string dir;
	{
		Mutex::ScopedLock lock(_mutex);

		if (!_tasks.empty())
			throw Exception("Please cancel all package tasks before re-loading packages");	

		_localPackages.clear();
		dir = _options.interDir;
	}
	loadLocalPackages(dir);
}


void PackageManager::loadLocalPackages(const string& dir)
{
	Log("debug") << "[PackageManager] Loading manifests: " << dir << endl;		
	Mutex::ScopedLock lock(_mutex);

	DirectoryIterator fIt(dir);
	DirectoryIterator fEnd;
	while (fIt != fEnd)
	{		
		if (fIt.name().find("manifest") != string::npos &&
			fIt.name().find(".json") != string::npos) {
				
			Log("debug") << "[PackageManager] Package found: " << fIt.name() << endl;
			try {
				JSON::Value root;
				JSON::loadFile(root, fIt.path().toString());
				LocalPackage* package = new LocalPackage(root);
				if (!package->valid()) {
					delete package;
					throw Exception("The local package is invalid");
				}

				_localPackages.add(package->name(), package);
			}
			catch (Exception& exc) {
				Log("error") << "[PackageManager] Load Error: " << exc.displayText() << endl;
				//if (whiny)
				//	exc.rethrow();
			}
		}

		++fIt;
	}
}


bool PackageManager::saveLocalPackages(bool whiny)
{
	bool res = true;
	
	ScopedLockWithUnlock<Mutex> lock(_mutex);
	LocalPackageMapT toSave(_localPackages.items());
	lock.unlock();

	for (LocalPackageMapT::const_iterator it = toSave.begin(); it != toSave.end(); ++it) {
		if (!saveLocalPackage(static_cast<LocalPackage&>(*it->second), whiny))
			res = false;
	}

	return res;
}


bool PackageManager::saveLocalPackage(LocalPackage& package, bool whiny)
{
	Mutex::ScopedLock lock(_mutex);
	bool res = false;

	try {
		string path(format("%s/manifest_%s.json", _options.interDir, package.name()));
		Log("debug") << "[PackageManager] Saving Package:" << path << endl;
		JSON::saveFile(package, path);
		res = true;
	}
	catch (Exception& exc) {
		Log("error") << "[PackageManager] Save Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}
	return res;
}


// ---------------------------------------------------------------------
//
//	Package Installation Methods
//
// ---------------------------------------------------------------------

bool PackageManager::installPackage(const string& name, PackageInstallMonitor* monitor, bool whiny)
{	
	Log("debug") << "[PackageManager] Installing Package: " << name << endl;	

	try 
	{
		// Always try to update our remote package list if
		// it's unpopulated.
		// TODO: Store to cache file and unload when all tasks
		// complete.
		if (_remotePackages.empty())
			queryRemotePackages();

		PackagePair pair = getOrCreatePackagePair(name);
		
	Log("debug") << "[PackageManager] Installing Package 1: " << name << endl;	

		// Check the veracity of existing packages.
		// If the package is installed, the manifest is complete
		// and the package is up to date we have nothing to do, 
		// just return as success.
		if (pair.local.isInstalled()) {
	Log("debug") << "[PackageManager] Installing Package 2: " << name << endl;	
			if (checkInstallManifest(pair.local)) {
	Log("debug") << "[PackageManager] Installing Package 3: " << name << endl;	
				Log("debug") 
					<< pair.local.name() << " manifest is complete" << endl;			
				if (isLatestVersion(pair)) {
					Log("info") 
						<< pair.local.name() << " is already up to date: " 
						<< pair.local.version() << " >= " 
						<< pair.remote.latestAsset().version() << endl;
					return true;
				}
			}
		}
	Log("debug") << "[PackageManager] Installing Package 4: " << name << endl;	
	
		Log("info") 
			<< pair.local.name() << " is updating: " 
			<< pair.local.version() << " <= " 
			<< pair.remote.latestAsset().version() << endl;
		
	Log("debug") << "[PackageManager] Installing Package 5: " << name << endl;	

		PackageInstallTask* task = createPackageInstallTask(pair);	
		if (monitor)
			monitor->addTask(task);
	}
	catch (Exception& exc) 
	{
		Log("error") << "[PackageManager] Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}
	
	return true;
}


bool PackageManager::installPackages(const StringList& names, PackageInstallMonitor* monitor, bool whiny)
{	
	bool res = true;
	for (StringList::const_iterator it = names.begin(); it != names.end(); ++it) {
		if (!installPackage(*it, monitor, whiny))
			res = false;
	}
	return res;
}


bool PackageManager::updatePackage(const string& name, PackageInstallMonitor* monitor, bool whiny)
{	
	// An update action is essentially the same as an install
	// action, except we will make sure local package exists 
	// before we continue.
	{
		Mutex::ScopedLock lock(_mutex);
		if (!_localPackages.exists(name)) {
			string error("Update Failed: " + name + " is not installed");
			Log("error") << "[PackageManager] " << error << endl;	
			if (whiny)
				throw Exception(error);
			else
				return false;
		}
	}

	return installPackage(name, monitor, whiny);
}


bool PackageManager::updatePackages(const StringList& names, PackageInstallMonitor* monitor, bool whiny)
{
	bool res = true;
	for (StringList::const_iterator it = names.begin(); it != names.end(); ++it) {
		if (!updatePackage(*it, monitor, whiny))		
			res = false;
	}	
	return res;
}


bool PackageManager::updateAllPackages(PackageInstallMonitor* monitor, bool whiny)
{
	bool res = true;
	
	ScopedLockWithUnlock<Mutex> lock(_mutex);
	LocalPackageMapT toUpdate(_localPackages.items());
	lock.unlock();

	for (LocalPackageMapT::const_iterator it = toUpdate.begin(); it != toUpdate.end(); ++it) {	
		if (!updatePackage(it->second->name(), monitor, whiny))
			res = false;
	}
	
	return res;
}


bool PackageManager::uninstallPackage(const string& name, bool whiny)
{
	Log("debug") << "[PackageManager] Uninstalling Package: " << name << endl;	
	Mutex::ScopedLock lock(_mutex);
	
	try 
	{
		LocalPackage* package = _localPackages.get(name, true);
		LocalPackage::Manifest manifest = package->manifest();
		if (manifest.empty())
			throw Exception("Uninstall Error: The local package manifests is empty");
	
		// Delete package files
		// NOTE: If some files fail to delete we will still
		// concider the uninstall a success.
		try 
		{			
			// Check file system for each manifest file
			LocalPackage::Manifest manifest = package->manifest();
			for (JSON::ValueIterator it = manifest.root.begin(); it != manifest.root.end(); it++) {
				Log("debug") << "[PackageManager] Uninstalling file: " << (*it).asString() << endl;	
				Path path(getInstallFilePath((*it).asString()));
				File file(path);
				file.remove();
			}
			manifest.root.clear();
		}
		catch (Exception& exc) 
		{
			Log("error") << "[PackageManager] Uninstall Error: " << exc.displayText() << endl;
		}

	
		// Delete package manifest file
		File file(format("%s/manifest_%s.json", _options.interDir, package->name()));
		file.remove();	
		
		// Free package reference from memory
		_localPackages.free(name);
	}
	catch (Exception& exc) 
	{
		Log("error") << "[PackageManager] Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}

	return true;
}


bool PackageManager::uninstallPackages(const StringList& names, bool whiny)
{	
	bool res = true;
	for (StringList::const_iterator it = names.begin(); it != names.end(); ++it) {
		if (!uninstallPackage(*it, whiny))
			res = false;
	}
	return res;
}


PackageInstallTask* PackageManager::createPackageInstallTask(PackagePair& pair) //const std::string& name, PackageInstallMonitor* monitor)
{	
	Mutex::ScopedLock lock(_mutex);

	PackageInstallTask* task = new PackageInstallTask(*this, &pair.local, &pair.remote);
	task->TaskComplete += delegate(this, &PackageManager::onPackageInstallComplete, -1);
	task->start();		
	_tasks.push_back(task);

	return task;
}


bool PackageManager::hasUnfinalizedPackages()
{	
	Mutex::ScopedLock lock(_mutex);

	Log("debug") << "[PackageManager] Checking if Finalization Required" << endl;
	
	bool res = false;	
	LocalPackageMapT toCheck(_localPackages.items());
	for (LocalPackageMapT::const_iterator it = toCheck.begin(); it != toCheck.end(); ++it) {
		if (it->second->state() == "Installing" && 
			it->second->installState() == "Finalizing") {
			Log("debug") << "[PackageManager] Finalization required: " << it->second->name() << endl;
			res = true;
		}
	}

	return res;
}


bool PackageManager::finalizeInstallations(bool whiny)
{
	Log("debug") << "[PackageManager] Finalizing Installations" << endl;
	
	Mutex::ScopedLock lock(_mutex);
	
	bool res = true;
	
	try {
		for (LocalPackageMapT::const_iterator it = _localPackages.items().begin(); it != _localPackages.items().end(); ++it) {
			if (it->second->state() == "Installing" && 
				it->second->installState() == "Finalizing") {
				Log("debug") << "[PackageManager] Finalizing: " << it->second->name() << endl;

				// Create an install task on the stack - we just
				// have to move some files so no async required.
				PackageInstallTask task(*this, it->second, NULL);
				task.doFinalize();
				saveLocalPackage(*task._local);
				if (it->second->state() != "Installed" ||
					it->second->installState() != "Installed") {
					res = false;
					if (whiny)
						throw Exception(it->second->name() + ": Finalization failed");
				}
			}
		}
	}
	catch (Exception& exc) {
		Log("error") << "[PackageManager] Finalizing Error: " << exc.displayText() << endl;
		res = false;
		if (whiny)
			exc.rethrow();
	}

	return res;
}


// ---------------------------------------------------------------------
//
// Package Helper Methods
//
// ---------------------------------------------------------------------
PackagePair PackageManager::getPackagePair(const std::string& name) const
{		
	Mutex::ScopedLock lock(_mutex);

	LocalPackage* local = _localPackages.get(name, true);
	RemotePackage* remote = _remotePackages.get(name, true);

	if (!local->valid())
		throw Exception("The local package is invalid");

	if (!remote->valid())
		throw Exception("The remote package is invalid");

	return PackagePair(*local, *remote);
}


PackagePair PackageManager::getOrCreatePackagePair(const std::string& name)
{	
	Mutex::ScopedLock lock(_mutex);
	
	Log("trace") << "[PackageManager] " << "Creating Package Pair: " << name << endl;

	RemotePackage* remote = _remotePackages.get(name, true);
	Log("trace") << "[PackageManager] " << "Creating Package Pair 1: " << name << endl;
	if (!remote->latestAsset().valid())
		throw Exception("The remote package has no file assets");
	
	Log("trace") << "[PackageManager] " << "Creating Package Pair 2: " << name << endl;

	if (!remote->valid())
		throw Exception("The remote package is invalid");
	Log("trace") << "[PackageManager] " << "Creating Package Pair 3: " << name << endl;

	// Get or create the local package description.
	LocalPackage* local = _localPackages.get(name, false);
	Log("trace") << "[PackageManager] " << "Creating Package Pair 4: " << name << endl;
	if (!local) {
		local = new LocalPackage(*remote);
	Log("trace") << "[PackageManager] " << "Creating Package Pair 5: " << name << endl;
		_localPackages.add(name, local);
	}
	
	Log("trace") << "[PackageManager] " << "Creating Package Pair 6: " << name << endl;
	if (!local->valid())
		throw Exception("The local package is invalid");
	Log("trace") << "[PackageManager] " << "Creating Package Pair 7: " << name << endl;
	
	return PackagePair(*local, *remote);
}


bool PackageManager::isLatestVersion(PackagePair& pair)
{		
	assert(pair.valid());
	if (!pair.local.isLatestVersion(pair.remote.latestAsset()))
		return false;
	return true;
}


bool PackageManager::checkInstallManifest(LocalPackage& package)
{	
	Log("debug") << "[PackageManager] " << package.name() 
		<< ": Checking install manifest" << endl;

	// Check file system for each manifest file
	LocalPackage::Manifest manifest = package.manifest();
	for (JSON::ValueIterator it = manifest.root.begin(); it != manifest.root.end(); it++) {
		Path path(getInstallFilePath((*it).asString()));
		File file(path);
		if (!file.exists()) {
			Log("error") << "[PackageManager] Local package missing file: " << path.toString() << endl;
			return false;
		}
	}
	
	return true;
}
	

// ---------------------------------------------------------------------
//
// File Helper Methods
//
// ---------------------------------------------------------------------
void PackageManager::clearCache()
{
	Path path(_options.cacheDir);
	path.makeDirectory();
	return File(path).remove();
}


bool PackageManager::clearPackageCache(LocalPackage& package)
{
	bool res = true;
	JSON::Value& assets = package["assets"];
	for (unsigned i = 0; i < assets.size(); i++) {
		Package::Asset asset(assets[i]);
		if (!clearCacheFile(asset.fileName(), false))
			res = false;
	}
	return res;
}


bool PackageManager::clearCacheFile(const std::string& fileName, bool whiny)
{
	try {
		Mutex::ScopedLock lock(_mutex);	
		Path path(_options.cacheDir);
		path.makeDirectory();
		path.setFileName(fileName);
		File(path).remove();
		return true;
	}
	catch (Exception& exc) {
		Log("error") << "[PackageManager] Clear Cache Error: " 
			<< fileName << ": " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}

	return false;
}


bool PackageManager::hasCachedFile(const std::string& fileName)
{
	Mutex::ScopedLock lock(_mutex);

	Path path(_options.cacheDir);
	path.makeDirectory();
	path.setFileName(fileName);
	return File(path).exists();
}


bool PackageManager::isSupportedFileType(const std::string& fileName)
{
	return fileName.find(".zip") != string::npos  
		|| fileName.find(".tar.gz") != string::npos;
}


Path PackageManager::getCacheFilePath(const std::string& fileName)
{
	Mutex::ScopedLock lock(_mutex);

	Path path(_options.cacheDir);
	path.makeDirectory();
	path.setFileName(fileName);
	return path;
}


Path PackageManager::getInstallFilePath(const std::string& fileName)
{
	Mutex::ScopedLock lock(_mutex);

	Path path(_options.installDir);
	path.makeDirectory();
	path.setFileName(fileName);
	return path;
}

	
Path PackageManager::getIntermediatePackageDir(const std::string& packageName)
{
	Mutex::ScopedLock lock(_mutex);

	Path dir(_options.interDir);
	dir.makeDirectory();
	dir.pushDirectory(packageName);
	File(dir).createDirectories();
	return dir;
}


// ---------------------------------------------------------------------
//
// Event Handlers
//
// ---------------------------------------------------------------------
void PackageManager::onPackageInstallComplete(void* sender)
{
	PackageInstallTask* task = reinterpret_cast<PackageInstallTask*>(sender);
	saveLocalPackage(*task->_local);
	Mutex::ScopedLock lock(_mutex);
	for (PackageInstallTaskList::iterator it = _tasks.begin(); it != _tasks.end();) {
		if (*it == task) {
			_tasks.erase(it);
			return;
		}
	}
}


} } // namespace Sourcey::Pacman









		/*

	package.print(cout);
		//package.print(cout); //.data());
		parseRemotePackageRequest(_remotePackages, data);

		// Parse the response JSON
		pugi::json_document doc;
		pugi::json_parse_result result = doc.load(data.data());
		if (!result)
			throw Exception(result.description());	
	
		// Create a package instance for each package node
		for (pugi::json_node node = doc.first_child().child("package"); 
			node; node = node.next_sibling("package")) {				
			stringstream ss;
			node.print(ss);
			RemotePackage* package = new RemotePackage();
			pugi::json_parse_result result = package->load(ss);
			if (!result) {
				delete package;
				throw Exception(result.description()); 
			}
			_remotePackages.add(package->name(), package);
		}
		*/
			/*
			LocalPackage* package = new LocalPackage();
			pugi::json_parse_result result = package->load_file(fIt.path().toString().data());
			if (!package->valid()) {				
				if (!result)
					Log("error") << "[PackageManager] Package Load Error: " << result.description() << endl;
				else
					Log("error") << "[PackageManager] Package Load Error: " << package->name() << endl;
				package->print(cout);
				delete package;
			}
			else
			*/
	
			
				//if (!file.exists())
				//	throw NotFoundException(path.toString());
			/*
			for (pugi::json_node node = manifest.first_child(); node; node = node.next_sibling()) {
				Log("debug") << "[PackageManager] Uninstalling file: " << node.attribute("path").value() << endl;	
				Path path(getInstallFilePath(node.attribute("path").value()));
				File file(path);
				//if (!file.exists())
				//	throw NotFoundException(path.toString());
				file.remove();
				manifest.remove_child(node);
			}
			*/


	/*
	if (isLatestVersion(PackagePair& pair)) {
		return NULL;
	}
	// If the package is already up to date then do nothing.
	//if (isLatestVersion(name))
		//Log("debug") << "[PackageManager] Uninstalling Package: " << name << endl;	
		//throw Exception(
		//	format("%s is already up to date: %s >= %s",
		//		name, local->version(), remote->latestAsset().version()));
		//return NULL;

	if (_remotePackages.empty())
		throw Exception("No remote package manifests have been loaded");

	// Retreive the remote package manifest or fail.
	RemotePackage* remote = _remotePackages.get(name, true);
		
	// Get or create the local package description.
	LocalPackage* local = _localPackages.get(name, false);
	if (!local) {
		local = new LocalPackage(*remote);
		_localPackages.add(name, local);
	}

	//if (monitor)
	//	monitor->addTask(task);
	*/

	/*
	DirectoryIterator dIt(_options.interDir);
	DirectoryIterator dEnd;
	while (dIt != dEnd)
	{
		//if ()

		// Finalization is required if any files exist
		// within the extraction folder's package folders.
		DirectoryIterator fIt(dIt.path());
		DirectoryIterator fEnd;
		if (fIt != fEnd)
			return true;

		++dIt;
	}
	*/
	/*
	bool errors = false;
	DirectoryIterator dIt(_options.interDir);
	DirectoryIterator dEnd;
	while (dIt != dEnd)
	{
		Log("debug") << "[PackageManager] Finalizing: Found: " << dIt.path().toString() << endl;
		
		// PackageInstallTasks are located inside their respective folders.
		DirectoryIterator fIt(dIt.path());
		DirectoryIterator fEnd;
		while (fIt != fEnd)
		{	
			try
			{
				Log("debug") << "[PackageManager] Finalizing: Installing: " << fIt.path().toString() << endl;
				File(fIt.path()).moveTo(_options.installDir);
				++fIt;
			}
			catch (Exception& exc)
			{
				// The previous version files may be currently in use,
				// in which case PackageManager::finalizeInstallations()
				// must be called from an external process before the
				// installation can be completed.
				errors = true;
				Log("error") << "[PackageInstallTask] Finalizing Error: " << exc.displayText() << endl;
			}
		}

		//if (!errors) {			
		//	File(_options.interDir).remove(true);
		//}

		++dIt;
	}
	*/
	

			/*
			if (remove(_options.interDir.data()) == 0)
				Log("debug") << "[PackageManager] Finalizing: Deleted: " << fIt.path().toString() << endl;
			else
				Log("error") << "[PackageManager] Finalizing: Delete Error: " << fIt.path().toString() << endl;
				*/
	/* 
	Mutex::ScopedLock lock(_mutex);
	//const string& name
	//if (Util::compareVersion(remote->latestAsset().version(), local->version()))
	//	return false;

	LocalPackage* local = _localPackages.get(name, true);
	RemotePackage* remote = _remotePackages.get(name, true);

	if (!local->valid())
		throw Exception("The local package is invalid");

	if (!remote->valid())
		throw Exception("The remote package is invalid");
		*/
/*	
PackagePair PackageManager::getPackagePair(const std::string& name, bool whiny)
{
}


bool PackageManager::isLatestVersion(const string& name)
{	
	Mutex::ScopedLock lock(_mutex);
	
	LocalPackage* local = _localPackages.get(name, true);
	RemotePackage* remote = _remotePackages.get(name, true);

	if (!local->valid())
		throw Exception("The local package is invalid");

	if (!remote->valid())
		throw Exception("The remote package is invalid");
		
	if (!local->isLatestVersion(remote->latestAsset())) {	
		Log("info") 
			<< name << " has a new version available: " 
			<< local->version() << " <= " 
			<< remote->latestAsset().version() << endl;
		return false;
	}

	//if (Util::compareVersion(remote->latestAsset().version(), local->version()))
	//	return false;
	
	// Check file system for each manifest file
	LocalPackage::Manifest manifest = local->manifest();
	for (pugi::json_node node = manifest.first_child(); node; node = node.next_sibling()) {
		Path path(getInstallFilePath(node.attribute("path").value()));
		File file(path);
		if (!file.exists()) {
			Log("error") << "[PackageManager] Local package missing file: " << path.toString() << endl;
			return false;
		}
	}

	try 
	{
	}
	catch (Exception& exc) 
	{
		Log("error") << "[PackageManager] Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}
	
	Log("debug") 
		<< name << " is already up to date: " 
		<< local->version() << " >= " 
		<< remote->latestAsset().version() << endl;

	return true;
}
*/
	//switch (state.id()) {
	//case PackageInstallState::Incomplete:
	//case PackageInstallState::Installed:
	//case PackageInstallState::Failed:
	//} //, PackageInstallState& state, const PackageInstallState&