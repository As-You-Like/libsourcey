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


#ifndef SOURCEY_MEDIA_CaptureEncoder_H
#define SOURCEY_MEDIA_CaptureEncoder_H


#include "Sourcey/Media/Types.h"
#include "Sourcey/Media/ICapture.h"
#include "Sourcey/Media/IEncoder.h"
#include "Sourcey/Media/AudioEncoder.h"
#include "Sourcey/Media/VideoEncoder.h"
#include "Sourcey/IStartable.h"
#include "Sourcey/Logger.h"

#include "Poco/DateTimeFormatter.h"
#include "Poco/Timestamp.h"


namespace Scy {
namespace Media {


template <class EncoderT>
class CaptureEncoder: public IStartable, public EncoderT
	/// This class extends a Encoder object for encoding the output
	/// of an ICapture object.
	///
	/// EncoderT should be a VideoEncoder or AudioEncoder.
	///
	/// NOTE: This class is depreciated.
{
public:
	CaptureEncoder(ICapture* capture, const EncoderOptions& options, bool destroyOnStop = false) : 
		EncoderT(params),
		_capture(capture),
		_destroyOnStop(destroyOnStop)
	{
		LogDebug() << "[CaptureEncoder] Creating" << std::endl;
		assert(_capture);	
	}

	virtual ~CaptureEncoder() 
	{
		LogDebug() << "[CaptureEncoder] Destroying" << std::endl;

		// A call to stop() is required before destruction.
		assert(EncoderT::state().id() == EncoderState::None);
	}

	virtual void start(/*const ParamT& params*/) 
	{
		LogDebug() << "[CaptureEncoder] Starting..." << std::endl;
		if (!EncoderT::isReady())
			EncoderT::initialize();

		try {
			assert(_capture);
			
			EncoderT::setState(this, EncoderState::Encoding);

			_capture->attach(polymorphicDelegate(this, &CaptureEncoder::onCapture));
			//_capture->start(Callback<CaptureEncoder, const MediaPacket, false>(this, &CaptureEncoder::onCapture));
			
		} 
		catch (Exception& exc) {
			LogError() << "Encoder Error: " << exc.displayText() << std::endl;
			EncoderT::setState(this, EncoderState::Error);
			stop();
			exc.rethrow();
		}
	}

	virtual void stop() 
	{
		LogDebug() << "[CaptureEncoder] Stopping..." << std::endl;
		try {
			assert(_capture);
			//assert(EncoderT::isReady());

			_capture->detach(polymorphicDelegate(this, &CaptureEncoder::onCapture));
			//_capture->stop(Callback<CaptureEncoder, const MediaPacket, false>(this, &CaptureEncoder::onCapture));
			
			EncoderT::setState(this, EncoderState::None);
		} 
		catch (Exception& exc) {
			LogError() << "Encoder Error: " << exc.displayText() << std::endl;
			EncoderT::setState(this, EncoderState::Error);
		}

		if (_destroyOnStop)
			delete this;
	}
	
	virtual void onCapture(void*, MediaPacket& packet)
	{
		try {
			if (!EncoderT::isEncoding())
				throw Exception("The encoder is not initialized.");

			int size = EncoderT::encode((unsigned char*)packet.data, packet.size);
		} 
		catch (Exception& exc) {
			LogError() << "Encoder Error: " << exc.displayText() << std::endl;
			EncoderT::setState(this, EncoderState::Error);
			stop();
		}
	}

	// Connect to PacketEncoded signal to receive encoded packets.

private:
	EncoderT	_encoder; 
	ICapture*	_capture;
	bool		_destroyOnStop;
};


/*
class RawCaptureEncoder: public IEncoder
	/// This class extends a Encoder object and encodes the output
	/// of a single Capture object.
	///
	/// This is a convenience class for Provider's which want
	// to receive raw device output without encoding.
{
public:
	RawCaptureEncoder(ICapture* capture, bool destroyOnStop = false) : 
		_capture(capture),
		_destroyOnStop(destroyOnStop)
	{
		LogDebug() << "[RawCaptureEncoder] Creating" << std::endl;
		assert(_capture);	
		EncoderT::setState(this, EncoderState::None);
	}


	virtual ~RawCaptureEncoder() 
	{
		LogDebug() << "[RawCaptureEncoder] Destroying" << std::endl;

		// A call to stop() is required before destruction.
		assert(state().id() == EncoderState::None);
	}

	virtual void start() 
	{
		LogDebug() << "[RawCaptureEncoder] Starting..." << std::endl;
		try {
			assert(_capture);
			//assert(isInitialized());
			
			_capture->start(Callback<RawCaptureEncoder, const MediaPacket, false>(this, &RawCaptureEncoder::onCapture));
			
			EncoderT::setState(this, EncoderState::Encoding);
		} 
		catch (Exception& exc) {
			LogError() << "Encoder Error: " << exc.displayText() << std::endl;
			EncoderT::setState(this, EncoderState::Error);
			stop();
			exc.rethrow();
		}
	}

	virtual void stop() 
	{
		LogDebug() << "[RawCaptureEncoder] Stopping..." << std::endl;
		try {
			assert(_capture);
			//assert(isInitialized());

			_capture->stop(Callback<RawCaptureEncoder, const MediaPacket, false>(this, &RawCaptureEncoder::onCapture));

			EncoderT::setState(this, EncoderState::None);
		} 
		catch (Exception& exc) {
			LogError() << "Encoder Error: " << exc.displayText() << std::endl;
			EncoderT::setState(this, EncoderState::Error);
		}

		if (_destroyOnStop)
			delete this;
	}
	
	virtual void onCapture(const MediaPacket& packet)
	{		
		if (!isEncoding())
			throw Exception("The encoder is not initilaized.");

		// No encoding takes place, just relay the packet.
		//PacketEncoded.emit(this, static_cast<Packet&>(packet));

		PacketEncoded.emit(this, packet);
	}

	// Connect to PacketEncoded signal to receive encoded packets.

private:
	ICapture*	_capture;
	bool		_destroyOnStop;
};
*/


} } // namespace Scy::Media


#endif // SOURCEY_MEDIA_CaptureEncoder_H