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


#ifndef SOURCEY_MEDIA_VideoContext_H
#define SOURCEY_MEDIA_VideoContext_H


#include "Sourcey/Timer.h"
#include "Sourcey/Media/Types.h"
#include "Sourcey/Media/Format.h"
#include "Sourcey/Media/FPSCounter.h"

//#include "Sourcey/Mutex.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/fifo.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}


namespace scy {
namespace av {


struct VideoConversionContext;

	
AVFrame* createVideoFrame(::PixelFormat pixelFmt, int width, int height);
void initVideoEncoderContext(AVCodecContext* ctx, AVCodec* codec, VideoCodec& oparams);
void initDecodedVideoPacket(const AVStream* stream, const AVCodecContext* ctx, const AVFrame* frame, AVPacket* opacket, double* pts);
void initVideoCodecFromContext(const AVCodecContext* ctx, VideoCodec& params);
AVRational getCodecTimeBase(AVCodec* c, double fps);


//
// Video Context
//


struct VideoContext
	/// Base video context which all encoders and decoders extend
{
	VideoContext();
	virtual ~VideoContext();
		
	virtual void create();
		// Create the AVCodecContext using default values

	virtual void open();
		// Open the AVCodecContext

	virtual void close();	
		// Close the AVCodecContext

	AVStream* stream;		// encoder or decoder stream
	AVCodecContext* ctx;	// encoder or decoder context
	AVCodec* codec;			// encoder or decoder codec
	AVFrame* frame;			// encoded or decoded frame
	FPSCounter fps;			// encoder or decoder fps rate
	//FPSCounter1 fps1;
    double pts;				// pts in decimal seconds
	
	Stopwatch frameDuration;
    std::string error;		// error message
};


//
// Video Encoder Context
//


struct VideoEncoderContext: public VideoContext
{
	VideoEncoderContext(AVFormatContext* format);
	virtual ~VideoEncoderContext();	
	
	virtual void create();
	//virtual void open();
	virtual void close();
	
	virtual bool encode(unsigned char* data, int size, Int64 pts, AVPacket& opacket);
	virtual bool encode(AVPacket& ipacket, AVPacket& opacket);
	virtual bool encode(AVFrame* iframe, AVPacket& opacket);
	virtual bool flush(AVPacket& opacket);
	
	virtual void createConverter();
	virtual void freeConverter();
	
	VideoConversionContext* conv;
	AVFormatContext* format;

    UInt8*	buffer;
    int		bufferSize;

	VideoCodec	iparams;
	VideoCodec	oparams;
};


//
// Video Codec Encoder Context
//


struct VideoCodecEncoderContext: public VideoContext
{
	VideoCodecEncoderContext();
	virtual ~VideoCodecEncoderContext();	
	
	virtual void create();
	//virtual void open(); //const VideoCodec& params
	virtual void close();
	
	virtual bool encode(unsigned char* data, int size, AVPacket& opacket);
	virtual bool encode(AVPacket& ipacket, AVPacket& opacket);
	virtual bool encode(AVFrame* iframe, AVPacket& opacket);
		
	VideoConversionContext* conv;

    UInt8*			buffer;
    int				bufferSize;

	VideoCodec	iparams;
	VideoCodec	oparams;
};


//
// Video Decode Context
//


struct VideoDecoderContext: public VideoContext
{
	VideoDecoderContext();
	virtual ~VideoDecoderContext();
	
	virtual void create(AVFormatContext *ic, int streamID);	
	//virtual void open();
	virtual void close();	
	
	virtual bool decode(UInt8* data, int size, AVPacket& opacket);
	virtual bool decode(AVPacket& ipacket, AVPacket& opacket);
		// Decodes a the given input packet.
		// Returns true an output packet was returned, 
		// false otherwise.
    
	virtual bool flush(AVPacket& opacket);
		// Flushes buffered frames.
		// This method should be called after decoding
		// until false is returned.

	//double maxFPS; 
		// Maximum decoding FPS. 
		// FPS is calculated from ipacket PTS. 
		// Extra frames will be dropped.
};


//
// Video Conversion Context
//


struct VideoConversionContext
{
	VideoConversionContext();
	virtual ~VideoConversionContext();	
	
	virtual void create(const VideoCodec& iparams, const VideoCodec& oparams);
	virtual void free();

	virtual AVFrame* convert(AVFrame* iframe);

	AVFrame* oframe;
	struct SwsContext* ctx;
	VideoCodec iparams;
	VideoCodec oparams;
};


} } // namespace scy::av


#endif	// SOURCEY_MEDIA_VideoContext_H




	//virtual void reset();
	//virtual void reset();	
	
	//AVFrame* oframe;
	//struct SwsContext* convCtx;
/*
void VideoDecoderContext::reset() 
{
	VideoContext::reset();
}
*/
	//virtual void reset();	



	//virtual int encode(unsigned char* buffer, int bufferSize, AVPacket& opacket/*, unsigned pts = AV_NOPTS_VALUE*/);
		// Encodes a video frame from given data buffer and
		// stores it in the opacket.
		// If a pts value is given it will be applied to the
		// encoded video packet.