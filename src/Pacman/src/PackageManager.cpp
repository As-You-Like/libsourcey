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


#include "Sourcey/Pacman/PackageManager.h"
#include "Sourcey/Pacman/Package.h"
#include "Sourcey/JSON/JSON.h"

#include "Sourcey/Util.h"

#include "Poco/Format.h"
#include "Poco/StreamCopier.h"
#include "Poco/DirectoryIterator.h"

#include "Poco/Net/HTTPBasicCredentials.h"


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
	FastMutex::ScopedLock lock(_mutex);
	return !_remotePackages.empty()
		|| !_localPackages.empty();
}


void PackageManager::uninitialize()
{	
	abortAllTasks();
	
	FastMutex::ScopedLock lock(_mutex);
	_remotePackages.clear();
	_localPackages.clear();
}


void PackageManager::abortAllTasks()
{
	FastMutex::ScopedLock lock(_mutex);
	InstallTaskList::iterator it = _tasks.begin();
	while (it != _tasks.end()) {
		(*it)->cancel();
		it = _tasks.erase(it);
	}
}


void PackageManager::createDirectories()
{
	FastMutex::ScopedLock lock(_mutex);
	File(_options.cacheDir).createDirectories();
	File(_options.interDir).createDirectories();
	File(_options.installDir).createDirectories();
}


void PackageManager::queryRemotePackages()
{	
	FastMutex::ScopedLock lock(_mutex);
	
	LogDebug() << "[PackageManager] Querying Packages: " << _options.endpoint << _options.indexURI << endl;

	if (!_tasks.empty())
		throw Exception("Please cancel all package tasks before re-loading packages.");

	try {

		// Make the API call to retrieve the remote manifest
		HTTP::Request* request = new HTTP::Request("GET", _options.endpoint + _options.indexURI);	
		if (!_options.httpUsername.empty()) {
			Poco::Net::HTTPBasicCredentials cred(_options.httpUsername, _options.httpPassword);
			cred.authenticate(*request); 
		}

		HTTP::Transaction transaction(request);
		HTTP::Response& response = transaction.response();
		if (!transaction.send())
			throw Exception(format("Failed to query packages from server: HTTP Error: %d %s", 
				static_cast<int>(response.getStatus()), response.getReason()));			

		LogDebug() << "[PackageManager] Package Query Response:" 
			<< "\n\tStatus: " << response.getStatus()
			<< "\n\tReason: " << response.getReason()
			<< "\n\tResponse: " << response.body.str()
			<< endl;
		
		JSON::Value root;
		JSON::Reader reader;
		bool res = reader.parse(response.body.str(), root);
		if (!res)
			throw Exception("Invalid Server Response: " + reader.getFormatedErrorMessages());

		_remotePackages.clear();
		
		for (JSON::ValueIterator it = root.begin(); it != root.end(); it++) {		
			RemotePackage* package = new RemotePackage(*it);
			if (!package->valid()) {
				LogError() << "[PackageManager] Invalid package: " << package->id() << endl;
				delete package;
				continue;
			}
			_remotePackages.add(package->id(), package);
		}

		LogDebug() << "[PackageManager] Querying Packages: Success" << endl;
	}
	catch (Exception& exc) {
		LogError() << "[PackageManager] Package Query Error: " << exc.displayText() << endl;
		exc.rethrow();
	}
}


void PackageManager::loadLocalPackages()
{
	string dir;
	{
		FastMutex::ScopedLock lock(_mutex);

		if (!_tasks.empty())
			throw Exception("Please cancel all package tasks before re-loading packages");	

		_localPackages.clear();
		dir = _options.interDir;
	}
	loadLocalPackages(dir);
}


void PackageManager::loadLocalPackages(const string& dir)
{
	LogDebug() << "[PackageManager] Loading manifests: " << dir << endl;		
	//FastMutex::ScopedLock lock(_mutex);

	DirectoryIterator fIt(dir);
	DirectoryIterator fEnd;
	while (fIt != fEnd)
	{		
		if (fIt.name().find("manifest") != string::npos &&
			fIt.name().find(".json") != string::npos) {
				
			LogDebug() << "[PackageManager] Package found: " << fIt.name() << endl;
			try {
				JSON::Value root;
				JSON::loadFile(root, fIt.path().toString());
				LocalPackage* package = new LocalPackage(root);
				if (!package->valid()) {
					delete package;
					throw Exception("The local package is invalid");
				}

				localPackages().add(package->id(), package);
			}
			catch (std::exception& exc) {
				LogError() << "[PackageManager] Manifest Load Error: " << exc.what() << endl;
			}
			catch (Exception& exc) {
				LogError() << "[PackageManager] Load Error: " << exc.displayText() << endl;
			}
		}

		++fIt;
	}
}


bool PackageManager::saveLocalPackages(bool whiny)
{
	LogTrace() << "[PackageManager] Saving Local Packages" << endl;

	bool res = true;
	LocalPackageMap toSave = localPackages().copy();
	for (LocalPackageMap::const_iterator it = toSave.begin(); it != toSave.end(); ++it) {
		if (!saveLocalPackage(static_cast<LocalPackage&>(*it->second), whiny))
			res = false;
	}

	LogTrace() << "[PackageManager] Saving Local Packages: OK" << endl;

	return res;
}


bool PackageManager::saveLocalPackage(LocalPackage& package, bool whiny)
{
	LogTrace() << "[PackageManager] Saving Local Package: " << package.id() << endl;

	//FastMutex::ScopedLock lock(_mutex);
	bool res = false;

	try {
		string path(format("%s/manifest_%s.json", options().interDir, package.id()));
		LogDebug() << "[PackageManager] Saving Package: " << path << endl;
		JSON::saveFile(package, path);
		res = true;
	}
	catch (Exception& exc) {
		LogError() << "[PackageManager] Save Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}	

	LogTrace() << "[PackageManager] Saving Local Package: OK: " << package.id() << endl;

	return res;
}


// ---------------------------------------------------------------------
//
//	Package Installation Methods
//
// ---------------------------------------------------------------------
InstallTask* PackageManager::installPackage(const string& name, const InstallTask::Options& options, bool whiny)
{	
	LogDebug() << "[PackageManager] Installing Package: " << name << endl;	

	try 
	{
		// Try to update our remote package list if none exist.
		// TODO: Consider storing a remote package cache file.
		if (remotePackages().empty())
			queryRemotePackages();

		PackagePair pair = getOrCreatePackagePair(name);

		// Check the existing package veracity if one exists.
		// If the package is up to date we have nothing to do 
		// so just return a NULL pointer but do not throw.
		if (pair.local.isInstalled()) {
			if (checkInstallManifest(pair.local)) {
				LogDebug() 
					<< pair.local.name() << " manifest is complete" << endl;			
				if (isLatestVersion(pair)) {
					ostringstream ost;
					ost << pair.local.name() << " is already up to date: " 
						<< pair.local.version() << " >= " 
						<< pair.remote.latestAsset().version() << endl;
					if (whiny)
						throw Exception(ost.str());
					else
						return NULL;
				}
			}
		}
	
		LogInfo() 
			<< pair.local.name() << " is updating: " 
			<< pair.local.version() << " <= " 
			<< pair.remote.latestAsset().version() << endl;

		//InstallTask* task = createInstallTask(pair, options);	
		//if (monitor)
		//	monitor->addTask(task);
	
		return createInstallTask(pair, options);
	}
	catch (Exception& exc) 
	{
		LogError() << "[PackageManager] Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}
	
	return NULL;
}


InstallMonitor* PackageManager::installPackages(const StringList& ids, const InstallTask::Options& options, bool whiny)
{	
	InstallMonitor* monitor = new InstallMonitor();
	try 
	{
		for (StringList::const_iterator it = ids.begin(); it != ids.end(); ++it) {
			InstallTask* task = installPackage(*it, options, whiny);
			if (task)
				monitor->addTask(task);
		}

		// Delete the monitor and return NULL
		// if no tasks were created.
		if (monitor->tasks().empty()) {
			delete monitor;
			return NULL;
		}
	}
	catch (Exception& exc) 
	{
		delete monitor;
		assert(whiny);
		exc.rethrow();
	}

	return monitor;
}


InstallTask* PackageManager::updatePackage(const string& name, const InstallTask::Options& options, bool whiny)
{	
	// An update action is essentially the same as an install
	// action, except we will make sure local package exists 
	// before we continue.
	{
		if (!localPackages().exists(name)) {
			string error("Update Failed: " + name + " is not installed.");
			LogError() << "[PackageManager] " << error << endl;	
			if (whiny)
				throw Exception(error);
			else
				return NULL;
		}
	}

	return installPackage(name, options, whiny);
}


InstallMonitor* PackageManager::updatePackages(const StringList& ids, const InstallTask::Options& options, bool whiny)
{	
	// An update action is essentially the same as an install
	// action, except we will make sure local package exists 
	// before we continue.
	{
		for (StringList::const_iterator it = ids.begin(); it != ids.end(); ++it) {
			if (!localPackages().exists(*it)) {
				string error("Update Failed: " + *it + " is not installed.");
				LogError() << "[PackageManager] " << error << endl;	
				if (whiny)
					throw Exception(error);
				else
					return NULL;
			}
		}
	}
	
	return installPackages(ids, options, whiny);
}


bool PackageManager::updateAllPackages(bool whiny) //InstallMonitor* monitor, 
{
	bool res = true;
	
	LocalPackageMap& packages = localPackages().items();
	for (LocalPackageMap::const_iterator it = packages.begin(); it != packages.end(); ++it) {	
		//if (!updatePackage(it->second->name(), monitor, InstallTask::Options(), whiny))
		//	res = false;	
		InstallTask::Options options;
		InstallTask* task = updatePackage(it->second->name(), options, whiny);
		if (!task)
			res = false;
		//else if (monitor)
		//	monitor->addTask(task);
	}
	
	return res;
}


bool PackageManager::uninstallPackage(const string& id, bool whiny)
{
	LogDebug() << "[PackageManager] Uninstalling Package: " << id << endl;	
	//FastMutex::ScopedLock lock(_mutex);
	
	try 
	{
		LocalPackage* package = localPackages().get(id, true);
		LocalPackage::Manifest manifest = package->manifest();
		if (manifest.empty())
			throw Exception("The local package manifest is empty.");
	
		// Delete package files
		// NOTE: If some files fail to delete we still
		// consider the uninstall a success.
		try 
		{			
			// Check file system for each manifest file
			LocalPackage::Manifest manifest = package->manifest();
			for (JSON::ValueIterator it = manifest.root.begin(); it != manifest.root.end(); it++) {
				LogDebug() << "[PackageManager] Uninstalling file: " << (*it).asString() << endl;	
				Path path(getInstallFilePath((*it).asString()));
				File file(path);
				file.remove();
			}
			manifest.root.clear();
		}
		catch (Exception& exc) 
		{
			LogError() << "[PackageManager] Uninstall Error: " << exc.displayText() << endl;
		}
	
		// Delete package manifest file
		File file(format("%s/manifest_%s.json", options().interDir, package->id()));
		file.remove();	

		// Notify the outside application
		PackageUninstalled.dispatch(this, *package);
		
		// Free package reference from memory
		localPackages().remove(package);
		delete package;
	}
	catch (Exception& exc) 
	{
		LogError() << "[PackageManager] Uninstall Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}

	return true;
}


bool PackageManager::uninstallPackages(const StringList& ids, bool whiny)
{	
	bool res = true;
	for (StringList::const_iterator it = ids.begin(); it != ids.end(); ++it) {
		if (!uninstallPackage(*it, whiny))
			res = false;
	}
	return res;
}


InstallTask* PackageManager::createInstallTask(PackagePair& pair, const InstallTask::Options& options) //const string& name, InstallMonitor* monitor)
{	
	InstallTask* task = new InstallTask(*this, &pair.local, &pair.remote, options);
	task->Complete += delegate(this, &PackageManager::onPackageInstallComplete, -1); // lowest priority to remove task
	//task->start();

	FastMutex::ScopedLock lock(_mutex);
	_tasks.push_back(task);

	return task;
}


bool PackageManager::hasUnfinalizedPackages()
{	
	//FastMutex::ScopedLock lock(_mutex);

	LogDebug() << "[PackageManager] Checking if Finalization Required" << endl;
	
	bool res = false;	
	//LocalPackageMap toCheck(_localPackages.items());
	LocalPackageMap& packages = localPackages().items();
	for (LocalPackageMap::const_iterator it = packages.begin(); it != packages.end(); ++it) {
		if (it->second->state() == "Installing" && 
			it->second->installState() == "Finalizing") {
			LogDebug() << "[PackageManager] Finalization required: " << it->second->name() << endl;
			res = true;
		}
	}

	return res;
}


bool PackageManager::finalizeInstallations(bool whiny)
{
	LogDebug() << "[PackageManager] Finalizing Installations" << endl;
	
	//FastMutex::ScopedLock lock(_mutex);
	
	bool res = true;
	
	LocalPackageMap& packages = localPackages().items();
	for (LocalPackageMap::const_iterator it = packages.begin(); it != packages.end(); ++it) {
		try {
			if (it->second->state() == "Installing" && 
				it->second->installState() == "Finalizing") {
				LogDebug() << "[PackageManager] Finalizing: " << it->second->name() << endl;

				// Create an install task on the stack - we just
				// have to move some files so no async required.
				InstallTask task(*this, it->second, NULL);
				task.doFinalize();
				
				assert(it->second->state() == "Installed" 
					&& it->second->installState() == "Installed");
				
				PackageInstalled.dispatch(this, *it->second);
				/*
				if (it->second->state() != "Installed" ||
					it->second->installState() != "Installed") {
					res = false;
					if (whiny)
						throw Exception(it->second->name() + ": Finalization failed");
				}
				*/
			}
		}
		catch (Exception& exc) {
			LogError() << "[PackageManager] Finalize Error: " << exc.displayText() << endl;
			res = false;
			if (whiny)
				exc.rethrow();
		}

		// Always save the package.
		saveLocalPackage(*it->second, false);
	}

	return res;
}


// ---------------------------------------------------------------------
//
// Package Helper Methods
//
// ---------------------------------------------------------------------
PackagePair PackageManager::getPackagePair(const string& id) const
{		
	FastMutex::ScopedLock lock(_mutex);
	LocalPackage* local = _localPackages.get(id, true);
	RemotePackage* remote = _remotePackages.get(id, true);

	if (!local->valid())
		throw Exception("The local package is invalid");

	if (!remote->valid())
		throw Exception("The remote package is invalid");

	return PackagePair(*local, *remote);
}


PackagePair PackageManager::getOrCreatePackagePair(const string& id)
{	
	RemotePackage* remote = remotePackages().get(id, true);
	if (remote->assets().empty())
		throw Exception("The remote package has no file assets.");

	if (!remote->latestAsset().valid())
		throw Exception("The remote package has invalid file assets.");

	if (!remote->valid())
		throw Exception("The remote package is invalid.");

	// Get or create the local package description.
	LocalPackage* local = localPackages().get(id, false);
	if (!local) {
		local = new LocalPackage(*remote);
		localPackages().add(id, local);
	}
	
	if (!local->valid())
		throw Exception("The local package is invalid.");
	
	return PackagePair(*local, *remote);
}


bool PackageManager::isLatestVersion(PackagePair& pair)
{		
	assert(pair.valid());
	if (!pair.local.isLatestVersion(pair.remote.latestAsset()))
		return false;
	return true;
}


string PackageManager::installedPackageVersion(const string& id) const
{
	FastMutex::ScopedLock lock(_mutex);
	LocalPackage* local = _localPackages.get(id, true);
	
	if (!local->valid())
		throw Exception("The local package is invalid.");
	if (!local->isInstalled())
		throw Exception("The local package is not installed.");

	return local->version();
}


bool PackageManager::checkInstallManifest(LocalPackage& package)
{	
	LogDebug() << "[PackageManager] " << package.id() 
		<< ": Checking install manifest" << endl;

	// Check file system for each manifest file
	LocalPackage::Manifest manifest = package.manifest();
	for (JSON::ValueIterator it = manifest.root.begin(); it != manifest.root.end(); it++) {
		Path path(getInstallFilePath((*it).asString()));
		File file(path);
		if (!file.exists()) {
			LogError() << "[PackageManager] Local package missing file: " << path.toString() << endl;
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
	Path path(options().cacheDir);
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


bool PackageManager::clearCacheFile(const string& fileName, bool whiny)
{
	try {
		Path path(options().cacheDir);
		path.makeDirectory();
		path.setFileName(fileName);
		File(path).remove();
		return true;
	}
	catch (Exception& exc) {
		LogError() << "[PackageManager] Clear Cache Error: " 
			<< fileName << ": " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}

	return false;
}


bool PackageManager::hasCachedFile(const string& fileName)
{
	Path path(options().cacheDir);
	path.makeDirectory();
	path.setFileName(fileName);
	return File(path).exists();
}


bool PackageManager::isSupportedFileType(const string& fileName)
{
	return fileName.find(".zip") != string::npos  
		|| fileName.find(".tar.gz") != string::npos;
}


Path PackageManager::getCacheFilePath(const string& fileName)
{
	FastMutex::ScopedLock lock(_mutex);

	Path path(options().cacheDir);
	path.makeDirectory();
	path.setFileName(fileName);
	return path;
}


Path PackageManager::getInstallFilePath(const string& fileName)
{
	Path path(options().installDir);
	path.makeDirectory();
	path.setFileName(fileName);
	return path;
}

	
Path PackageManager::getIntermediatePackageDir(const string& id)
{
	LogDebug() << "[PackageManager] getIntermediatePackageDir 0" << endl;
		
	Path dir(options().interDir);
	dir.makeDirectory();
	dir.pushDirectory(id);
	File(dir).createDirectories();
	return dir;
}


PackageManager::Options& PackageManager::options() 
{ 
	FastMutex::ScopedLock lock(_mutex);
	return _options;
}


RemotePackageStore& PackageManager::remotePackages() 
{ 
	FastMutex::ScopedLock lock(_mutex);
	return _remotePackages; 
}


LocalPackageStore& PackageManager::localPackages()
{ 
	FastMutex::ScopedLock lock(_mutex);
	return _localPackages; 
}


// ---------------------------------------------------------------------
//
// Event Handlers
//
// ---------------------------------------------------------------------
void PackageManager::onPackageInstallComplete(void* sender)
{
	InstallTask* task = reinterpret_cast<InstallTask*>(sender);
	
	LogTrace() << "[PackageManager] Package Install Complete: " << task->state().toString() << endl;

	PackageInstalled.dispatch(this, *task->local());

	// Save the local package
	saveLocalPackage(*task->local());

	// Remove the task reference
	FastMutex::ScopedLock lock(_mutex);
	for (InstallTaskList::iterator it = _tasks.begin(); it != _tasks.end(); it++) {
		if (*it == task) {
			_tasks.erase(it);
			return;
		}
	}
}


} } // namespace Sourcey::Pacman





	/*
	InstallMonitor* monitor = new InstallMonitor();
	try 
	{
		bool res = true;
		for (StringList::const_iterator it = names.begin(); it != names.end(); ++it) {
			InstallTask* task = updatePackage(*it, options, whiny);
			if (!task)
				res = false;
			monitor->addTask(task);
			//else if (monitor)
			//if (!updatePackage(*it, monitor, options, whiny))		
			//	res = false;
		}	
	}
	catch (Exception& exc) 
	{
		delete monitor;
		monitor = NULL;
		if (whiny)
			exc.rethrow();
	}

	return monitor;
	*/




/*
bool PackageManager::installPackage(const string& name, InstallMonitor* monitor, const InstallTask::Options& options, bool whiny)
{	
	LogDebug() << "[PackageManager] Installing Package: " << name << endl;	

	try 
	{
		// Always try to update our remote package list if
		// it's unpopulated.
		// TODO: Store to cache file and unload when all tasks
		// complete.
		if (remotePackages().empty())
			queryRemotePackages();

		// Check the remote package against provided options
		// to make sure we can proceed.
		assert(0);

		PackagePair pair = getOrCreatePackagePair(name);

		// Check the veracity of existing packages.
		// If the package is installed, the manifest is complete
		// and the package is up to date we have nothing to do, 
		// just return as success.
		if (pair.local.isInstalled()) {
			if (checkInstallManifest(pair.local)) {
				LogDebug() 
					<< pair.local.name() << " manifest is complete" << endl;			
				if (isLatestVersion(pair)) {
					LogInfo() 
						<< pair.local.name() << " is already up to date: " 
						<< pair.local.version() << " >= " 
						<< pair.remote.latestAsset().version() << endl;
					//return true;
				}
			}
		}
	
		LogInfo() 
			<< pair.local.name() << " is updating: " 
			<< pair.local.version() << " <= " 
			<< pair.remote.latestAsset().version() << endl;

		InstallTask* task = createInstallTask(pair, options);	
		if (monitor)
			monitor->addTask(task);
	}
	catch (Exception& exc) 
	{
		LogError() << "[PackageManager] Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}
	
	return true;
}


bool PackageManager::installPackages(const StringList& names, InstallMonitor* monitor, const InstallTask::Options& options, bool whiny)
{	
	bool res = true;
	for (StringList::const_iterator it = names.begin(); it != names.end(); ++it) {
		if (!installPackage(*it, monitor, options, whiny))
			res = false;
	}
	return res;
}


bool PackageManager::updatePackage(const string& name, InstallMonitor* monitor, const InstallTask::Options& options, bool whiny)
{	
	// An update action is essentially the same as an install
	// action, except we will make sure local package exists 
	// before we continue.
	{
		if (!localPackages().exists(name)) {
			string error("Update Failed: " + name + " is not installed");
			LogError() << "[PackageManager] " << error << endl;	
			if (whiny)
				throw Exception(error);
			else
				return false;
		}
	}

	return installPackage(name, monitor, options, whiny);
}


bool PackageManager::updatePackages(const StringList& names, InstallMonitor* monitor, const InstallTask::Options& options, bool whiny)
{
	bool res = true;
	for (StringList::const_iterator it = names.begin(); it != names.end(); ++it) {
		if (!updatePackage(*it, monitor, options, whiny))		
			res = false;
	}	
	return res;
}
*/



		/*
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
		*/
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
			_remotePackages.add(package->id(), package);
		}
		*/
			/*
			LocalPackage* package = new LocalPackage();
			pugi::json_parse_result result = package->load_file(fIt.path().toString().data());
			if (!package->valid()) {				
				if (!result)
					LogError() << "[PackageManager] Package Load Error: " << result.description() << endl;
				else
					LogError() << "[PackageManager] Package Load Error: " << package->id() << endl;
				package->print(cout);
				delete package;
			}
			else
			*/
	
			
				//if (!file.exists())
				//	throw NotFoundException(path.toString());
			/*
			for (pugi::json_node node = manifest.first_child(); node; node = node.next_sibling()) {
				LogDebug() << "[PackageManager] Uninstalling file: " << node.attribute("path").value() << endl;	
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
		//LogDebug() << "[PackageManager] Uninstalling Package: " << name << endl;	
		//throw Exception(
		//	format("%s is already up to date: %s >= %s",
		//		name, local->version(), remote->latestAsset().version()));
		//return NULL;

	if (_remotePackages.empty())
		throw Exception("No remote package manifests have been loaded");

	// retrieve the remote package manifest or fail.
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
		LogDebug() << "[PackageManager] Finalizing: Found: " << dIt.path().toString() << endl;
		
		// InstallTasks are located inside their respective folders.
		DirectoryIterator fIt(dIt.path());
		DirectoryIterator fEnd;
		while (fIt != fEnd)
		{	
			try
			{
				LogDebug() << "[PackageManager] Finalizing: Installing: " << fIt.path().toString() << endl;
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
				LogError() << "[InstallTask] Finalizing Error: " << exc.displayText() << endl;
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
				LogDebug() << "[PackageManager] Finalizing: Deleted: " << fIt.path().toString() << endl;
			else
				LogError() << "[PackageManager] Finalizing: Delete Error: " << fIt.path().toString() << endl;
				*/
	/* 
	FastMutex::ScopedLock lock(_mutex);
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
PackagePair PackageManager::getPackagePair(const string& name, bool whiny)
{
}


bool PackageManager::isLatestVersion(const string& name)
{	
	FastMutex::ScopedLock lock(_mutex);
	
	LocalPackage* local = _localPackages.get(name, true);
	RemotePackage* remote = _remotePackages.get(name, true);

	if (!local->valid())
		throw Exception("The local package is invalid");

	if (!remote->valid())
		throw Exception("The remote package is invalid");
		
	if (!local->isLatestVersion(remote->latestAsset())) {	
		LogInfo() 
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
			LogError() << "[PackageManager] Local package missing file: " << path.toString() << endl;
			return false;
		}
	}

	try 
	{
	}
	catch (Exception& exc) 
	{
		LogError() << "[PackageManager] Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}
	
	LogDebug() 
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