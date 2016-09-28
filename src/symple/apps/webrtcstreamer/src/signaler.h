#ifndef SCY_Signaler_H
#define SCY_Signaler_H


#include "scy/webrtc/peerconnectionmanager.h"
#include "scy/application.h"
#include "scy/symple/client.h"
#include "scy/net/sslsocket.h"
#include "scy/net/sslmanager.h"
#include "scy/ipc.h"
#include "scy/util.h"


namespace scy {


class Signaler:
    public PeerConnectionManager,
    public Application
{
public:
    Signaler(const smpl::Client::Options& options);
    ~Signaler();

    // PeerConnectionManager interface
    virtual void sendSDP(const std::string& peerid, const std::string& type, const std::string& sdp);
    virtual void sendCandidate(const std::string& peerid, const std::string& mid, int mlineindex, const std::string& sdp);
    virtual void onAddRemoteStream(const std::string& peerid, webrtc::MediaStreamInterface* stream);
    virtual void onRemoveRemoteStream(const std::string& peerid, webrtc::MediaStreamInterface* stream);

  protected:
    void postMessage(const smpl::Message& m);
    void syncMessage(const ipc::Action& action);

    void onPeerConnected(void*, smpl::Peer& peer);
    void onPeerMessage(void*, smpl::Message& m);
    void onPeerDiconnected(void*, const smpl::Peer& peer);

    void onClientStateChange(void* sender, sockio::ClientState& state, const sockio::ClientState& oldState);

protected:
    ipc::SyncQueue<> _ipc;
#if USE_SSL
    smpl::SSLClient _client;
#else
    smpl::TCPClient _client;
#endif
};


} // namespace scy


#endif
