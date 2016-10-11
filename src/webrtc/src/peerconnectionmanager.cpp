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

#include "scy/webrtc/peerconnectionmanager.h"


using std::endl;


namespace scy {


PeerConnectionManager::PeerConnectionManager() :
    _factory(webrtc::CreatePeerConnectionFactory())
{
}


PeerConnectionManager::~PeerConnectionManager()
{
}


void PeerConnectionManager::sendSDP(PeerConnection* conn, const std::string& type, const std::string& sdp)
{
    assert(0 && "virtual");
}


void PeerConnectionManager::sendCandidate(PeerConnection* conn, const std::string& mid, int mlineindex, const std::string& sdp)
{
    assert(0 && "virtual");
}


void PeerConnectionManager::recvSDP(const std::string& peerid, const json::Value& message)
{
    auto conn = PeerConnectionManager::get(peerid, false);
    if (!conn) {
        assert(0 && "peer mismath");
        return;
    }

    std::string type = message.get(kSessionDescriptionTypeName, "").asString();
    std::string sdp = message.get(kSessionDescriptionSdpName, "").asString();
    if (sdp.empty() || (type != "offer" && type != "answer")) {
        ErrorL << "Received bad sdp: " << type << ": " << sdp << endl;
        assert(0 && "bad sdp");
        return;
    }

    conn->recvSDP(type, sdp);

    DebugL << "Received " << type << ": " << sdp << endl;
}


void PeerConnectionManager::recvCandidate(const std::string& peerid, const json::Value& message)
{
    auto conn = PeerConnectionManager::get(peerid, false);
    if (!conn) {
        assert(0 && "peer mismath");
        return;
    }

    std::string mid = message.get(kCandidateSdpMidName, "").asString();
    int mlineindex = message.get(kCandidateSdpMlineIndexName, -1).asInt();
    std::string sdp = message.get(kCandidateSdpName, "").asString();
    if (mlineindex == -1 || mid.empty() || sdp.empty()) {
        ErrorL << "Invalid candidate format" << endl;
        assert(0 && "bad candiate");
        return;
    }

    DebugL << "Received candidate: " << sdp << endl;

    conn->recvCandidate(mid, mlineindex, sdp);
}


void PeerConnectionManager::onAddRemoteStream(PeerConnection* conn, webrtc::MediaStreamInterface* stream)
{
    assert(0 && "virtual");
}


void PeerConnectionManager::onRemoveRemoteStream(PeerConnection* conn, webrtc::MediaStreamInterface* stream)
{
    assert(0 && "virtual");
}


void PeerConnectionManager::onStable(PeerConnection* conn)
{
}


void PeerConnectionManager::onClosed(PeerConnection* conn)
{
    DebugL << "Deleting peer connection: " << conn->peerid() << endl;

    if (remove(conn))
        deleteLater<PeerConnection>(conn); // async delete
}


void PeerConnectionManager::onFailure(PeerConnection* conn, const std::string& error)
{
    DebugL << "Deleting peer connection: " << conn->peerid() << endl;

    if (remove(conn))
        deleteLater<PeerConnection>(conn); // async delete
}


rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionManager::factory() const
{
    return _factory;
}


} // namespace scy
