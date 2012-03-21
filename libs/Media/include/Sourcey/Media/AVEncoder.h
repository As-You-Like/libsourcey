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


#ifndef SOURCEY_MEDIA_AVEncoder_H
#define SOURCEY_MEDIA_AVEncoder_H


#include "Sourcey/PacketStream.h"
#include "Sourcey/Media/Types.h"
#include "Sourcey/Media/IEncoder.h"
#include "Sourcey/Media/FPSCounter.h"
#include "Sourcey/Media/VideoContext.h"
#include "Sourcey/Media/AudioContext.h"

#include "Poco/Mutex.h"
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/fifo.h>
#include <libswscale/swscale.h>
}


namespace Sourcey {
namespace Media {


class AVEncoder: public IPacketEncoder
	/// This class implements an Audio/Video encoder and writer
	/// which  depends on libavcodec/libavformat.
{
public:
	AVEncoder(const RecorderParams& params);
	AVEncoder();
	virtual ~AVEncoder();

	virtual void initialize();
	virtual void uninitialize();
	virtual void cleanup();

	virtual void process(IPacket& packet);

	virtual bool encodeAudio(unsigned char* buffer, int bufferSize);
	virtual bool encodeVideo(unsigned char* buffer, int bufferSize, int width, int height);

	void setParams(const RecorderParams& params) { _params = params; };
	RecorderParams& params() { return _params; };

protected:			
	virtual void onStreamStateChange(const PacketStreamState& state);
	
protected:
	static Poco::FastMutex _mutex; // Protects avcodec_open/close()

	RecorderParams	_params;	
	FPSCounter		_fpsCounter;
	AVFormatContext* _formatCtx;
	ByteIOContext*	_outIO;
	unsigned char*  _outIOBuffer; 
	int				_outIOBufferSize; 

	//
 	// Video
	//
	VideoEncoderContext* _video;

 	// The following variables allow for dynamically
	// calculated video presentation timestamps (PTS).
	double			_videoPTS;
	Int64			_videoLastPTS;
	clock_t			_videoTime;

	//
 	// Audio
	//
	AudioEncoderContext* _audio;
	clock_t			_audioTime;	
};


} } // namespace Sourcey::Media


#endif	// SOURCEY_MEDIA_AVEncoder_H

