///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup socketio
/// @{


#ifndef SCY_SocketIO_Client_H
#define SCY_SocketIO_Client_H


#include "scy/socketio/packet.h"
#include "scy/socketio/transaction.h"
#include "scy/http/websocket.h"
#include "scy/json/json.h"
#include "scy/collection.h"


namespace scy {
namespace sockio {


struct ClientState: public State
{
    enum Type
    {
        Closed        = 0x00,
        Connecting    = 0x01,
        Connected     = 0x02,
        Online        = 0x04,
        Error         = 0x08
    };

    std::string str(unsigned int id) const
    {
        switch(id) {
        case Closed:          return "Closed";
        case Connecting:      return "Connecting";
        case Connected:       return "Connected";
        case Online:          return "Online";
        case Error:           return "Error";
        default: assert(false);
        }
        return "undefined";
    };
};


class Client:
    public Stateful<ClientState>,
    public net::SocketAdapter,
    public PacketSignal
{
  public:
      struct Options
      {
          std::string host;
          std::uint16_t port;

          /// Weather or not to reconnect if disconnected from the server.
          bool reconnection;

          /// The number of times to attempt to reconnect if disconnected
          /// from the server. (0 = unlimited)
          int reconnectAttempts;
          int reconnectDelay;

          Options() {
              host = "127.0.0.1";
              port = 4500;

              reconnection = true;
              reconnectAttempts = 0;
              reconnectDelay = 6 * 1000; // 6 secs
          }
      };

public:
    /// Client(const net::Socket::Ptr& socket);
    Client(const net::Socket::Ptr& socket,
        const Options& options = Options()); //, const std::string& host, std::uint16_t port
    virtual ~Client();

    /// virtual void connect(const std::string& host, std::uint16_t port);
    virtual void connect();
    virtual void close();

    /// Send a default message packet.
    virtual int send(const std::string& message, bool ack = false);
    virtual int send(const json::Value& message, bool ack = false);

    /// Send an event packet.
    virtual int send(const std::string& event, const char* message, bool ack = false);
    virtual int send(const std::string& event, const std::string& message, bool ack = false);
    virtual int send(const std::string& event, const json::Value& message, bool ack = false);

    /// Send the given packet.
    virtual int send(const sockio::Packet& packet);

    /// Create a packet transaction.
    Transaction* createTransaction(const sockio::Packet& request, long timeout = 10000);

    /// Return a reference to the client options object.
    Client::Options& options();

    /// Return the underlying WebSocket instance.
    http::ws::WebSocket& ws();

    /// Return the current session ID assigned by the server.
    std::string sessionID() const;

    /// Return the error object (if any).
    scy::Error error() const;

    /// Return true if the client is is Online state.
    bool isOnline() const;

    /// Return true if currently reconnecting.
    bool reconnecting() const;

    /// Return true if the client was previously in the Online state.
    /// Useful for delegates handling the Error state.
    bool wasOnline() const;

    virtual const char* className() const { return "SocketIO::Client"; }

protected:
    virtual void setError(const scy::Error& error);

    /// Reset variables and timers at the beginning and end of each session.
    virtual void reset();

    virtual void onConnect();
    virtual void onOnline();
    virtual void onClose();
    virtual void onPacket(sockio::Packet& packet);
    virtual void onHandshake(sockio::Packet& packet);
    virtual void onMessage(sockio::Packet& packet);

    virtual void onSocketConnect();
    virtual void onSocketRecv(const MutableBuffer& buffer, const net::Address& peerAddress);
    virtual void onSocketError(const scy::Error& error);
    virtual void onSocketClose();

    virtual void onPingTimer();
    virtual void onPingTimeoutTimer();

    virtual void startReconnectTimer();
    virtual void stopReconnectTimer();
    virtual void onReconnectTimer();

    virtual int sendPing();
    virtual void onPong();

protected:
    // mutable Mutex    _mutex;

    Timer _pingTimer;
    Timer _pingTimeoutTimer;
    Timer _reconnectTimer;
    scy::Error _error;
    std::string _sessionID;
    // std::string _host;
    // std::uint16_t _port;
    Client::Options _options;
    http::ws::WebSocket _ws;
    int _pingTimeout;
    int _pingInterval;
    bool _reconnecting;
    bool _wasOnline;
};


//
// TCP Client
//


Client* createTCPClient(const Client::Options& options = Client::Options(), uv::Loop* loop = uv::defaultLoop());

class TCPClient: public Client
{
public:
    TCPClient(const Client::Options& options = Client::Options(), uv::Loop* loop = uv::defaultLoop());
};


//
// SSL Client
//


Client* createSSLClient(const Client::Options& options = Client::Options(), uv::Loop* loop = uv::defaultLoop());

class SSLClient: public Client
{
public:
    SSLClient(const Client::Options& options = Client::Options(), uv::Loop* loop = uv::defaultLoop());
};


} } // namespace scy::sockio


#endif // SCY_SocketIO_Client_H

/// @\}
