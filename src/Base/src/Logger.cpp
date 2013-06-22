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


#include "Sourcey/Logger.h"
#include "Poco/Format.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include <assert.h>


using namespace Poco;
using namespace std;


namespace scy {


// ---------------------------------------------------------------------
// Logger
//
Logger*		Logger::_instance;
FastMutex	Logger::_mutex;


Logger& Logger::instance() 
{
	if (_instance == NULL) 
	{
		FastMutex::ScopedLock lock(_mutex);
		if (_instance == NULL)
			_instance = new Logger;
	}
	return *_instance;
} 


void Logger::initialize() 
{
	instance();
}


void Logger::uninitialize() 
{
	FastMutex::ScopedLock lock(_mutex);
	if (_instance) {
		delete _instance;
		_instance = NULL;
	}
}


void Logger::setInstance(Logger* logger)
{
	uninitialize();
	FastMutex::ScopedLock lock(_mutex);
	_instance = logger;
}


Logger::Logger() :
	_defaultChannel(NULL)
{
	//cout << "[Logger:" << this << "] Creating" << endl;
}


Logger::~Logger()
{	
	LogDebug("Logger", this) << "Destroying" << endl;
	{
		FastMutex::ScopedLock lock(_mutex);
		Util::ClearMap(_map);
		_defaultChannel = NULL;
	}
}


void Logger::add(LogChannel* channel) 
{
	LogDebug("Logger", this) << "Adding Channel: " << channel->name() << endl;
	FastMutex::ScopedLock lock(_mutex);
	// The first channel that is added will be our default channel.
	if (_defaultChannel == NULL)
		_defaultChannel = channel;
	_map[channel->name()] = channel;
}


void Logger::remove(const string& name, bool deletePointer) 
{
	LogDebug("Logger", this) << "Removing Channel: " << name << endl;
	FastMutex::ScopedLock lock(_mutex);
	LogMap::iterator it = _map.find(name);	
	assert(it != _map.end());
	if (it != _map.end()) {
		if (_defaultChannel == it->second)
			_defaultChannel = NULL;
		if (deletePointer)
			delete it->second;	
		_map.erase(it);
	}
}


LogChannel* Logger::get(const string& name, bool whiny) const
{
	FastMutex::ScopedLock lock(_mutex);
	LogMap::const_iterator it = _map.find(name);	
	if (it != _map.end())
		return it->second;
	if (whiny)
		throw Poco::NotFoundException("No log channel named: " + name);
	return NULL;
}


void Logger::setDefault(const string& name)
{
	LogDebug("Logger", this) << "Set Default Channel: " << name << endl;
	_defaultChannel = get(name, true);
}


LogChannel* Logger::getDefault() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _defaultChannel;
}


void Logger::write(const LogStream& stream)
{	
	// will write to the null channel if no default channel exists
	LogChannel* c = getDefault();
	if (c)
		c->write(stream);
}


void Logger::write(const char* channel, const LogStream& stream)
{	
	// will throw if the specified channel doesn't exist
	LogChannel* c = get(channel);
	if (c)
		c->write(stream);
}


void Logger::write(const string& message, const char* level, 
		const string& realm, const void* ptr) const
{
	// will write to the null channel if no default channel exists
	LogChannel* c = getDefault();
	if (c)
		c->write(message, getLogLevelFromString(level), realm, ptr);
}

	
LogStream Logger::send(const char* level, const string& realm, const void* ptr) const
{
	return LogStream(getLogLevelFromString(level), realm, ptr);
}


// ---------------------------------------------------------------------
// Log Stream
//
LogStream::LogStream(LogLevel level, const string& realm, const void* ptr) : 
	level(level), realm(realm), pid(ptr ? Util::getPID(ptr) : "") 
{
}


LogStream::LogStream(LogLevel level, const string& realm, const string& pid) :
	level(level), realm(realm), pid(pid)
{
}

	
// ---------------------------------------------------------------------
// Log Channel
//
LogChannel::LogChannel(const string& name, LogLevel level, const char* dateFormat) : 
	_name(name), 
	_level(level), 
	_dateFormat(dateFormat)
{
}


void LogChannel::write(const string& message, LogLevel level, const string& realm, const void* ptr) 
{	
	LogStream stream(level, realm, ptr);
	stream << message;
	write(stream);
}


void LogChannel::write(const LogStream& stream)
{
	(void)stream;
}


void LogChannel::format(const LogStream& stream, ostream& ost)
{ 
	if (_dateFormat)
		ost << Poco::DateTimeFormatter::format(Poco::Timestamp(), _dateFormat);
	ost << " [" << getStringFromLogLevel(stream.level) << "] ";
	if (!stream.realm.empty() || !stream.pid.empty()) {		
		ost << "[";		
		if (!stream.realm.empty())
			ost << stream.realm;
		if (!stream.pid.empty())
			ost << ":" << stream.pid;
		ost << "] ";
	}
	ost << stream.message.str();
	ost.flush();
}


// ---------------------------------------------------------------------
// Console Channel
//
ConsoleChannel::ConsoleChannel(const string& name, LogLevel level, const char* dateFormat) : 
	LogChannel(name, level, dateFormat) 
{
}


void ConsoleChannel::write(const LogStream& stream)
{ 	
	if (this->level() > stream.level)
		return;
	
	ostringstream ss;
	format(stream, ss);
#if defined(_CONSOLE) && defined(_DEBUG)
	cout << ss.str();
#endif
#if defined(_MSC_VER) && defined(_DEBUG) 
	string s(ss.str());
	wstring temp(s.length(), L' ');
	copy(s.begin(), s.end(), temp.begin());
	OutputDebugString(temp.data());
#endif
}


// ---------------------------------------------------------------------
// File Channel
//
FileChannel::FileChannel(const string& name,
						 const string& path, 
						 LogLevel level, 
						 const char* dateFormat) : 
	LogChannel(name, level, dateFormat),
	_path(path)
{
}


FileChannel::~FileChannel() 
{
	close();
}


void FileChannel::open() 
{
	// Ensure a path was set
	if (_path.empty())
		throw Exception("Log file path must be set.");
	
	// Create directories if needed
	Path dir(_path);
	dir.setFileName("");
	File(dir).createDirectories();
	
	// Open the file stream
	_fstream.close();
	_fstream.open(_path.data(), ios::out | ios::app);	

	// Throw on failure
	if (!_fstream.is_open())
		throw Exception("Failed to open log file: " + _path);
}


void FileChannel::close() 
{ 
	_fstream.close();
}


void FileChannel::write(const LogStream& stream)
{	
	if (this->level() > stream.level)
		return;
	
	if (!_fstream.is_open())	
		open();
	
	ostringstream ss;
	format(stream, ss);
	_fstream << ss.str();
	_fstream.flush();

#if defined(_CONSOLE) && defined(_DEBUG)
	cout << ss.str();
#endif
#if defined(_MSC_VER) && defined(_DEBUG) 
	string s(ss.str());
	wstring temp(s.length(), L' ');
	copy(s.begin(), s.end(), temp.begin());
	OutputDebugString(temp.data());
#endif
}


void FileChannel::setPath(const string& path) 
{ 
	_path = path; 
	open();
}


string FileChannel::path() const 
{ 
	return _path;
}


// ---------------------------------------------------------------------
// Rotating File Channel
//
RotatingFileChannel::RotatingFileChannel(const string& name,
	                                     const string& dir, 
										 LogLevel level, 
										 const string& extension, 
										 int rotationInterval, 
										 const char* dateFormat) : 
	LogChannel(name, level, dateFormat),
	_fstream(NULL),
	_dir(dir),
	_extension(extension),
	_rotationInterval(rotationInterval),
	_rotatedAt(0)
{
	// The initial log file will be opened on the first call to rotate()
}
	

RotatingFileChannel::~RotatingFileChannel() 
{
	if (_fstream) {
		_fstream->close();
		delete _fstream;	
	}
}


void RotatingFileChannel::write(const LogStream& stream)
{	
	if (this->level() > stream.level)
		return;

	if (_fstream == NULL || time(0) - _rotatedAt > _rotationInterval)	
		rotate();
	
	ostringstream ss;
	format(stream, ss);
	*_fstream << ss.str();
	_fstream->flush();
	
#if defined(_CONSOLE) // && defined(_DEBUG)
	cout << ss.str();
#endif
#if defined(_MSC_VER) // && defined(_DEBUG) 
	string s(ss.str());
	wstring temp(s.length(), L' ');
	copy(s.begin(), s.end(), temp.begin());
	OutputDebugString(temp.data());
#endif
}


void RotatingFileChannel::rotate() 
{
	if (_fstream) {
		_fstream->close();
		delete _fstream;
	}

	// Open the next log file
	_filename = Poco::format("%s_%ld.%s", _name, static_cast<long>(Timestamp().epochTime()), _extension);
	Path path(_dir);	
	File(path).createDirectories();
	path.setFileName(_filename);
	_fstream = new ofstream(path.toString().data());	
	_rotatedAt = time(0);
}


} // namespace scy


/*
// ---------------------------------------------------------------------
// Evented File Channel
//
EventedFileChannel::EventedFileChannel(const string& name,
						 const string& dir, 
						 LogLevel level, 
						 const string& extension, 
						 int rotationInterval, 
						 const char* dateFormat) : 
	FileChannel(name, dir, level, extension, rotationInterval, dateFormat)
{
}


EventedFileChannel::~EventedFileChannel() 
{
}


void EventedFileChannel::write(const LogStream& stream, LogLevel level, const string& realm, const void* ptr) 
{	
	if (this->level() > level)
		return;

	FileChannel::write(message, level, ptr);	
	OnLogStream.emit(this, message, level, ptr);
}
*/