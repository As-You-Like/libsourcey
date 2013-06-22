#ifndef SOURCEY_MEDIA_AVInputReader_H
#define SOURCEY_MEDIA_AVInputReader_H


#include "Sourcey/PacketEmitter.h"
#include "Sourcey/IStartable.h"
#include "Sourcey/Media/Types.h"
#include "Sourcey/Media/VideoContext.h"
#include "Sourcey/Media/AudioContext.h"

#include "Poco/Runnable.h"
#include "Poco/Mutex.h"


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef LIBAVDEVICE_VERSION
#include <libavdevice/avdevice.h>
#endif
}


namespace Scy {
namespace Media {


class AVInputReader: public PacketEmitter, public IStartable, public Poco::Runnable
	/// Video capture and file input decoder class with reusable
	/// code that depends on ffmpeg libavcodec/libavformat.
{
public:		
	struct Options 
	{			
		bool disableVideo;
		bool disableAudio;
		bool iFramesOnly;			// Process i-frames only (does not restrict audio)
		int processVideoXFrame;		// Process every Xth video frame
		int processAudioXFrame;		// Process every Xth audio frame
		double processVideoXSecs;	// Process video frame every X seconds
		double processAudioXSecs;	// Process audio frame every X seconds

		// Device Input
		std::string deviceEngine;	// The device capture engine"
									//		Windows: vfwcap, dshow
									//		Linux: v4l(video4linux2 or video4linux), dv1394
		std::string deviceStandard; // Linux only: 'pal', 'secam' or 'ntsc'.

		Options() {
			disableVideo = false;
			disableAudio = false;
			iFramesOnly = false;
			processVideoXFrame = 0;
			processAudioXFrame = 0;
			processVideoXSecs = 0;
			processAudioXSecs = 0;
						
#ifdef WIN32
			deviceEngine = "vfwcap";
#else
			deviceEngine = "v4l";
			deviceStandard = "ntsc";
#endif
		}
	};

	AVInputReader(const Options& options = Options());
	virtual ~AVInputReader();
	
	virtual void openFile(const std::string& file);
#ifdef LIBAVDEVICE_VERSION
	virtual void openDevice(int deviceID, int width = 0, int height = 0, double framerate = 0); 
	virtual void openDevice(const std::string& device, int width = 0, int height = 0, double framerate = 0);
#endif
	virtual void openStream(const char* filename, AVInputFormat* inputFormat, AVDictionary** formatParams);
	virtual void close();
	
	virtual void start();
	virtual void stop();

	virtual void run();
		
	virtual Options& options();
	virtual AVFormatContext* formatCtx() const;
	virtual VideoDecoderContext* video() const;
	virtual AudioDecoderContext* audio() const;
	virtual std::string error() const;

	NullSignal ReadComplete;

protected:		
	mutable Poco::FastMutex	_mutex;
	Poco::Thread			_thread;
	bool					_stopping;
	//std::string				_ifile;	
	std::string				_error;
	AVFormatContext*		_formatCtx;	
	VideoDecoderContext*	_video;
	AudioDecoderContext*	_audio;
	Options					_options;
};


} } // namespace Scy::Media


#endif	// SOURCEY_MEDIA_AVInputReader_H

