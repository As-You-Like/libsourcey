///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup turn
/// @{


#include "scy/turn/client/tcpclient.h"
#include "scy/logger.h"
#include "scy/net/tcpsocket.h"

#include <algorithm>
#include <assert.h>
#include <iostream>


using namespace std;


namespace scy {
namespace turn {


TCPClient::TCPClient(TCPClientObserver& observer, const Client::Options& options)
    : Client(observer, options)
    , _observer(observer)
{
    TraceL << "Create" << endl;

    _socket = net::makeSocket<net::TCPSocket>();
}


TCPClient::~TCPClient()
{
    TraceL << "Destroy" << endl;
    shutdown();
    // assert(connections().empty());
}


void TCPClient::initiate()
{
    Client::initiate();
}


void TCPClient::shutdown()
{
    TraceL << "Shutdown" << endl;

    // if (closed()) {
    //    TraceL << "Already closed" << endl;
    //    return;
    //}

    // Destroy transactions and stop timer
    Client::shutdown();

    {
        auto connections = _connections.map();
        TraceL << "Shutdown: Active connections: " << connections.size() << endl;
        for (auto it = connections.begin(); it != connections.end(); ++it) {
            // assert(it->second->base().refCount() == 1);

            // The connection will be removed via onRelayConnectionClosed
            it->second->close();
        }
        // assert(connections().empty());
    }

    TraceL << "Shutdown: OK" << endl;
}


void TCPClient::sendConnectRequest(const net::Address& peerAddress)
{
    // 4.3. Initiating a Connection
    //
    // To initiate a TCP connection to a peer, a client MUST send a Connect
    // request over the control connection for the desired allocation.  The
    // Connect request MUST include an XOR-PEER-ADDRESS attribute containing
    // the transport address of the peer to which a connection is desired.

    TraceL << "Send Connect request" << endl;

    auto transaction = createTransaction();
    // transaction->request().setType(stun::Message::Connect);
    // stun::Message request(stun::Message::Request, stun::Message::Allocate);
    transaction->request().setClass(stun::Message::Request);
    transaction->request().setMethod(stun::Message::Connect);

    auto peerAttr = new stun::XorPeerAddress;
    peerAttr->setAddress(peerAddress);
    // peerAttr->setFamily(1);
    // peerAttr->setPort(peerAddress.port());
    // peerAttr->setIP(peerAddress.host());
    transaction->request().add(peerAttr);

    sendAuthenticatedTransaction(transaction);
}


void TCPClient::sendData(const char* data, std::size_t size, const net::Address& peerAddress)
{
    TraceL << "Send data to " << peerAddress << endl;

    // Ensure permissions exist for the peer.
    if (!hasPermission(peerAddress.host()))
        throw std::runtime_error("No permission exists for peer: " + peerAddress.host());

    auto conn = connections().get(peerAddress, nullptr);
    if (!conn)
        throw std::runtime_error("No peer exists for: " + peerAddress.toString());

    conn->send(data, size);
}


bool TCPClient::handleResponse(const stun::Message& response)
{
    if (!Client::handleResponse(response)) {
        if (response.methodType() == stun::Message::Connect &&
            response.classType() == stun::Message::SuccessResponse)
            handleConnectResponse(response);

        else if (response.methodType() == stun::Message::ConnectionAttempt)
            handleConnectionAttemptIndication(response);

        else if (response.methodType() == stun::Message::ConnectionBind &&
                 response.classType() == stun::Message::SuccessResponse)
            handleConnectionBindResponse(response);

        else if (response.methodType() == stun::Message::ConnectionBind &&
                 response.classType() == stun::Message::ErrorResponse)
            handleConnectionBindErrorResponse(response);

        else if (response.methodType() == stun::Message::Connect &&
                 response.classType() == stun::Message::ErrorResponse)
            handleConnectErrorResponse(response);

        else
            return false;
    }

    return true;
}


void TCPClient::handleConnectResponse(const stun::Message& response)
{
    // If the connection is successfully established, the client will
    // receive a success response. That response will contain a
    // CONNECTION-ID attribute. The client MUST initiate a new TCP
    // connection to the server, utilizing the same destination transport
    // address to which the control connection was established.  This
    // connection MUST be made using a different local transport address.
    // Authentication of the client by the server MUST use the same method
    // and credentials as for the control connection.  Once established, the
    // client MUST send a ConnectionBind request over the new connection.
    // That request MUST include the CONNECTION-ID attribute, echoed from
    // the Connect Success response.  When a response to the ConnectionBind
    // request is received, if it is a success, the TCP connection on which
    // it was sent is called the client data connection corresponding to the
    // peer.
    //

    auto transaction = reinterpret_cast<stun::Transaction*>(response.opaque);
    auto peerAttr = transaction->request().get<stun::XorPeerAddress>();
    if (!peerAttr || (peerAttr && peerAttr->family() != 1)) {
        assert(0);
        return;
    }

    auto connAttr = response.get<stun::ConnectionID>();
    if (!connAttr) {
        assert(0);
        return;
    }

    createAndBindConnection(connAttr->value(), peerAttr->address());
}


void TCPClient::handleConnectErrorResponse(const stun::Message& response)
{
    // If the result of the Connect request was an Error Response, and the
    // response code was 447 (Connection Timeout or Failure), it means that
    // the TURN server was unable to connect to the peer.  The client MAY
    // retry with the same XOR-PEER-ADDRESS attribute, but MUST wait at
    // least 10 seconds.
    //
    // As with any other request, multiple Connect requests MAY be sent
    // simultaneously.  However, Connect requests with the same XOR-PEER-
    // ADDRESS parameter MUST NOT be sent simultaneously.

    auto transaction = reinterpret_cast<stun::Transaction*>(response.opaque);
    auto peerAttr = transaction->request().get<stun::XorPeerAddress>();
    if (!peerAttr || (peerAttr && peerAttr->family() != 1)) {
        assert(0);
        return;
    }

    _observer.onRelayConnectionBindingFailed(*this, peerAttr->address());
}


void TCPClient::handleConnectionAttemptIndication(const stun::Message& response)
{
    // 4.4. Receiving a Connection
    //
    // After an Allocate request is successfully processed by the server,
    // the client will start receiving a ConnectionAttempt indication each
    // time a peer for which a permission has been installed attempts a new
    // connection to the relayed transport address.  This indication will
    // contain CONNECTION-ID and XOR-PEER-ADDRESS attributes.  If the client
    // wishes to accept this connection, it MUST initiate a new TCP
    // connection to the server, utilizing the same destination transport
    // address to which the control connection was established.  This
    // connection MUST be made using a different local transport address.
    // Authentication of the client by the server MUST use the same method
    // and credentials as for the control connection.  Once established, the
    // client MUST send a ConnectionBind request over the new connection.
    // That request MUST include the CONNECTION-ID attribute, echoed from
    // the ConnectionAttempt indication.  When a response to the
    // ConnectionBind request is received, if it is a success, the TCP
    // connection on which it was sent is called the client data connection
    // corresponding to the peer.

    auto peerAttr = response.get<stun::XorPeerAddress>();
    if (!peerAttr || (peerAttr && peerAttr->family() != 1)) {
        assert(0);
        return;
    }

    auto connAttr = response.get<stun::ConnectionID>();
    if (!connAttr) {
        assert(0);
        return;
    }

    if (_observer.onPeerConnectionAttempt(*this, peerAttr->address()))
        createAndBindConnection(connAttr->value(), peerAttr->address());
}


void TCPClient::handleConnectionBindResponse(const stun::Message& response)
{
    TraceL << "ConnectionBind success response" << endl;

    auto transaction = reinterpret_cast<stun::Transaction*>(response.opaque);
    auto req = reinterpret_cast<RelayConnectionBinding*>(transaction->socket->opaque);

    auto conn = connections().get(req->peerAddress, nullptr);
    if (!conn) {
        assert(0);
        return;
    }

    // Data will now be transferred as-is to and from the peer.
    conn->Recv += slot(this, &TCPClient::onRelayDataReceived);
    _observer.onRelayConnectionCreated(*this, conn, req->peerAddress);

    TraceL << "ConnectionBind success response: OK" << endl;
}


void TCPClient::handleConnectionBindErrorResponse(const stun::Message& response)
{
    TraceL << "ConnectionBind error response" << endl;

    auto transaction = reinterpret_cast<stun::Transaction*>(response.opaque);
    auto req = reinterpret_cast<RelayConnectionBinding*>(transaction->socket->opaque);

    // TODO: Handle properly

    freeConnection(req->peerAddress);
}


bool TCPClient::createAndBindConnection(std::uint32_t connectionID, const net::Address& peerAddress)
{
    TraceL << "Create and bind connection: " << peerAddress << endl;

    try {
        net::TCPSocket::Ptr conn(net::makeSocket<net::TCPSocket>());
        conn->Connect += slot(this, &TCPClient::onRelayConnectionConnect);
        conn->Error += slot(this, &TCPClient::onRelayConnectionError);
        conn->Close += slot(this, &TCPClient::onRelayConnectionClosed);

        auto req = new RelayConnectionBinding;
        req->connectionID = connectionID;
        req->peerAddress = peerAddress;
        conn->opaque = req;

        conn->connect(_options.serverAddr); // will throw on error

        _connections.add(peerAddress, conn);
        return true;
    } catch (std::exception& exc) {
        // Socket instance deleted via state callback
        ErrorL << "ConnectionBind Error: " << exc.what() << endl;
    }

    return false;
}


void TCPClient::onRelayConnectionConnect(net::Socket& socket)
{
    TraceL << "onRelayConnectionConnect" << endl;

    socket.Connect -= slot(this, &TCPClient::onRelayConnectionConnect);
    auto req = reinterpret_cast<RelayConnectionBinding*>(socket.opaque);
    assert(connections().has(req->peerAddress));

    auto transaction = createTransaction(connections().get(req->peerAddress));
    transaction->request().setClass(stun::Message::Request);
    transaction->request().setMethod(stun::Message::ConnectionBind);

    auto connAttr = new stun::ConnectionID;
    connAttr->setValue(req->connectionID);
    transaction->request().add(connAttr);

    // assert(transaction->socket() == &conn);
    sendAuthenticatedTransaction(transaction);
}


void TCPClient::onRelayConnectionError(net::Socket& socket, const Error& /* error */)
{
    auto req = reinterpret_cast<RelayConnectionBinding*>(socket.opaque);
    auto conn = _connections.get(req->peerAddress);

    TraceL << "Relay connection error: " << req->peerAddress << endl;
    assert(connections().has(req->peerAddress));

    _observer.onRelayConnectionError(*this, conn, req->peerAddress);
}


void TCPClient::onRelayConnectionClosed(net::Socket& socket)
{
    auto req = reinterpret_cast<RelayConnectionBinding*>(socket.opaque);
    auto conn = _connections.get(req->peerAddress);

    TraceL << "Relay connection closed: " << req->peerAddress << endl;
    assert(connections().has(req->peerAddress));

    _observer.onRelayConnectionClosed(*this, conn, req->peerAddress);
    freeConnection(req->peerAddress);
}


void TCPClient::onRelayDataReceived(net::Socket& socket,
                                    const MutableBuffer& buffer,
                                    const net::Address& peerAddress)
{
    auto req = reinterpret_cast<RelayConnectionBinding*>(socket.opaque);
    assert(connections().has(req->peerAddress));
    // TraceL << "Relay Data Received: " << peerAddress << ": " << req->peerAddress << endl;
    // assert(req->peerAddress == peerAddress);

    _observer.onRelayDataReceived(*this, bufferCast<const char*>(buffer),
                                  buffer.size(), req->peerAddress);
}


void TCPClient::freeConnection(const net::Address& peerAddress)
{
    TraceL << "Freeing TCP connection: " << peerAddress << endl;

    auto socket = connections().get(peerAddress);
    socket->Recv -= slot(this, &TCPClient::onRelayDataReceived);
    socket->Connect -= slot(this, &TCPClient::onRelayConnectionConnect);
    socket->Error -= slot(this, &TCPClient::onRelayConnectionError);
    socket->Close -= slot(this, &TCPClient::onRelayConnectionClosed);

    // assert(socket->base().refCount() == 1);
    // assert(connections().has(socket->address()));
    connections().remove(peerAddress); // destroy socket

    // auto map = connections().map();
    // for (auto conn : map) {
    //    assert(it->second->base().refCount() == 1);
    //    The connection will be removed via onRelayConnectionClosed
    //    it->second->close();
    //}

    delete reinterpret_cast<RelayConnectionBinding*>(socket->opaque);
}


ConnectionManager& TCPClient::connections()
{
    return _connections;
}


int TCPClient::transportProtocol()
{
    return 6; // TCP
}


} } // namespace scy::turn


/// @\}
