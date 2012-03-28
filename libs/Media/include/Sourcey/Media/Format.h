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


#ifndef SOURCEY_MEDIA_Format_H
#define SOURCEY_MEDIA_Format_H


#include "Sourcey/Media/Codec.h"

#include <iostream>
#include <string>
#include <vector>


namespace Sourcey {
namespace Media {


struct Format 
	/// Defines a media container format which is available through the
	/// Format Registry for encoding/decoding. A format specifies preferred
	/// default values for each codec.
{	
	enum Type 
	{
		None		= 0,	// huh?
		Video		= 1,	// video only
		Audio		= 2,	// audio only
		Multiplex	= 3		// both video & audio
	};	

	enum ID 
	{
		Unknown		= 0,
		Raw			= 1,

		// Video
		MP4			= 2,
		FLV			= 3,
		OGG			= 4,
		AVI			= 5,
		MJPEG		= 6,
		MPNG		= 7,

		// Audio
		M4A			= 20,
		MP3			= 21
	};

	//
	// Base Format Variables
	//
	std::string label;		// The display name of this codec.
	Type type;				// The Format::Type of the current object.
	UInt32 id;				// The format numeric ID. Not a Format::ID type for extensibility.
	
	VideoCodec video;		// The video codec.
	AudioCodec audio;		// The audio codec.

	int priority;			// The priority this format will be displayed on the list.

	//
	// Ctors/Dtors
	//
	Format();

	Format(const std::string& label, UInt32 id, 
		const VideoCodec& video, const AudioCodec& audio, int priority = 0);
		// Multiplex format constructor
	
	Format(const std::string& label, UInt32 id, 
		const VideoCodec& video, int priority = 0);
		// Video only format constructor
	
	Format(const std::string& label, UInt32 id, 
		const AudioCodec& audio, int priority = 0);
		// Audio only format constructor

	Format(const Format& r);

	//
	// Methods
	//	
	virtual std::string name() const;
		// Returns a string representation of the Codec name.
		// The default implementation just calls idString() 
		// on the current ID.

	virtual std::string extension() const;
		// Returns the file extension for this format.
		// The default implementation just transforms the 
		// idString() to lowercase.
	
	virtual std::string toString() const;
	virtual void print(std::ostream& ost);

	static Format::ID toID(const std::string& name);	
	static std::string idString(UInt32 id);
	static bool preferable(const Format& first, const Format& second) {
		return first.priority > second.priority;
	}
};


typedef std::vector<Format> FormatList;
typedef std::vector<Format*> FormatPList;


} // namespace Media
} // namespace Sourcey


#endif