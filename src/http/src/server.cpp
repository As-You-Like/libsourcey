///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup http
/// @{


#include "scy/http/server.h"
#include "scy/http/websocket.h"
#include "scy/logger.h"
#include "scy/util.h"


using std::endl;


namespace scy {
namespace http {


Server::Server(const net::Address& address, net::TCPSocket::Ptr socket)
    : _address(address)
    , _socket(socket)
    , _timer(5000, 5000, socket->loop())
    , _factory(new ServerConnectionFactory())
{
    // TraceS(this) << "Create" << endl;
}


Server::~Server()
{
    // TraceS(this) << "Destroy" << endl;
    shutdown();
    if (_factory)
        delete _factory;
}


void Server::start()
{
    _socket->setReceiver(this);
    _socket->AcceptConnection += slot(this, &Server::onClientSocketAccept);
    _socket->bind(_address);
    _socket->listen(1000);

    DebugS(this) << "HTTP server listening on " << _address << endl;

    _timer.Timeout += slot(this, &Server::onTimer);
    _timer.start();
}


void Server::shutdown()
{
    // TraceS(this) << "Shutdown" << endl;

    if (_socket) {
        _socket->removeReceiver(this);
        _socket->AcceptConnection -= slot(this, &Server::onClientSocketAccept);
        _socket->close();
    }

    _timer.stop();

    Shutdown.emit();
}


void Server::onClientSocketAccept(const net::TCPSocket::Ptr& socket)
{
    // TraceS(this) << "On accept socket connection" << endl;

    // FIXME

    ServerConnection::Ptr conn = _factory->createConnection(*this, socket);
    //ServerConnection::Ptr conn1 = _factory->createConnection(*this, socket);
    //ServerConnection::Ptr conn2 = _factory->createConnection(*this, socket);
    conn->Close += slot(this, &Server::onConnectionClose);
    _connections.push_back(conn);

    //socket->setReceiver(this);
    //_sockets.push_back(socket);

    // std::clock_t start = std::clock();
    //new ServerConnection(*this, socket);
    // ServerConnection::Ptr conn = _factory->createConnection(*this, socket);
    // conn->Close += slot(this, &Server::onConnectionClose);
    // //std::cout << "create connection time: " << (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000) << " ms" << std::endl;
    // _connections.push_back(conn);
    // std::cout << "create connection time 1: " << (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000) << " ms" << std::endl;
}


void Server::onSocketRecv(net::Socket& socket, const MutableBuffer& buffer, const net::Address& peerAddress)
{
    //DebugL << "On recv: " << &socket << ": " << buffer.str() << std::endl;
    // std::cout << "On recv: " << &socket << ": " << buffer.str() << std::endl;

    // Echo it back
    // socket.send(bufferCast<const char*>(buffer), buffer.size());

    // Send a HTTP packet
    std::ostringstream res;
    res << "HTTP/1.1 200 OK\r\n"
        << "Connection: close\r\n"
        << "Content-Length: 0" << "\r\n"
        << "\r\n";
    std::string response(res.str());
    socket.send(response.c_str(), response.size());
}

void Server::onConnectionReady(ServerConnection& conn)
{
    // TraceS(this) << "On connection ready" << endl;

    for (auto it = _connections.begin(); it != _connections.end(); ++it) {
        if (it->get() == &conn) {
            Connection.emit(*it);
            return;
        }
    }
}


void Server::onConnectionClose(ServerConnection& conn)
{
    // TraceS(this) << "On connection closed" << endl;

    // std::cout << "release connection: " << _connections.size() << std::endl;
    // std::clock_t start = std::clock();
    for (auto it = _connections.begin(); it != _connections.end(); ++it) {
        if (it->get() == &conn) {
            _connections.erase(it);
            // std::cout << "release connection time: " << (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000) << " ms" << std::endl;
            return;
        }
    }
}


void Server::onSocketClose(net::Socket& socket)
{
    // TraceS(this) << "On server socket close" << endl;

    // FIXME

    // for (auto it = _sockets.begin(); it != _sockets.end(); ++it) {
    //     if (it->get() == &socket) {
    //         _sockets.erase(it);
    //         //return;
    //     }
    // }
    //
    // // clean the connection that matches the closing socket
    // for (auto it = _connections.begin(); it != _connections.end(); ++it) {
    //     if (it->get()->socket().get() == &socket) {
    //         _connections.erase(it);
    //         //return;
    //     }
    // }
}


void Server::onTimer()
{
    // DebugS(this) << "Num active HTTP server connections: " << connections.size() << endl;

    // TODO: cleanup timed out pending connections
}


net::Address& Server::address()
{
    return _address;
}


//
// Server Connection
//


ServerConnection::ServerConnection(Server& server, net::TCPSocket::Ptr socket)
    : Connection(socket)
    , _server(server)
    , _upgrade(false)
{
    // TraceS(this) << "Create" << endl;
    replaceAdapter(new ConnectionAdapter(this, HTTP_REQUEST));
}


ServerConnection::~ServerConnection()
{
    // TraceS(this) << "Destroy" << endl;
    close(); // FIXME
}


Server& ServerConnection::server()
{
    return _server;
}


void ServerConnection::onHeaders()
{
    // TraceS(this) << "On headers" << endl;

#if 0
    _response.add("Content-Length", "0");
    _response.add("Connection", "close"); // "keep-alive"
    sendHeader();
    //send("hello universe", 14);
    //close();
    return;
#endif

    // Upgrade the connection if required
    _upgrade = reinterpret_cast<ConnectionAdapter*>(adapter())->parser().upgrade();
    if (_upgrade) {
        //util::icompare(request().get("Connection", ""), "upgrade") == 0 &&
        //util::icompare(request().get("Upgrade", ""), "websocket") == 0) {
        // TraceS(this) << "Upgrading to WebSocket: " << request() << endl;

        // Note: To upgrade the connection we need to replace the
        // underlying SocketAdapter instance. Since we are currently
        // inside the default ConnectionAdapter's HTTP parser callback
        // scope we just swap the SocketAdapter instance pointers and do
        // a deferred delete on the old adapter. No more callbacks will be
        // received from the old adapter after replaceAdapter is called.
        auto wsAdapter = new ws::ConnectionAdapter(this, ws::ServerSide);
        replaceAdapter(wsAdapter);

        // Send the handshake request to the WS adapter for handling.
        // If the request fails the underlying socket will be closed
        // resulting in the destruction of the current connection.

        // std::ostringstream oss;
        // request().write(oss);
        // request().clear();
        // std::string buffer(oss.str());

        std::string buffer;
        buffer.reserve(256);
        request().write(buffer);
        request().clear();

        wsAdapter->onSocketRecv(*socket().get(), mutableBuffer(buffer), socket()->peerAddress());
    }

    // Upgraded connections don't receive the onHeaders callback
    //if (!_upgrade)
    //    _responder->onHeaders(_request);

    _server.onConnectionReady(*this);
}


void ServerConnection::onPayload(const MutableBuffer& buffer)
{
    // TraceS(this) << "On payload: " << buffer.size() << endl;

    // The connection may have been closed inside a previous callback.
    if (_closed) {
        // TraceS(this) << "On payload: Closed" << endl;
        return;
    }

    // assert(_responder);
    // _responder->onPayload(buffer);
    Payload.emit(*this, buffer);
}


void ServerConnection::onComplete()
{
    // TraceS(this) << "On complete" << endl;

    // The connection may have been closed inside a previous callback.
    if (_closed) {
        // TraceS(this) << "On complete: Closed" << endl;
        return;
    }

    // The HTTP request is complete.
    // The request handler can give a response.
    // assert(_responder);
    // _responder->onRequest(_request, _response);
}


void ServerConnection::onClose()
{
    // TraceS(this) << "On close" << endl;

    Close.emit(*this);
}


http::Message* ServerConnection::incomingHeader()
{
    return reinterpret_cast<http::Message*>(&_request);
}


http::Message* ServerConnection::outgoingHeader()
{
    return reinterpret_cast<http::Message*>(&_response);
}


} // namespace http
} // namespace scy


/// @\}
