///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup net
/// @{


#include "scy/net/tcpsocket.h"
#include "scy/logger.h"


using std::endl;


namespace scy {
namespace net {


TCPSocket::TCPSocket(uv::Loop* loop) :
    Stream(loop)
{
    TraceS(this) << "Create" << endl;
    init();
}


TCPSocket::~TCPSocket()
{
    TraceS(this) << "Destroy" << endl;
    close();
}


void TCPSocket::init()
{
    if (ptr()) return;

    TraceS(this) << "Init" << endl;
    auto tcp = new uv_tcp_t;
    tcp->data = this;
    _ptr = reinterpret_cast<uv_handle_t*>(tcp);
    _closed = false;
    _error.reset();
    int r = uv_tcp_init(loop(), tcp);
    if (r)
        setUVError("Cannot initialize TCP socket", r);
}


namespace internal {

    UVStatusCallbackWithType(TCPSocket, onConnect, uv_connect_t);
    UVStatusCallbackWithType(TCPSocket, onAcceptConnection, uv_stream_t);

}


void TCPSocket::connect(const net::Address& peerAddress)
{
    TraceS(this) << "Connecting to " << peerAddress << endl;
    init();
    auto req = new uv_connect_t;
    req->data = this;
    int r = uv_tcp_connect(req, ptr<uv_tcp_t>(), peerAddress.addr(), internal::onConnect);
    if (r) setAndThrowError("TCP connect failed", r);
}


void TCPSocket::bind(const net::Address& address, unsigned flags)
{
    TraceS(this) << "Binding on " << address << endl;
    init();
    int r;
    switch (address.af()) {
    case AF_INET:
        r = uv_tcp_bind(ptr<uv_tcp_t>(), address.addr(), flags);
        break;
    //case AF_INET6:
    //    r = uv_tcp_bind6(ptr<uv_tcp_t>(), *reinterpret_cast<const sockaddr_in6*>(address.addr()));
    //    break;
    default:
        throw std::runtime_error("Unexpected address family");
    }
    if (r) setAndThrowError("TCP bind failed", r);
}


void TCPSocket::listen(int backlog)
{
    TraceS(this) << "Listening" << endl;
    init();
    int r = uv_listen(ptr<uv_stream_t>(), backlog, internal::onAcceptConnection);
    if (r) setAndThrowError("TCP listen failed", r);
}


bool TCPSocket::shutdown()
{
    TraceS(this) << "Shutdown" << endl;
    return Stream::shutdown();
}


void TCPSocket::close()
{
    TraceS(this) << "Close" << endl;
    Stream::close();
}


void TCPSocket::setNoDelay(bool enable)
{
    init();
    int r = uv_tcp_nodelay(ptr<uv_tcp_t>(), enable ? 1 : 0);
    if (r) setUVError("TCP socket error", r);
}


void TCPSocket::setKeepAlive(int enable, unsigned int delay)
{
    init();
    int r = uv_tcp_keepalive(ptr<uv_tcp_t>(), enable, delay);
    if (r) setUVError("TCP socket error", r);
}


#ifdef _WIN32
void TCPSocket::setSimultaneousAccepts(bool enable)
{
    init();
    int r = uv_tcp_simultaneous_accepts(ptr<uv_tcp_t>(), enable ? 1 : 0);
    if (r) setUVError("TCP socket error", r);
}
#endif


int TCPSocket::send(const char* data, std::size_t len, int flags)
{
    return send(data, len, peerAddress(), flags);
}


int TCPSocket::send(const char* data, std::size_t len, const net::Address& /* peerAddress */, int /* flags */)
{
    TraceS(this) << "Send: " << len << endl;
    // TraceS(this) << "Send: " << len << ": " << std::string(data, len) << endl;
    assert(Thread::currentID() == tid());
    //assert(len <= net::MAX_TCP_PACKET_SIZE); // libuv handles this for us

    if (!Stream::write(data, len)) {
        WarnL << "Send error" << endl;
        return -1;
    }

    // TODO: Return native error code
    return len;
}


void TCPSocket::acceptConnection()
{
    // Create the shared socket pointer so the if the socket handle is not
    // incremented the accepted socket will be destroyed.
    auto socket = net::makeSocket<net::TCPSocket>(loop());
    TraceS(this) << "Accept connection: " << socket->ptr() << endl;
    uv_accept(ptr<uv_stream_t>(), socket->ptr<uv_stream_t>());
    socket->readStart();
    AcceptConnection.emit(/*Socket::self(), */socket);
}


net::Address TCPSocket::address() const
{
    if (!active())
        return net::Address();
        //throw std::runtime_error("Invalid TCP socket: No address");

    struct sockaddr_storage address;
    int addrlen = sizeof(address);
    int r = uv_tcp_getsockname(ptr<uv_tcp_t>(),
                                reinterpret_cast<sockaddr*>(&address),
                                &addrlen);
    if (r)
        return net::Address();
        //throwLastError("Invalid TCP socket: No address");

    return net::Address(reinterpret_cast<const sockaddr*>(&address), addrlen);
}


net::Address TCPSocket::peerAddress() const
{
    //TraceS(this) << "Get peer address: " << closed() << endl;
    if (!active())
        return net::Address();
        //throw std::runtime_error("Invalid TCP socket: No peer address");

    struct sockaddr_storage address;
    int addrlen = sizeof(address);
    int r = uv_tcp_getpeername(ptr<uv_tcp_t>(),
                                reinterpret_cast<sockaddr*>(&address),
                                &addrlen);

    if (r)
        return net::Address();
        //throwLastError("Invalid TCP socket: No peer address");

    return net::Address(reinterpret_cast<const sockaddr*>(&address), addrlen);
}


void TCPSocket::setError(const scy::Error& err)
{
    assert(!error().any());
    Stream::setError(err);
}


const scy::Error& TCPSocket::error() const
{
    return Stream::error();
}


net::TransportType TCPSocket::transport() const
{
    return net::TCP;
}


bool TCPSocket::closed() const
{
    return Stream::closed();
}


uv::Loop* TCPSocket::loop() const
{
    return uv::Handle::loop();
}


void TCPSocket::setMode(SocketMode mode)
{
    _mode = mode;
}


const SocketMode TCPSocket::mode() const
{
    return _mode;
}


//
// Callbacks

void TCPSocket::onRead(const char* data, std::size_t len)
{
    TraceS(this) << "On read: " << len << endl;

    // Note: The const_cast here is relatively safe since the given
    // data pointer is the underlying _buffer.data() pointer, but
    // a better way should be devised.
    onRecv(mutableBuffer(const_cast<char*>(data), len));
}


void TCPSocket::onRecv(const MutableBuffer& buf)
{
    TraceS(this) << "Recv: " << buf.size() << endl;
    onSocketRecv(*this, buf, peerAddress());
}


void TCPSocket::onConnect(uv_connect_t* handle, int status)
{
    TraceS(this) << "On connect" << endl;

    // Error handled by static callback proxy
    if (status == 0) {
        if (readStart())
            onSocketConnect(*this);
    }
    else {
        setUVError("Connection failed", status);
        //ErrorS(this) << "Connection failed: " << error().message << endl;
    }
    delete handle;
}


void TCPSocket::onAcceptConnection(uv_stream_t*, int status)
{
    if (status == 0) {
        TraceS(this) << "On accept connection" << endl;
        acceptConnection();
    }
    else
        ErrorS(this) << "Accept connection failed" << endl;
}


void TCPSocket::onError(const scy::Error& error)
{
    DebugS(this) << "Error: " << error.message << endl;
    onSocketError(*this, error);
    close(); // close on error
}


void TCPSocket::onClose()
{
    TraceS(this) << "On close" << endl;
    onSocketClose(*this);
}


} } // namespace scy::net

/// @\}
