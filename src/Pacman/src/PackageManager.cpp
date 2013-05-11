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


namespace Scy { 
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
//	Initialization Methods
//
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
	cancelAllTasks();
	
	FastMutex::ScopedLock lock(_mutex);
	_remotePackages.clear();
	_localPackages.clear();
}


void PackageManager::cancelAllTasks()
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
	
	LogDebug("PackageManager", this) << "Querying Packages: " 
		<< _options.endpoint << _options.indexURI << endl;

	if (!_tasks.empty())
		throw Exception("Cannot load packages while tasks are active.");	

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

		LogDebug("PackageManager", this) << "Package Query Response:" 
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
				LogError("PackageManager", this) << "Invalid package: " << package->id() << endl;
				delete package;
				continue;
			}
			_remotePackages.add(package->id(), package);
		}

		LogDebug("PackageManager", this) << "Querying Packages: Success" << endl;
	}
	catch (Exception& exc) {
		LogError("PackageManager", this) << "Package Query Error: " << exc.displayText() << endl;
		exc.rethrow();
	}
}


void PackageManager::loadLocalPackages()
{
	string dir;
	{
		FastMutex::ScopedLock lock(_mutex);

		if (!_tasks.empty())
			throw Exception("Cannot load packages while tasks are active.");	

		_localPackages.clear();
		dir = _options.interDir;
	}
	loadLocalPackages(dir);
}


void PackageManager::loadLocalPackages(const string& dir)
{
	LogDebug("PackageManager", this) << "Loading manifests: " << dir << endl;		

	DirectoryIterator fIt(dir);
	DirectoryIterator fEnd;
	while (fIt != fEnd) {		
		if (fIt.name().find(".json") != string::npos) {				
			LogDebug("PackageManager", this) << "Local package found: " << fIt.name() << endl;
			try {
				JSON::Value root;
				JSON::loadFile(root, fIt.path().toString());
				LocalPackage* package = new LocalPackage(root);
				if (!package->valid()) {
					delete package;
					throw Exception("The local package is invalid");
				}
				
				LogDebug("PackageManager", this) << "Local package added: " << package->name() << endl;
				localPackages().add(package->id(), package);
			}
			catch (Exception& exc) {
				LogError("PackageManager", this) << "Load Error: " << exc.displayText() << endl;
			}
			catch (exception& exc) { // Thrown by JSON parser
				LogError("PackageManager", this) << "Manifest Load Error: " << exc.what() << endl;
			}
		}

		++fIt;
	}
}


bool PackageManager::saveLocalPackages(bool whiny)
{
	LogTrace("PackageManager", this) << "Saving Local Packages" << endl;

	bool res = true;
	LocalPackageMap toSave = localPackages().items();
	for (LocalPackageMap::const_iterator it = toSave.begin(); it != toSave.end(); ++it) {
		if (!saveLocalPackage(static_cast<LocalPackage&>(*it->second), whiny))
			res = false;
	}

	LogTrace("PackageManager", this) << "Saving Local Packages: OK" << endl;

	return res;
}


bool PackageManager::saveLocalPackage(LocalPackage& package, bool whiny)
{
	bool res = false;
	try {
		string path(format("%s/%s.json", options().interDir, package.id()));
		LogDebug("PackageManager", this) << "Saving Local Package: " << package.id() << endl;
		JSON::saveFile(package, path);
		res = true;
	}
	catch (Exception& exc) {
		LogError("PackageManager", this) << "Save Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}
	return res;
}


// ---------------------------------------------------------------------
//	Package Installation Methods
//
InstallTask* PackageManager::installPackage(const string& name, const InstallTask::Options& options, bool whiny)
{	
	LogDebug("PackageManager", this) << "Installing Package: " << name << endl;	

	try 
	{
		// Try to update our remote package list if none exist.
		// TODO: Consider storing a remote package cache file.
		if (remotePackages().empty())
			queryRemotePackages();

		PackagePair pair = getOrCreatePackagePair(name);
		Package::Asset asset = getAssetToInstall(pair, options);

		// Set the version option as the version to install.
		// KLUDGE: Instead of modifying options, we should have 
		// a better way of sending the asset/version to the InstallTask.
		InstallTask::Options opts(options);
		opts.version = asset.version();
	
		LogInfo() << "Installing " << pair.name() << " " << asset.version() << endl;	
		return createInstallTask(pair, opts);
	}
	catch (Exception& exc) 
	{
		LogError("PackageManager", this) << "Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}
	
	return NULL;
}


Package::Asset PackageManager::getAssetToInstall(PackagePair& pair, const InstallTask::Options& options)
{
	bool isInstalledAndVerified =
		pair.local->isInstalled() && 
		pair.local->verifyInstallManifest();
	
	LogDebug("PackageManager", this) << "Getting Best Asset:"
		<< "\n\tName:" << pair.local->name() 
		<< "\n\tVersion:" << pair.local->version() 
		<< "\n\tVersion Lock:" << pair.local->versionLock() 
		<< "\n\tSDK Version Lock:" << pair.local->sdkVersionLock() 
		<< "\n\tVerified:" << isInstalledAndVerified
		<< endl;

	// Return true if the locked version is already installed
	if (!pair.local->versionLock().empty() || !options.version.empty()) {
		string versionLock = options.version.empty() ? pair.local->versionLock() : options.version;
		// TODO: assert valid version?

		// Make sure the version option matches the lock, if both were set
		if (!options.version.empty() && !pair.local->versionLock().empty() && 
			options.version != pair.local->versionLock())
			throw Exception("Invalid version option: Package locked at version: " + pair.local->versionLock());

		// If everything is in order there is nothing to install
		if (isInstalledAndVerified && pair.local->versionLock() == pair.local->version())
			throw Exception("Package already up to date. Locked at " + pair.local->versionLock());

		// Return the locked asset, or thorow
		return pair.remote->assetVersion(versionLock);
	}
	
	// Get the best asset from the locked SDK version, if applicable
	if (!pair.local->sdkVersionLock().empty() || !options.sdkVersion.empty()) {		
				
		// Make sure the version option matches the lock, if both were set
		if (!options.sdkVersion.empty() && !pair.local->sdkVersionLock().empty() && 
			options.sdkVersion != pair.local->sdkVersionLock())
			throw Exception("Invalid SDK version option: Package locked at SDK version: " + pair.local->sdkVersionLock());
		
		// Get the latest asset for SDK version
		Package::Asset sdkAsset = pair.remote->latestSDKAsset(pair.local->sdkVersionLock()); // throw if none
		
		// If everything is in order there is nothing to install
		if (isInstalledAndVerified && pair.local->asset().sdkVersion() == pair.local->sdkVersionLock() &&
			!Util::compareVersion(sdkAsset.version(), pair.local->version()))
			throw Exception("Package already up to date for SDK: " + pair.local->sdkVersionLock());

		return sdkAsset;
	}
	
	// If all else fails return the latest asset!
	Package::Asset latestAsset = pair.remote->latestAsset();
	if (isInstalledAndVerified && !Util::compareVersion(latestAsset.version(), pair.local->version()))
		throw Exception("Package already up to date.");

	return latestAsset;
}


bool PackageManager::installPackages(const StringVec& ids, const InstallTask::Options& options, InstallMonitor* monitor, bool whiny)
{	
	bool res = true;	
	try 
	{
		for (StringVec::const_iterator it = ids.begin(); it != ids.end(); ++it) {
			InstallTask* task = installPackage(*it, options, whiny);
			if (task) {
				if (monitor)
					monitor->addTask(task);
				res = false;
			}
		}
	}
	catch (Exception& exc) 
	{
		assert(whiny);
		exc.rethrow();
	}
	return res;
}


InstallTask* PackageManager::updatePackage(const string& name, const InstallTask::Options& options, bool whiny)
{	
	// An update action is essentially the same as an install
	// action, except we will make sure local package exists 
	// before we continue.
	{
		if (!localPackages().exists(name)) {
			string error("Update Failed: " + name + " is not installed.");
			LogError("PackageManager", this) << "" << error << endl;	
			if (whiny)
				throw Exception(error);
			else
				return NULL;
		}
	}

	return installPackage(name, options, whiny);
}


bool PackageManager::updatePackages(const StringVec& ids, const InstallTask::Options& options, InstallMonitor* monitor, bool whiny)
{	
	// An update action is essentially the same as an install
	// action, except we will make sure local package exists 
	// before we continue.
	{
		for (StringVec::const_iterator it = ids.begin(); it != ids.end(); ++it) {
			if (!localPackages().exists(*it)) {
				string error("Update Failed: " + *it + " is not installed.");
				LogError("PackageManager", this) << error << endl;	
				if (whiny)
					throw Exception(error);
				else
					return NULL;
			}
		}
	}
	
	return installPackages(ids, options, monitor, whiny);
}


bool PackageManager::updateAllPackages(bool whiny)
{
	bool res = true;	
	LocalPackageMap& packages = localPackages().items();
	for (LocalPackageMap::const_iterator it = packages.begin(); it != packages.end(); ++it) {	
		InstallTask::Options options;
		InstallTask* task = updatePackage(it->second->name(), options, whiny);
		if (!task)
			res = false;
	}
	
	return res;
}


bool PackageManager::uninstallPackage(const string& id, bool whiny)
{
	LogDebug("PackageManager", this) << "Uninstalling Package: " << id << endl;	
	
	try 
	{
		LocalPackage* package = localPackages().get(id, true);
		try {	
			// Delete package files from manifest
			// NOTE: If some files fail to delete we still consider the uninstall a success.	
			LocalPackage::Manifest manifest = package->manifest();
			if (!manifest.empty()) {
				for (JSON::ValueIterator it = manifest.root.begin(); it != manifest.root.end(); it++) {
					LogDebug("PackageManager", this) << "Uninstalling file: " << (*it).asString() << endl;	
					try {							
						File file(package->getInstalledFilePath((*it).asString()));
						file.remove();
					}
					catch (Exception& exc) {
						LogError("PackageManager", this) << "Uninstall File Error: " << exc.displayText() << endl;
					}
				}
				manifest.root.clear();
			}		
	
			// Delete package manifest file
			File file(format("%s/%s.json", options().interDir, package->id()));
			file.remove();	
		}
		catch (Exception& exc) {
			LogError("PackageManager", this) << "Uninstall Error: " << exc.displayText() << endl;
			// Swallow and continue...
		}

		// Set the package as Uninstalled
		package->setState("Uninstalled");

		// Notify the outside application
		PackageUninstalled.emit(this, *package);
		
		// Free package reference from memory
		localPackages().remove(package);
		delete package;
	}
	catch (Exception& exc) 
	{
		LogError("PackageManager", this) << "Uninstall Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}

	return true;
}


bool PackageManager::uninstallPackages(const StringVec& ids, bool whiny)
{	
	bool res = true;
	for (StringVec::const_iterator it = ids.begin(); it != ids.end(); ++it) {
		if (!uninstallPackage(*it, whiny))
			res = false;
	}
	return res;
}


InstallTask* PackageManager::createInstallTask(PackagePair& pair, const InstallTask::Options& options) //const string& name, InstallMonitor* monitor)
{	
	// Ensure we only have one task per package
	if (getInstallTask(pair.remote->id()))
		throw Exception(pair.remote->name() + " is already installing.");

	InstallTask* task = new InstallTask(*this, pair.local, pair.remote, options);
	task->Complete += delegate(this, &PackageManager::onPackageInstallComplete, -1); // lowest priority to remove task
	{
		FastMutex::ScopedLock lock(_mutex);
		_tasks.push_back(task);
	}
	TaskAdded.emit(this, *task);
	return task; // must call task->start()
}


bool PackageManager::hasUnfinalizedPackages()
{	
	//FastMutex::ScopedLock lock(_mutex);

	LogDebug("PackageManager", this) << "Checking if Finalization Required" << endl;
	
	bool res = false;	
	//LocalPackageMap toCheck(_localPackages.items());
	LocalPackageMap& packages = localPackages().items();
	for (LocalPackageMap::const_iterator it = packages.begin(); it != packages.end(); ++it) {
		if (it->second->state() == "Installing" && 
			it->second->installState() == "Finalizing") {
			LogDebug("PackageManager", this) << "Finalization required: " << it->second->name() << endl;
			res = true;
		}
	}

	return res;
}


bool PackageManager::finalizeInstallations(bool whiny)
{
	LogDebug("PackageManager", this) << "Finalizing Installations" << endl;
	
	//FastMutex::ScopedLock lock(_mutex);
	
	bool res = true;
	
	LocalPackageMap& packages = localPackages().items();
	for (LocalPackageMap::const_iterator it = packages.begin(); it != packages.end(); ++it) {
		try {
			if (it->second->state() == "Installing" && 
				it->second->installState() == "Finalizing") {
				LogDebug("PackageManager", this) << "Finalizing: " << it->second->name() << endl;

				// Create an install task on the stack - we just
				// have to move some files so no async required.
				InstallTask task(*this, it->second, NULL);
				task.doFinalize();
				
				assert(it->second->state() == "Installed" 
					&& it->second->installState() == "Installed");
				
				PackageComplete.emit(this, *it->second);
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
			LogError("PackageManager", this) << "Finalize Error: " << exc.displayText() << endl;
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
// Task Helper Methods
//
InstallTask* PackageManager::getInstallTask(const string& id) const
{
	FastMutex::ScopedLock lock(_mutex);
	for (InstallTaskList::const_iterator it = _tasks.begin(); it != _tasks.end(); it++) {
		if ((*it)->remote()->id() == id)
			return *it;
	}
	return NULL;
}


InstallTaskList PackageManager::tasks() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _tasks;
}


// ---------------------------------------------------------------------
// Package Helper Methods
//
PackagePair PackageManager::getPackagePair(const string& id, bool whiny) const
{		
	FastMutex::ScopedLock lock(_mutex);
	LocalPackage* local = _localPackages.get(id, false);
	RemotePackage* remote = _remotePackages.get(id, false);

	if (whiny && local && !local->valid())
		throw Exception("The local package is invalid");

	if (whiny && remote && !remote->valid())
		throw Exception("The remote package is invalid");

	return PackagePair(local, remote);
}


PackagePairList PackageManager::getPackagePairs() const
{	
	PackagePairList pairs;
	FastMutex::ScopedLock lock(_mutex);
	LocalPackageMap lpackages = _localPackages.items();
	RemotePackageMap rpackages = _remotePackages.items();
	for (LocalPackageMap::const_iterator lit = lpackages.begin(); lit != lpackages.end(); ++lit) {
		pairs.push_back(PackagePair(lit->second));
	}
	for (RemotePackageMap::const_iterator rit = rpackages.begin(); rit != rpackages.end(); ++rit) {
		bool exists = false;
		for (unsigned i = 0; i < pairs.size(); i++) {
			if (pairs[i].id() == rit->second->id()) {
				pairs[i].remote = rit->second;
				exists = true;
				break;
			}
		}
		if (!exists)
			pairs.push_back(PackagePair(NULL, rit->second));
	}
	return pairs;
}


PackagePair PackageManager::getOrCreatePackagePair(const string& id)
{	
	FastMutex::ScopedLock lock(_mutex);
	RemotePackage* remote = _remotePackages.get(id, true);
	if (remote->assets().empty())
		throw Exception("The remote package has no file assets.");

	if (!remote->latestAsset().valid())
		throw Exception("The remote package has invalid file assets.");

	if (!remote->valid())
		throw Exception("The remote package is invalid.");

	// Get or create the local package description.
	LocalPackage* local = _localPackages.get(id, false);
	if (!local) {
		local = new LocalPackage(*remote);
		_localPackages.add(id, local);
	}
	
	if (!local->valid())
		throw Exception("The local package is invalid.");
	
	return PackagePair(local, remote);
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


/*
bool PackageManager::isUpToDate(PackagePair& pair)
{		
	assert(pair.valid());
	return pair.local && pair.remote && 
		pair.local->isUpToDate(*pair.remote);
}


bool PackageManager::verifyInstallManifest(LocalPackage& package)
{	
	LogDebug("PackageManager", this) << package.name() 
		<< ": Verifying install manifest" << endl;

	// Check file system for each manifest file
	LocalPackage::Manifest manifest = package.manifest();
	for (JSON::ValueIterator it = manifest.root.begin(); it != manifest.root.end(); it++) {		
		string path = package.getInstalledFilePath((*it).asString(), false);
		LogDebug("PackageManager", this) << package.name() 
			<< ": Checking: " << path << endl;
		File file(path);
		if (!file.exists()) {
			LogError("PackageManager", this) << package.name() 
				<< ": Missing package file: " << path << endl;
			return false;
		}
	}
	
	return true;
}
*/
	

// ---------------------------------------------------------------------
// File Helper Methods
//
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
		LogError("PackageManager", this) << "Clear Cache Error: " 
			<< fileName << ": " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
	}

	return false;
}


bool PackageManager::hasCachedFile(Package::Asset& asset)
{
	Path path(options().cacheDir);
	path.makeDirectory();
	path.setFileName(asset.fileName());
	File file(path);
	return file.exists() && file.getSize() == asset.fileSize();
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

	
Path PackageManager::getIntermediatePackageDir(const string& id)
{
	LogDebug("PackageManager", this) << "getIntermediatePackageDir 0" << endl;
		
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
// Event Handlers
//
void PackageManager::onPackageInstallComplete(void* sender)
{
	InstallTask* task = reinterpret_cast<InstallTask*>(sender);
	
	LogTrace("PackageManager", this) << "Package Install Complete: " << task->state().toString() << endl;

	PackageComplete.emit(this, *task->local());

	// Save the local package
	saveLocalPackage(*task->local());

	// Remove the task reference
	{
		FastMutex::ScopedLock lock(_mutex);
		for (InstallTaskList::iterator it = _tasks.begin(); it != _tasks.end(); it++) {
			if (*it == task) {
				_tasks.erase(it);
				break;
			}
		}
	}

	TaskRemoved.emit(this, *task);
}


} } // namespace Scy::Pacman



		/*
		// Check the existing package veracity if one exists.
		// If the package is up to date we have nothing to do 
		// so just return a NULL pointer but do not throw.
		if (pair.local->isInstalled()) {
			if (verifyInstallManifest(*pair.local)) {
				LogDebug("PackageManager", this) 
					<< pair.local->name() << ": Manifest is valid" << endl;			
				if (isUpToDate(pair)) {
					ostringstream ost;
					ost << pair.local->name() << " is already up to date: " 
						<< pair.local->version() //<< " >= " 
						//<< pair.remote->latestAsset().version() 
						<< endl;
					throw Exception(ost.str());
				}
			}
		}
		
		// Verify installation options before we create the task.
		if (!options.version.empty()) {
			if (pair.local->versionLock() != options.version)
				throw Exception("Cannot install: Package locked at version: " + pair.local->versionLock());
			pair.remote->assetVersion(options.version); // throw if none
		}
		if (!options.sdkVersion.empty()) {			
			if (pair.local->versionLock() != options.version)
				throw Exception("Cannot install: Package locked at SDK version: " + pair.local->sdkVersionLock());
			pair.remote->latestSDKAsset(options.sdkVersion); // throw if none
		} << " to " 
			<< pair.remote->latestAsset().version()
		*/



	/*
	InstallMonitor* monitor = new InstallMonitor();
	try 
	{
		bool res = true;
		for (StringVec::const_iterator it = names.begin(); it != names.end(); ++it) {
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
	LogDebug("PackageManager", this) << "Installing Package: " << name << endl;	

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
		// If the package is installed, the manifest is valid
		// and the package is up to date we have nothing to do, 
		// just return as success.
		if (pair.local->isInstalled()) {
			if (verifyInstallManifest(pair.local)) {
				LogDebug() 
					<< pair.local->name() << " manifest is valid" << endl;			
				if (isUpToDate(pair)) {
					LogInfo() 
						<< pair.local->name() << " is already up to date: " 
						<< pair.local->version() << " >= " 
						<< pair.remote->latestAsset().version() << endl;
					//return true;
				}
			}
		}
	
		LogInfo() 
			<< pair.local->name() << " is updating: " 
			<< pair.local->version() << " <= " 
			<< pair.remote->latestAsset().version() << endl;

		InstallTask* task = createInstallTask(pair, options);	
		if (monitor)
			monitor->addTask(task);
	}
	catch (Exception& exc) 
	{
		LogError("PackageManager", this) << "Error: " << exc.displayText() << endl;
		if (whiny)
			exc.rethrow();
		else 
			return false;
	}
	
	return true;
}


bool PackageManager::installPackages(const StringVec& names, InstallMonitor* monitor, const InstallTask::Options& options, bool whiny)
{	
	bool res = true;
	for (StringVec::const_iterator it = names.begin(); it != names.end(); ++it) {
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
			LogError("PackageManager", this) << "" << error << endl;	
			if (whiny)
				throw Exception(error);
			else
				return false;
		}
	}

	return installPackage(name, monitor, options, whiny);
}


bool PackageManager::updatePackages(const StringVec& names, InstallMonitor* monitor, const InstallTask::Options& options, bool whiny)
{
	bool res = true;
	for (StringVec::const_iterator it = names.begin(); it != names.end(); ++it) {
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
					LogError("PackageManager", this) << "Package Load Error: " << result.description() << endl;
				else
					LogError("PackageManager", this) << "Package Load Error: " << package->id() << endl;
				package->print(cout);
				delete package;
			}
			else
			*/
	
			
				//if (!file.exists())
				//	throw NotFoundException(path.toString());
			/*
			for (pugi::json_node node = manifest.first_child(); node; node = node.next_sibling()) {
				LogDebug("PackageManager", this) << "Uninstalling file: " << node.attribute("path").value() << endl;	
				Path path(getInstalledFilePath(node.attribute("path").value()));
				File file(path);
				//if (!file.exists())
				//	throw NotFoundException(path.toString());
				file.remove();
				manifest.remove_child(node);
			}
			*/


	/*
	if (isUpToDate(PackagePair& pair)) {
		return NULL;
	}
	// If the package is already up to date then do nothing.
	//if (isUpToDate(name))
		//LogDebug("PackageManager", this) << "Uninstalling Package: " << name << endl;	
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
		LogDebug("PackageManager", this) << "Finalizing: Found: " << dIt.path().toString() << endl;
		
		// InstallTasks are located inside their respective folders.
		DirectoryIterator fIt(dIt.path());
		DirectoryIterator fEnd;
		while (fIt != fEnd)
		{	
			try
			{
				LogDebug("PackageManager", this) << "Finalizing: Installing: " << fIt.path().toString() << endl;
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
				LogDebug("PackageManager", this) << "Finalizing: Deleted: " << fIt.path().toString() << endl;
			else
				LogError("PackageManager", this) << "Finalizing: Delete Error: " << fIt.path().toString() << endl;
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


bool PackageManager::isUpToDate(const string& name)
{	
	FastMutex::ScopedLock lock(_mutex);
	
	LocalPackage* local = _localPackages.get(name, true);
	RemotePackage* remote = _remotePackages.get(name, true);

	if (!local->valid())
		throw Exception("The local package is invalid");

	if (!remote->valid())
		throw Exception("The remote package is invalid");
		
	if (!local->isUpToDate(remote->latestAsset())) {	
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
		Path path(getInstalledFilePath(node.attribute("path").value()));
		File file(path);
		if (!file.exists()) {
			LogError("PackageManager", this) << "Local package missing file: " << path.toString() << endl;
			return false;
		}
	}

	try 
	{
	}
	catch (Exception& exc) 
	{
		LogError("PackageManager", this) << "Error: " << exc.displayText() << endl;
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