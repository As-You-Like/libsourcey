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


#ifndef SOURCEY_FileSystem_H
#define SOURCEY_FileSystem_H


#include "Sourcey/Base.h"
#include <string>
#include <vector>


namespace scy {
namespace fs {

extern char* separator;
	// The platform specific path split separator:
	// "/" on unix and '\\' on windows.
	
extern char delimiter;
	// The platform specific path split delimiter:
	// '/' on unix and '\\' on windows.

std::string filename(const std::string& path);
	// Returns the file name and extension part of the given path.

std::string basename(const std::string& path);
	// Returns the file name sans extension.

std::string dirname(const std::string& path);
	// Returns the directory part of the path.

std::string extname(const std::string& path, bool includeDot = false);
	// Returns the file extension part of the path.

bool exists(const std::string& path);
	// Returns true if the file or directory exists.

bool isdir(const std::string& path);
	// Returns true if the directory exists on the system.

void readdir(const std::string& path, std::vector<std::string>& res);
	// Returns a list of all files and folders in the directory. 

void mkdir(const std::string& path, int mode = 0);
	// Creates a directory. 

void mkdirr(const std::string& path, int mode = 0);
	// Creates a directory recursively. 

void rmdir(const std::string& path);
	// Creates a directory. 

void unlink(const std::string& path);
	// Deletes a file. 

void rename(const std::string& path, const std::string& target);
	// Renames or moves the given file to the target path. 

void addsep(std::string& path);
	// Adds the trailing directory separator to the given path string.
	// If the last character is already a separator nothing will be done.

void addnode(std::string& path, const std::string& node);
	// Appends the given node to the path.
	// If the given path has no trailing separator one will be appended.

std::string normalize(const std::string& path);
	// Normalizes a path for the current opearting system. 
	// Currently this function only converts directory separators to native style.
		
std::string transcode(const std::string& path);
	/// Transcodes the path to into windows native format if using windows
	/// and if LibSourcey was compiled with Unicode support (SOURCEY_UNICODE),
	/// otherwise the path string is returned unchanged.

// TODO: Implement more libuv fs_* types

/*
class StatWatcher: public uv::Handle 
{
public:
	StatWatcher();
	virtual ~StatWatcher();

	void start(const std::string& path, uint32_t interval, bool persistent = true);
	void stop();

private:
	static void onCallback(uv_fs_poll_t* handle,
						int status,
						const uv_stat_t* prev,
						const uv_stat_t* curr);
	void stop();
};
*/


} } // namespace scy::fs


#endif




/*
#ifdef _WIN32
  #define ALLOWABLE_DIRECTORY_DELIMITERS "/\\"
  #define DIRECTORY_DELIMITER '\\'
  #define DIRECTORY_DELIMITER_STRING "\\"
#else
  #define ALLOWABLE_DIRECTORY_DELIMITERS "/"
  #define DIRECTORY_DELIMITER '/'
  #define DIRECTORY_DELIMITER_STRING "/"
#endif
  */

	// Note: This method only checks if the path string is a directory, 
	// it does not check the filesystem for actual existence.