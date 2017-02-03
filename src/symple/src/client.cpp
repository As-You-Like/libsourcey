///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:    LGPL-2.1+
//
/// @addtogroup symple
/// @{


#include "scy/symple/client.h"
#include "scy/net/sslsocket.h"
#include "scy/net/tcpsocket.h"


using std::endl;


namespace scy {
namespace smpl {


//
// TCP Client
//


Client* createTCPClient(const Client::Options& options, uv::Loop* loop)
{
    return new Client(std::make_shared<net::TCPSocket>(loop), options); //, loop
}


TCPClient::TCPClient(const Client::Options& options, uv::Loop* loop)
    : Client(std::make_shared<net::TCPSocket>(loop), options) //, loop
{
}


//
// SSL Client
//


Client* createSSLClient(const Client::Options& options, uv::Loop* loop)
{
    return new Client(std::make_shared<net::SSLSocket>(loop), options);
}


SSLClient::SSLClient(const Client::Options& options, uv::Loop* loop)
    : Client(std::make_shared<net::SSLSocket>(loop), options)
{
}


//
// Client implementation
//


Client::Client(const net::Socket::Ptr& socket, const Client::Options& options)
    : sockio::Client(socket)
    , _options(options)
    , _announceStatus(0)
{
    TraceL << "Create" << endl;
}


Client::~Client()
{
    TraceL << "Destroy" << endl;
}


void Client::connect()
{
    TraceL << "Connecting" << endl;

    assert(!_options.host.empty());
    assert(!_options.user.empty());

    // Update the Socket.IO options with local values before connecting
    sockio::Client::options().host = _options.host;
    sockio::Client::options().port = _options.port;
    sockio::Client::options().reconnection = _options.reconnection;
    sockio::Client::options().reconnectAttempts = _options.reconnectAttempts;

    sockio::Client::connect();
}


void Client::close()
{
    TraceL << "Closing" << endl;
    sockio::Client::close();
}


void assertCanSend(Client* client, Message& m)
{
    if (!client->isOnline()) {
        throw std::runtime_error("Cannot send message while offline.");
    }

    assert(client->ourPeer());
    m.setFrom(client->ourPeer()->address());

    if (m.to().id == m.from().id) {
        assert(0);
        throw std::runtime_error("Cannot send message with matching sender and recipient.");
    }

    if (!m.valid()) {
        assert(0);
        throw std::runtime_error("Cannot send invalid message.");
    }

#ifdef _DEBUG
    TraceL << "Sending message: " << json::stringify(m, true) << endl;
#endif
}


int Client::send(Message& m, bool ack)
{
    assertCanSend(this, m);
    return sockio::Client::send(m, ack);
}


sockio::Transaction* Client::createTransaction(Message& message)
{
    sockio::Packet pkt(message, true);
    auto txn = sockio::Client::createTransaction(pkt);
    return txn; //->send();
}


int Client::send(const std::string& data, bool ack)
{
    Message m;
    if (!m.read(data))
        throw std::runtime_error("Cannot send malformed message.");
    return send(m, ack);
}


int Client::respond(Message& m, bool ack)
{
    m.setTo(m.from());
    return send(m, ack);
}


void Client::createPresence(Presence& p)
{
    TraceL << "Create presence" << endl;

    auto peer = ourPeer();
    if (peer) {
        CreatePresence.emit(*peer);
        p["data"] = *peer;
    } else
        assert(0 && "no session");
}


int Client::sendPresence(bool probe)
{
    TraceL << "Broadcasting presence" << endl;

    Presence p;
    createPresence(p);
    p.setProbe(probe);
    return send(p);
}


int Client::sendPresence(const Address& to, bool probe)
{
    TraceL << "Sending presence" << endl;

    Presence p;
    createPresence(p);
    p.setProbe(probe);
    p.setTo(to);
    return send(p);
}


int Client::announce()
{
    TraceL << "Announcing" << endl;

    json::Value data;
    data["user"] = _options.user;
    data["name"] = _options.name;
    data["type"] = _options.type;
    data["token"] = _options.token;
    sockio::Packet pkt("announce", data, true);
    auto txn = sockio::Client::createTransaction(pkt);
    txn->StateChange += slot(this, &Client::onAnnounceState);
    return txn->send();
}


int Client::joinRoom(const std::string& room)
{
    DebugL << "Join room: " << room << endl;

    _rooms.push_back(room);
    sockio::Packet pkt("join", "\"" + room + "\"");
    return sockio::Client::send(pkt);
}


int Client::leaveRoom(const std::string& room)
{
    DebugL << "Leave room: " << room << endl;

    _rooms.erase(std::remove(_rooms.begin(), _rooms.end(), room), _rooms.end());
    sockio::Packet pkt("leave", "\"" + room + "\"");
    return sockio::Client::send(pkt);
}


void Client::onAnnounceState(void* sender, TransactionState& state,
                             const TransactionState&)
{
    TraceL << "On announce response: " << state << endl;

    auto transaction = reinterpret_cast<sockio::Transaction*>(sender);
    switch (state.id()) {
        case TransactionState::Success:
            try {
                json::Value data =
                    transaction->response().json(); //[(unsigned)0];
                _announceStatus = data["status"].asInt();

                if (_announceStatus != 200)
                    throw std::runtime_error(data["message"].asString());

                _ourID = data["data"]["id"].asString(); // Address();
                if (_ourID.empty())
                    throw std::runtime_error("Invalid announce response.");

                // Set our local peer data from the response or fail.
                onPresenceData(data["data"], true);

                // Notify the outside application of the response
                // status before we transition the client state.
                Announce.emit(_announceStatus);

                // Transition to Online state.
                onOnline();
                // setState(this, sockio::ClientState::Online);

                // Broadcast a presence probe to our network.
                sendPresence(true);
            } catch (std::exception& exc) {
                // Set the error message and close the connection.
                setError("Announce failed: " + std::string(exc.what()));
            }
            break;

        case TransactionState::Failed:
            Announce.emit(_announceStatus);
            setError("Announce failed");
            break;
    }
}


void Client::onOnline()
{
    // TraceL << "On online" << endl;

    // NOTE: Do not transition to Online state until the Announce
    // callback is successful
    if (!_announceStatus)
        announce();
    else
        sockio::Client::onOnline();
}


void Client::emit(/*void* sender, */ IPacket& raw)
{
    auto packet = reinterpret_cast<sockio::Packet&>(raw);
    TraceL << "Emit packet: " << packet.toString() << endl;

    // Parse Symple messages from Socket.IO packets
    if (packet.type() == sockio::Packet::Type::Event) {
        TraceL << "JSON packet: " << packet.toString() << endl;

        json::Value data = packet.json();
#ifdef _DEBUG
        TraceL << "Received " << json::stringify(data, true) << endl;
#endif
        assert(data.isObject());
        if (data.isMember("type")) {
            std::string type(data["type"].asString());

            // KLUDGE: Note we are currently creating the JSON object
            // twice with these polymorphic message classes. Perhaps
            // free functions are a better for working with messages.
            if (type == "message") {
                Message m(data);
                if (!m.valid()) {
                    WarnL << "Dropping invalid message: "
                          << json::stringify(data, false) << endl;
                    return;
                }
                PacketSignal::emit(m);
            } else if (type == "event") {
                Event e(data);
                if (!e.valid()) {
                    WarnL << "Dropping invalid event: "
                          << json::stringify(data, false) << endl;
                    return;
                }
                PacketSignal::emit(e);
            } else if (type == "presence") {
                Presence p(data);
                if (!p.valid()) {
                    WarnL << "Dropping invalid presence: "
                          << json::stringify(data, false) << endl;
                    return;
                }
                PacketSignal::emit(p);
                if (p.isMember("data")) {
                    onPresenceData(p["data"], false);
                    if (p.isProbe())
                        sendPresence(p.from());
                }
            } else if (type == "command") {
                Command c(data);
                if (!c.valid()) {
                    WarnL << "Dropping invalid command: "
                          << json::stringify(data, false) << endl;
                    return;
                }
                PacketSignal::emit(c);
                if (c.isRequest()) {
                    c.setStatus(404);
                    WarnL << "Command not handled: " << c.id() << ": "
                          << c.node() << endl;
                    respond(c);
                }
            } else
                WarnL << "Received non-standard message: " << type << endl;
        } else {
            assert(0 && "invalid packet");
            WarnL << "Invalid Symple message" << endl;
        }
    }

    // Other packet types are proxied directly
    else {
        TraceL << "Proxying packet" << endl;
        PacketSignal::emit(packet);
    }
}


void Client::onPresenceData(const json::Value& data, bool whiny)
{
    TraceL << "Updating: " << json::stringify(data, true) << endl;

    if (data.isObject() && data.isMember("id") && data.isMember("user") &&
        data.isMember("name") && data.isMember("online")) {
        std::string id = data["id"].asString();
        bool online = data["online"].asBool();
        auto peer = _roster.get(id, false);
        if (online) {
            if (!peer) {
                peer = new Peer(data);
                _roster.add(id, peer);
                InfoL << "Peer connected: " << peer->address().toString()
                      << endl;
                PeerConnected.emit(*peer);
            } else
                static_cast<json::Value&>(*peer) = data;
        } else {
            if (peer) {
                InfoL << "Peer disconnected: " << peer->address().toString()
                      << endl;
                PeerDisconnected.emit(*peer);
                _roster.free(id);
            } else {
                WarnL << "Got peer disconnected for unknown peer: " << id
                      << endl;
            }
        }
    } else {
        std::string error("Bad presence data: " + json::stringify(data));
        ErrorL << error << endl;
        if (whiny)
            throw std::runtime_error(error);
    }

#if 0
    if (data.isObject() &&
        data.isMember("id") &&
        data.isMember("user") &&
        data.isMember("name") //&&
        //data.isMember("type")
        ) {
        TraceL << "Updating: " << json::stringify(data, true) << endl;
        std::string id = data["id"].asString();
        Peer* peer = get(id, false);
        if (!peer) {
            peer = new Peer(data);
            add(id, peer);
        } else
            static_cast<json::Value&>(*peer) = data;
    }
    else if (data.isArray()) {
        for (auto it = data.begin(); it != data.end(); it++) {
            onPresenceData(*it, whiny);
        }
    }
    else {
        std::string error("Bad presence data: " + json::stringify(data));
        ErrorL << error << endl;
        if (whiny)
            throw std::runtime_error(error);
    }
#endif
}


void Client::reset()
{
    // Note: Not clearing persisted messages just in case
    // they are still in use by the outside application
    //_persistence.clear();

    _roster.clear();
    _rooms.clear();
    _announceStatus = 0;
    _ourID = "";
    sockio::Client::reset();
}


Roster& Client::roster()
{

    return _roster;
}


PersistenceT& Client::persistence()
{
    return _persistence;
}


Client::Options& Client::options()
{
    return _options;
}


std::string Client::ourID() const
{
    return _ourID;
}


StringVec Client::rooms() const
{
    return _rooms;
}


Peer* Client::ourPeer()
{
    if (_ourID.empty())
        return nullptr;
    // throw std::runtime_error("No active peer session is available.");
    return _roster.get(_ourID, false);
}


Client& Client::operator>>(Message& message)
{
    send(message);
    return *this;
}


} // namespace smpl
} // namespace scy


/// @\}
