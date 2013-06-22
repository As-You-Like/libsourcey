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

#ifndef SOURCEY_Logger_H
#define SOURCEY_Logger_H


#include "Sourcey/Util.h"
#include "Sourcey/Signal.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/Thread.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

#include <string.h>


namespace scy {


enum LogLevel
{
	TraceLevel		= 0,
	DebugLevel		= 1,
	InfoLevel		= 2,
	WarningLevel	= 3,
	ErrorLevel		= 4,
	FatalLevel		= 5,
};


inline LogLevel getLogLevelFromString(const char* level)
{
    if (strcmp(level, "trace") == 0)
        return TraceLevel;
    if (strcmp(level, "debug") == 0)
        return DebugLevel;
    if (strcmp(level, "info") == 0)
        return InfoLevel;
    if (strcmp(level, "warn") == 0)
        return WarningLevel;
    if (strcmp(level, "error") == 0)
        return ErrorLevel;
    if (strcmp(level, "fatal") == 0)
        return FatalLevel;
    return DebugLevel;
}


inline const char* getStringFromLogLevel(LogLevel level) 
{
	switch(level)
	{
		case TraceLevel:		return "trace";
		case DebugLevel:		return "debug";
		case InfoLevel:			return "info";
		case WarningLevel:		return "warn";
		case ErrorLevel:		return "error";
		case FatalLevel:		return "fatal";
	}
	return "debug";
}


class LogChannel;
struct LogStream;
typedef std::map<std::string, LogChannel*> LogMap;


// ---------------------------------------------------------------------
//
class Logger 
{
public:
	static Logger& instance();
	static void initialize();	
	static void uninitialize();
	
	static void setInstance(Logger* logger);
		// Sets the logger instance and deletes the current instance.

	void add(LogChannel* channel);
	void remove(const std::string& name, bool deletePointer = true);
	LogChannel* get(const std::string& name, bool whiny = true) const;
		// Returns the specified log channel. 
		// Throws an exception if the channel doesn't exist.
	
	void setDefault(const std::string& name);
		// Sets the default log to the specified log channel.

	LogChannel* getDefault() const;
		// Returns the default log channel, or the null channel
		// if no default channel has been set.

	void write(const LogStream& stream);
		// Writes the given message to the default log channel.

	void write(const char* channel, const LogStream& stream);
		// Writes the given message to the given log channel.

	void write(const std::string& message, const char* level = "debug", 
		const std::string& realm = "", const void* ptr = NULL) const;
		// Writes the given message to the default log channel.
	
	LogStream send(const char* level = "debug", 
		const std::string& realm = "", const void* ptr = NULL) const;
		// Sends to the default log using the given class instance.
		// Recommend using write(LogStream&) to avoid copying data.

protected:
	Logger();								// Private so that it can not be called
	Logger(Logger const&) {};				// Copy constructor is protected
	Logger& operator=(Logger const&) {};	// Assignment operator is protected
	~Logger();
	
	LogChannel*	_defaultChannel;
	LogMap      _map;

	static Logger*         _instance;
	static Poco::FastMutex _mutex;
};


// ---------------------------------------------------------------------
//
struct LogStream
{
	LogLevel level;
	std::ostringstream message;
	std::string realm;
	std::string pid;

	LogStream(LogLevel level = DebugLevel, const std::string& realm = "", const void* ptr = NULL);
	LogStream(LogLevel level, const std::string& realm = "", const std::string& pid = "");

	template<typename T>
	LogStream& operator << (const T& data) {
#ifndef DISABLE_LOGGING
		message << data;
#endif
		return *this;
	}

	LogStream& operator << (std::ostream&(*f)(std::ostream&)) {
#ifndef DISABLE_LOGGING		
		message << f;

		// send to default channel
		Logger::instance().write(*this);
#endif
		return *this;
	}
};


//
// Global log stream accessors
//
inline LogStream Log(const char* level = "debug", const char* realm = "", const void* ptr = NULL) 
	{ return LogStream(getLogLevelFromString(level), realm, ptr); }

inline LogStream Log(const char* level, const void* ptr, const char* realm = "") 
	{ return LogStream(getLogLevelFromString(level), realm, ptr); }

inline LogStream LogTrace(const char* realm = "", const void* ptr = NULL) 
	{ return LogStream(TraceLevel, realm, ptr); }

inline LogStream LogDebug(const char* realm = "", const void* ptr = NULL) 
	{ return LogStream(DebugLevel, realm, ptr); }

inline LogStream LogInfo(const char* realm = "", const void* ptr = NULL) 
	{ return LogStream(InfoLevel, realm, ptr); }

inline LogStream LogWarn(const char* realm = "", const void* ptr = NULL) 
	{ return LogStream(WarningLevel, realm, ptr); }

inline LogStream LogError(const char* realm = "", const void* ptr = NULL) 
	{ return LogStream(ErrorLevel, realm, ptr); }

inline LogStream LogFatal(const char* realm = "", const void* ptr = NULL) 
	{ return LogStream(FatalLevel, realm, ptr); }


// ---------------------------------------------------------------------
//
class LogChannel
{
public:	
	LogChannel(const std::string& name, LogLevel level = DebugLevel, const char* dateFormat = "%H:%M:%S");
	virtual ~LogChannel() {}; 
	
	virtual void write(const LogStream& stream);
	virtual void write(const std::string& message, LogLevel level = DebugLevel, 
		const std::string& realm = "", const void* ptr = NULL);
	virtual void format(const LogStream& stream, std::ostream& ost);

	std::string	name() const { return _name; };
	LogLevel level() const { return _level; };
	const char* dateFormat() const { return _dateFormat; };
	
	void setLevel(LogLevel level) { _level = level; };
	void setDateFormat(const char* format) { _dateFormat = format; };

protected:
	std::string _name;
	LogLevel	_level;
	const char*	_dateFormat;
};


// ---------------------------------------------------------------------
//
class ConsoleChannel: public LogChannel
{		
public:
	ConsoleChannel(const std::string& name, LogLevel level = DebugLevel, const char* dateFormat = "%H:%M:%S");
	virtual ~ConsoleChannel() {}; 
		
	virtual void write(const LogStream& stream);
};


// ---------------------------------------------------------------------
//
class FileChannel: public LogChannel
{	
public:
	FileChannel(
		const std::string& name,
		const std::string& path,
		LogLevel level = DebugLevel, 
		const char* dateFormat = "%H:%M:%S");
	virtual ~FileChannel();
	
	virtual void write(const LogStream& stream);
	
	void setPath(const std::string& path);
	std::string	path() const;

protected:
	virtual void open();
	virtual void close();

protected:
	std::ofstream	_fstream;
	std::string		_path;
};


// ---------------------------------------------------------------------
//
class RotatingFileChannel: public LogChannel
{	
public:
	RotatingFileChannel(
		const std::string& name,
		const std::string& dir,
		LogLevel level = DebugLevel, 
		const std::string& extension = "log", 
		int rotationInterval = 12 * 3600, 
		const char* dateFormat = "%H:%M:%S");
	virtual ~RotatingFileChannel();
	
	virtual void write(const LogStream& stream);
	virtual void rotate();

	std::string	dir() const { return _dir; };
	std::string	filename() const { return _filename; };
	int	rotationInterval() const { return _rotationInterval; };
	
	void setDir(const std::string& dir) { _dir = dir; };
	void setExtension(const std::string& ext) { _extension = ext; };
	void setRotationInterval(int interval) { _rotationInterval = interval; };

protected:
	std::ofstream*	_fstream;
	std::string		_dir;
	std::string		_filename;
	std::string		_extension;
	time_t			_rotatedAt;			// The time the log was last rotated
	int				_rotationInterval;	// The log rotation interval in seconds
};


#if 0
class EventedFileChannel: public FileChannel
{	
public:
	EventedFileChannel(
		const std::string& name,
		const std::string& dir,
		LogLevel level = DebugLevel, 
		const std::string& extension = "log", 
		int rotationInterval = 12 * 3600, 
		const char* dateFormat = "%H:%M:%S");
	virtual ~EventedFileChannel();
	
	virtual void write(const std::string& message, LogLevel level = DebugLevel, 
		const std::string& realm = "", const void* ptr = NULL);
	virtual void write(const LogStream& stream);

	Signal3<const std::string&, LogLevel&, const IPolymorphic*&> OnLogStream;
};
#endif


} // namespace scy


#endif