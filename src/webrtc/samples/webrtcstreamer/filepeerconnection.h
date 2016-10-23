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

#ifndef SCY_FilePeerConnection_H
#define SCY_FilePeerConnection_H


#include "audiopacketmodule.h"

#include "scy/av/mediacapture.h"
#include "scy/webrtc/peerconnection.h"

#include "webrtc/modules/audio_device/include/audio_device.h"
// #include "webrtc/api/peerconnectioninterface.h"
// #include "webrtc/api/jsep.h"
// #include "webrtc/api/test/fakeconstraints.h"


namespace scy {


// class PeerConnectionManager;


class FilePeerConnection: public PeerConnection
{
public:
    FilePeerConnection(PeerConnectionManager* manager, const std::string& peerid, Mode mode);
    virtual ~FilePeerConnection();

    virtual rtc::scoped_refptr<webrtc::MediaStreamInterface> createMediaStream();
        // Create the local media stream.
        // Only necessary when we are creating the offer.

    virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state);
    // virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state);

protected:
    av::MediaCapture::Ptr _capture;
    rtc::scoped_refptr<AudioPacketModule> _audioCaptureModule;
//
//     virtual void createConnection();
//         // Create the peer connection once configuration, constraints and
//         // streams have been set.
//
//     virtual void closeConnection();
//         // Close the peer connection.
//
//     virtual void createOffer();
//         // Create the offer SDP tos end to the peer.
//         // No offer should be received after creating the offer.
//         // A call to `recvRemoteAnswer` is expected to initiate the session.
//
//     virtual void recvSDP(const std::string& type, const std::string& sdp);
//         // Receive a remote offer or answer.
//
//     virtual void recvCandidate(const std::string& mid, int mlineindex, const std::string& sdp);
//         // Receive a remote candidate.
//
//     void setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory);
//         // Set a custom PeerConnectionFactory object
//         // Must be done before any streams are initiated
//
//     std::string peerid() const;
//     webrtc::FakeConstraints& constraints();
//     webrtc::PeerConnectionFactoryInterface* factory() const;
//     rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection() const;
//     rtc::scoped_refptr<webrtc::MediaStreamInterface> stream() const;
//
// protected:
//
//     // inherited from PeerConnectionObserver
//     virtual void OnAddStream(webrtc::MediaStreamInterface* stream);
//     virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream);
//     virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
//     virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state);
//     virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state);
//     virtual void OnRenegotiationNeeded();
//
//     // inherited from CreateSessionDescriptionObserver
//     virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
//     virtual void OnFailure(const std::string& error);
//
//     virtual int AddRef() const { return 1; }
//     virtual int Release() const { return 0; }
//
// private:
//     PeerConnectionManager* _manager;
//     std::string _peerid;
//     Mode _mode;
//     webrtc::PeerConnectionInterface::RTCConfiguration _config;
//     webrtc::FakeConstraints _constraints;
//     rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _factory;
//     rtc::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
//     rtc::scoped_refptr<webrtc::MediaStreamInterface> _stream;
};


} // namespace scy


#endif
