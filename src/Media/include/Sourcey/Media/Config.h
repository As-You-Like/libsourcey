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


#ifndef SOURCEY_MEDIA_Config_H
#define SOURCEY_MEDIA_Config_H


#define MAX_VIDEO_PACKET_SIZE		(3 * 1024 * 1024) 
#define MAX_AUDIO_PACKET_SIZE		(128 * 1500)			// 1 second of 48khz 32bit audio

#define DEFAULT_VIDEO_SAMPLE_RATE	(384000)				// 128 � 384 kbit/s � business-oriented videoconferencing quality using video compression
#define DEFAULT_VIDEO_BIT_RATE		(9000)					// Default value for RTP	

#define DEFAULT_AUDIO_SAMPLE_RATE	(44100)
#define DEFAULT_AUDIO_CHANNELS		(2)
#define DEFAULT_AUDIO_BIT_RATE		(64000)

#define MAX_ENCODE_DURATION			(3 * 60 * 60 * 1000)	// 3 hours


//#ifndef INT64_C
#if defined(_WIN32) && !defined(INT64_C) && !defined(__MINGW32__)
#include "inttypes.h"
#endif

/* Defined in stdint.h
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
*/
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#endif
#ifndef UINT64_C
#define UINT64_C(c) (c ## ULL)
#endif



#endif // SOURCEY_MEDIA_Config_H