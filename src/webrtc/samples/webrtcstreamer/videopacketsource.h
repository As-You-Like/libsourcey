//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
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

#ifndef SCY_VideoPacketSource_H
#define SCY_VideoPacketSource_H


#include "scy/av/videocapture.h"

#include "webrtc/media/base/videocapturer.h"


namespace scy {


class VideoPacketSource: public cricket::VideoCapturer
    /// VideoPacketSource implements a simple cricket::VideoCapturer that
    /// gets decoded remote video frames from a local media channel.
    /// It's used as the remote video source's VideoCapturer so that the remote
    /// video can be used as a cricket::VideoCapturer and in that way a remote
    /// video stream can implement the MediaStreamSourceInterface.
{
public:
    VideoPacketSource(av::MediaCapture::Ptr capture); //PacketSignal& emitter
    virtual ~VideoPacketSource();

    // cricket::VideoCapturer implementation.
    virtual cricket::CaptureState Start(
        const cricket::VideoFormat& capture_format);
    virtual void Stop();
    virtual bool IsRunning();
    virtual bool GetPreferredFourccs(std::vector<uint32_t>* fourccs);
    virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
        cricket::VideoFormat* best_format);
    virtual bool IsScreencast() const;

private:
    void onFrameCaptured(void* sender, av::VideoPacket& packet);

    // std::string device;
    av::MediaCapture::Ptr _capture;
    // PacketSignal& _emitter;
};


// class VideoPacketSourceFactory : public cricket::VideoDeviceCapturerFactory {
// public:
//     VideoPacketSourceFactory() {}
//     virtual ~VideoPacketSourceFactory() {}
//
//     virtual cricket::VideoCapturer* Create(const cricket::Device& device) {
//         return new VideoPacketSource(device.name);
//     }
// };


} // namespace scy


#endif
