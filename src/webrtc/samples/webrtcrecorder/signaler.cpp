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

#include "signaler.h"

#include "scy/util.h"
#include "scy/av/format.h"
#include "scy/av/codec.h"

#include <iostream>
#include <string>


#define OUTPUT_FILENAME "webrtcrecorder.mp4"
#define OUTPUT_FORMAT av::Format("MP4", "mp4", \
    av::VideoCodec("H.264", "libx264", 400, 300, 25, 48000, 128000, "yuv420p"), \
    av::AudioCodec("AAC", "libfdk_aac", 2, 44100, 64000, "s16"));


using std::endl;


namespace scy {


Signaler::Signaler(const smpl::Client::Options& options) :
    _client(options)
{
    _client.StateChange += sdelegate(this, &Signaler::onClientStateChange);
    _client.roster().ItemAdded += sdelegate(this, &Signaler::onPeerConnected);
    _client.roster().ItemRemoved += sdelegate(this, &Signaler::onPeerDiconnected);
    _client += smpl::messageDelegate(this, &Signaler::onPeerMessage);
    _client.connect();
}


Signaler::~Signaler()
{
}


void Signaler::sendSDP(PeerConnection* conn, const std::string& type, const std::string& sdp)
{
    assert(type == "offer" || type == "answer");
    smpl::Message m;
    Json::Value desc;
    desc[kSessionDescriptionTypeName] = type;
    desc[kSessionDescriptionSdpName] = sdp;
    m[type] = desc;

    postMessage(m);
}


void Signaler::sendCandidate(PeerConnection* conn, const std::string& mid, int mlineindex, const std::string& sdp)
{
    smpl::Message m;
    Json::Value desc;
    desc[kCandidateSdpMidName] = mid;
    desc[kCandidateSdpMlineIndexName] = mlineindex;
    desc[kCandidateSdpName] = sdp;
    m["candidate"] = desc;

    postMessage(m);
}


void Signaler::onPeerConnected(void*, smpl::Peer& peer)
{
    if (peer.id() == _client.ourID()) return;
    DebugL << "Peer connected: " << peer.id()  << endl;

    if (PeerConnectionManager::exists(peer.id())) {
        DebugL << "Peer already has session: " << peer.id() << endl;
        return;
    }

    auto conn = new PeerConnection(this, peer.id(), PeerConnection::Answer);
    conn->constraints().SetMandatoryReceiveVideo(true);
    conn->constraints().SetMandatoryReceiveAudio(true);
    conn->createConnection();

    PeerConnectionManager::add(peer.id(), conn);
}


void Signaler::onPeerMessage(void*, smpl::Message& m)
{
    DebugL << "Peer message: " << m.from().toString() << endl;

    if (m.isMember("offer")) {
        recvSDP(m.from().id, m["offer"]);
    }
    else if (m.isMember("answer")) {
        assert(0 && "answer not supported");
    }
    else if (m.isMember("candidate")) {
        recvCandidate(m.from().id, m["candidate"]);
    }
    // else assert(0 && "unknown event");
}


void Signaler::onPeerDiconnected(void*, const smpl::Peer& peer)
{
    DebugL << "Peer disconnected" << endl;

    auto conn = get(peer.id());
    if (conn) {
        DebugL << "Closing peer connection: " << peer.id() << endl;
        conn->closeConnection(); // will be deleted via callback
    }
}


void Signaler::onClientStateChange(void* sender, sockio::ClientState& state, const sockio::ClientState& oldState)
{
    DebugL << "Client state changed from " << oldState << " to " << state << endl;

    switch (state.id()) {
    case sockio::ClientState::Connecting:
        break;
    case sockio::ClientState::Connected:
        break;
    case sockio::ClientState::Online:
        break;
    case sockio::ClientState::Error:
        throw std::runtime_error("Cannot connect to Symple server. " \
            "Did you start the demo app and the Symple server is running on port 4500?");
    }
}


void Signaler::onAddRemoteStream(PeerConnection* conn, webrtc::MediaStreamInterface* stream)
{
    // TODO: StreamRecorder should be a member of PeerConnection

    av::EncoderOptions options;
    options.ofile = OUTPUT_FILENAME;
    options.oformat = OUTPUT_FORMAT;

    _recorder.reset(new StreamRecorder(options));

    auto videoTracks = stream->GetVideoTracks();
    if (!videoTracks.empty())
        _recorder->setVideoTrack(videoTracks[0]);

    auto audioTracks = stream->GetAudioTracks();
    if (!audioTracks.empty())
        _recorder->setAudioTrack(audioTracks[0]);
}


void Signaler::onRemoveRemoteStream(PeerConnection* conn, webrtc::MediaStreamInterface* stream)
{
    assert(0 && "free streams");
}


void Signaler::onStable(PeerConnection* conn)
{
}


void Signaler::onClosed(PeerConnection* conn)
{
    _recorder.reset(); // shutdown the recorder
    PeerConnectionManager::onClosed(conn);
}


void Signaler::onFailure(PeerConnection* conn, const std::string& error)
{
    _recorder.reset(); // shutdown the recorder
    PeerConnectionManager::onFailure(conn, error);
}


void Signaler::postMessage(const smpl::Message& m)
{
    _ipc.push(new ipc::Action(std::bind(&Signaler::syncMessage, this, std::placeholders::_1), m.clone()));
}


void Signaler::syncMessage(const ipc::Action& action)
{
    auto m = reinterpret_cast<smpl::Message*>(action.arg);
    _client.send(*m);
    delete m;
}


} // namespace scy
