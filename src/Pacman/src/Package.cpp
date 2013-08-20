//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
// LibSourcey is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// LibSourcey is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//


#include "Sourcey/Pacman/Package.h"
#include "Sourcey/Util.h"
#include "Sourcey/Logger.h"
#include "Sourcey/Filesystem.h"

//#include "Poco/Format.h" // depreciated
////#include "Poco/File.h" // depreciated

#include "assert.h"


using namespace std;



namespace scy { 
namespace pman {


// ---------------------------------------------------------------------
// Base Package
//	
Package::Package()
{
}


Package::Package(const json::Value& src) :
	json::Value(src)
{
}


Package::~Package()
{
}


bool Package::valid() const
{
	return !id().empty()
		&& !name().empty()
		&& !type().empty();
}


string Package::id() const
{
	return (*this)["id"].asString();
}


string Package::type() const
{
	return (*this)["type"].asString();
}


string Package::name() const
{
	return (*this)["name"].asString();
}


string Package::author() const
{
	return (*this)["author"].asString();
}


string Package::description() const
{
	return (*this)["description"].asString();
}


void Package::print(ostream& ost) const
{
	json::StyledWriter writer;
	ost << writer.write(*this);
}


// ---------------------------------------------------------------------
// Package Asset
//	
Package::Asset::Asset(json::Value& src) :
	root(src)
{
}


Package::Asset::~Asset()
{
}


string Package::Asset::fileName() const
{
	return root["file-name"].asString();
}


string Package::Asset::version() const
{
	return root.get("version", "0.0.0").asString();
}


string Package::Asset::sdkVersion() const
{
	return root.get("sdk-version", "0.0.0").asString();
}


string Package::Asset::url(int index) const
{
	return root["mirrors"][(size_t)index]["url"].asString();
}


int Package::Asset::fileSize() const
{
	return root.get("file-size", 0).asInt();
}


bool Package::Asset::valid() const
{
	return root.isMember("file-name")
		&& root.isMember("version")
		&& root.isMember("mirrors");
}


void Package::Asset::print(ostream& ost) const
{
	json::StyledWriter writer;
	ost << writer.write(root);
}


Package::Asset& Package::Asset::operator = (const Asset& r)
{
	root = r.root;
	return *this;
}


bool Package::Asset::operator == (const Asset& r) const
{
	return fileName() == r.fileName();
}


// ---------------------------------------------------------------------
// Remote Package
//	
RemotePackage::RemotePackage()
{
}


RemotePackage::RemotePackage(const json::Value& src) :
	Package(src)
{
}


RemotePackage::~RemotePackage()
{
}


json::Value& RemotePackage::assets()
{
	return (*this)["assets"];
}


Package::Asset RemotePackage::latestAsset()
{
	json::Value& assets = this->assets();
	if (assets.empty())
		throw std::runtime_error("Package has no assets");

	// The latest asset may not be in order, so
	// make sure we always return the latest one.
	size_t index = 0;
	if (assets.size() > 1) {
		for (unsigned i = 1; i < assets.size(); i++) {
			if (util::compareVersion(assets[i]["version"].asString(), assets[index]["version"].asString())) {
				index = i;
			}
		}
	}
	
	return Asset(assets[index]);
}


Package::Asset RemotePackage::assetVersion(const std::string& version)
{
	json::Value& assets = this->assets();
	if (assets.empty())
		throw std::runtime_error("Package has no assets");

	size_t index = -1;
	for (unsigned i = 0; i < assets.size(); i++) {
		if (assets[i]["version"].asString() == version) {
			index = i;
			break;
		}
	}

	if (index == -1)
		throw std::runtime_error("No package asset with version " + version);
	
	return Asset(assets[index]);
}


Package::Asset RemotePackage::latestSDKAsset(const std::string& version)
{
	json::Value& assets = this->assets();
	if (assets.empty())
		throw std::runtime_error("Package has no assets");
	
	size_t index = -1;
	for (unsigned i = 0; i < assets.size(); i++) {		
		if (assets[i]["sdk-version"].asString() == version && (
			assets[index]["sdk-version"].asString() != version || 
			util::compareVersion(assets[i]["version"].asString(), assets[index]["version"].asString()))) {
			index = i;
		}
	}
	
	if (index == -1)
		throw std::runtime_error("No package asset with SDK version " + version);

	return Asset(assets[index]);
}


// ---------------------------------------------------------------------
// Local Package
//	
LocalPackage::LocalPackage()
{
}


LocalPackage::LocalPackage(const json::Value& src) :
	Package(src)
{
}


LocalPackage::LocalPackage(const RemotePackage& src) :
	Package(src)
{
	assert(src.valid());	

	// Clear unwanted remote package fields
	removeMember("assets");
	assert(valid());
}


LocalPackage::~LocalPackage()
{
}


Package::Asset LocalPackage::asset()
{
	return Package::Asset((*this)["asset"]);
}


LocalPackage::Manifest LocalPackage::manifest()
{
	return Manifest((*this)["manifest"]);
}


void LocalPackage::setState(const std::string& state)
{
	assert(
		state == "Installing" || 
		state == "Installed" ||
		state == "Failed" ||
		state == "Uninstalled"
	);

	(*this)["state"] = state;
}


void LocalPackage::setInstallState(const std::string& state)
{
	(*this)["install-state"] = state;
}


void LocalPackage::setVersion(const std::string& version)
{
	if (state() != "Installed")
		throw std::runtime_error("Package must be installed before the version is set.");
	
	(*this)["version"] = version;
}


bool LocalPackage::isInstalled() const
{
	return state() == "Installed";
}


bool LocalPackage::isFailed() const
{
	return state() == "Failed";
}


string LocalPackage::state() const
{
	return get("state", "Installing").asString();
}


string LocalPackage::installState() const
{
	return get("install-state", "None").asString();
}


string LocalPackage::installDir() const
{
	return get("install-dir", "").asString();
}


string LocalPackage::getInstalledFilePath(const std::string& fileName, bool whiny)
{
	std::string dir = installDir();
	if (whiny && dir.empty())
		throw std::runtime_error("Package install directory is not set.");
	
	dir += fs::separator;
	dir += fileName;
	return dir;
	//Poco::Path path(dir);
	//path.makeDirectory();
	//path.setFileName(fileName);
	//return path;
}


void LocalPackage::setVersionLock(const std::string& version)
{
	if (version.empty())
		(*this).removeMember("version-lock");
	else
		(*this)["version-lock"] = version;
}


void LocalPackage::setSDKVersionLock(const std::string& version)
{
	if (version.empty())
		(*this).removeMember("sdk-version-lock");
	else
		(*this)["sdk-version-lock"] = version;
}


string LocalPackage::version() const
{
	return get("version", "0.0.0").asString();
}


string LocalPackage::versionLock() const
{
	return get("version-lock", "").asString();
}


string LocalPackage::sdkVersionLock() const
{
	return get("sdk-version-lock", "").asString();
}


bool LocalPackage::verifyInstallManifest()
{	
	debugL("LocalPackage", this) << name() 
		<< ": Verifying install manifest" << endl;

	// Check file system for each manifest file
	LocalPackage::Manifest manifest = this->manifest();
	for (auto it = manifest.root.begin(); it != manifest.root.end(); it++) {		
		std::string path = this->getInstalledFilePath((*it).asString(), false);
		debugL("LocalPackage", this) << name() 
			<< ": Checking: " << path << endl;
		
		//Poco::File file(path);
		//if (!file.exists()) {
		if (!fs::exists(path)) {
			errorL("PackageManager", this) << name() << ": Missing package file: " << path << endl;
			return false;
		}
	}
	
	return true;
}


		/*

		Asset bestAsset = 
		if (bestAsset)

		try {

			// The best SDK version only needs to be newer if the
			// installed asset matches the locked SDK version.
			if (asset().sdkVersion() != sdkVersionLock())
				return false;
		}
		catch (std::exception& exc/Exception& exc/) {
			// Return false, although we won't be able to update
			// at all, since there are no available SDK assets :(
			return false;
		}

bool LocalPackage::isUpToDate(RemotePackage& remote)
{
	// Return true if the locked version is already installed
	if (!versionLock().empty()) {
		return versionLock() == version();
	}
	
	json::Value tmp;
	Package::Asset bestAsset(tmp);

	// Get the best asset from the locked SDK version, if any
	if (!sdkVersionLock().empty()) {
		try {
			bestAsset = remote.latestSDKAsset(sdkVersionLock());

			// The best SDK version only needs to be newer if the
			// installed asset matches the locked SDK version.
			if (asset().sdkVersion() != sdkVersionLock())
				return false;
		}
		catch (std::exception& exc/Exception& exc/) {
			// Return false, although we won't be able to update
			// at all, since there are no available SDK assets :(
			return false;
		}
	}

	// Otherwise get the latest available asset
	else {
		try {
			bestAsset = remote.latestAsset();
		}
		catch (std::exception& exc/Exception& exc/) {
			// Return false, there are no available SDK assets.
			// The remote package is not valid!
			assert(0 && "invalid remote package");
			return false;
		}
	}
	
	// If L is greater than R the function returns true.
	// If L is equal or less than R the function returns false.
	return !util::compareVersion(bestAsset.version(), version());
}
		*/


void LocalPackage::setInstalledAsset(const Package::Asset& installedRemoteAsset)
{
	if (state() != "Installed")
		throw std::runtime_error("Package must be installed before asset can be set.");

	if (!installedRemoteAsset.valid())
		throw std::runtime_error("Remote asset is invalid.");

	(*this)["asset"] = installedRemoteAsset.root;
	setVersion(installedRemoteAsset.version());
}


void LocalPackage::setInstallDir(const std::string& dir)
{
	(*this)["install-dir"] = dir;
}


json::Value& LocalPackage::errors()
{	
	return (*this)["errors"];
}


void LocalPackage::addError(const std::string& message)
{
	errors().append(message);
}


string LocalPackage::lastError()
{
	return errors()[errors().size() - 1].asString();
}


void LocalPackage::clearErrors()
{
	errors().clear();
}
	

bool LocalPackage::valid() const
{
	return Package::valid(); 
}


// ---------------------------------------------------------------------
// Local Package Manifest
//	
LocalPackage::Manifest::Manifest(json::Value& src) :
	root(src)
{
}


LocalPackage::Manifest::~Manifest()
{
}

	
void LocalPackage::Manifest::addFile(const std::string& path)
{
	// Do not allow duplicates
	//if (!find_child_by_(*this)["file", "path", path.c_str()).empty())
	//	return;

	//json::Value node(path);
	root.append(path);
}
	

bool LocalPackage::Manifest::empty() const
{
	return root.empty();
}


//
// Package Pair
//	

PackagePair::PackagePair(LocalPackage* local, RemotePackage* remote) :
	local(local), remote(remote)
{
}
	

string PackagePair::id() const
{
	return local ? local->id() : remote ? remote->id() : "";
}


string PackagePair::name() const
{
	return local ? local->name() : remote ? remote->name() : "";
}


string PackagePair::type() const
{
	return local ? local->type() : remote ? remote->type() : "";
}


string PackagePair::author() const
{
	return local ? local->author() : remote ? remote->author() : "";
}


bool PackagePair::valid() const
{
	// Packages must be valid, and
	// must have at least one package.
	return (!local || local->valid())
		&& (!remote || remote->valid()) 
		&& (local || remote);
}


} } // namespace scy::pman





	//json::Value node = append_child();
	//node.set_name("file");
	//node.append_(*this)["path").set_value(path.c_str());


/*
Package::Asset LocalPackage::getMatchingAsset(const Package::Asset& asset) const
{
	assert(asset.valid());
	return Asset(select_single_node(
		format("//asset[@file-name='%s' and @version='%s']", 
			asset.fileName(), asset.version()).c_str()).node());
}
*/
/*
void LocalPackage::Manifest::addDir(const std::string& path)
{
	// Do not allow duplicates
	//if (!find_child_by_(*this)["dir", "path", path.c_str()).empty())
	//	return;

	//json::Value node = append_child();
	//node.set_name("dir");
	//node.append_(*this)["path").set_value(path.c_str());
	
	json::Value node(path);
	root.append(node);
}
*/