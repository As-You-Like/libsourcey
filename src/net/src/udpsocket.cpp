///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup net
/// @{


#include "scy/net/udpsocket.h"
#include "scy/logger.h"
#include "scy/net/net.h"


using namespace std;


namespace scy {
namespace net {


UDPSocket::UDPSocket(uv::Loop* loop)
    : uv::Handle(loop)
    , _buffer(65536)
{
    // TraceS(this) << "Create" << endl;
    init();
}


UDPSocket::~UDPSocket()
{
    // TraceS(this) << "Destroy" << endl;
}


void UDPSocket::init()
{
    if (initialized())
        return;

    if (!ptr()) {
        Handle::create<uv_udp_t>();
        ptr()->data = this;
    }
    // invoke(&uv_udp_init, loop(), ptr<uv_udp_t>());
    Handle::init<uv_udp_t>(&uv_udp_init);
}


void UDPSocket::reset()
{
    Handle::reset<uv_udp_t>();
    ptr()->data = this;
    init();
}


void UDPSocket::connect(const Address& peerAddress)
{
    init();
    _peer = peerAddress;

    // Emit the Connected signal to mimic TCPSocket behaviour
    // since socket implementations are interchangable.
    onSocketConnect(*this);
}


void UDPSocket::close()
{
    // TraceS(this) << "Closing" << endl;
    if (initialized() && !closed())
        recvStop();
    uv::Handle::close();
}


void UDPSocket::bind(const Address& address, unsigned flags)
{
    // TraceS(this) << "Binding on " << address << endl;
    init();

    if (address.af() == AF_INET6)
        flags |= UV_UDP_IPV6ONLY;

    if (invoke(&uv_udp_bind, ptr<uv_udp_t>(), address.addr(), flags))
        recvStart();
}


ssize_t UDPSocket::send(const char* data, size_t len, int flags)
{
    assert(_peer.valid());
    return send(data, len, _peer, flags);
}


namespace internal {
struct SendRequest
{
    uv_udp_send_t req;
    uv_buf_t buf;
};
}


ssize_t UDPSocket::send(const char* data, size_t len,
                            const Address& peerAddress, int /* flags */)
{
    // TraceS(this) << "Send: " << len << ": " << peerAddress << endl;
    assert(Thread::currentID() == tid());
    assert(_ptr);
    assert(initialized());
    // assert(len <= net::MAX_UDP_PACKET_SIZE);

    if (_peer.valid() && _peer != peerAddress) {
        ErrorS(this) << "Peer not authorized: " << peerAddress << endl;
        return -1;
    }

    if (!peerAddress.valid()) {
        ErrorS(this) << "Peer not valid: " << peerAddress << endl;
        return -1;
    }

    auto sr = new internal::SendRequest;
    sr->buf = uv_buf_init((char*)data, (unsigned int)len); // TODO: memcpy data?
    int r = uv_udp_send(&sr->req, ptr<uv_udp_t>(), &sr->buf, 1, peerAddress.addr(), UDPSocket::afterSend);
    if (r) {
        ErrorS(this) << "Send failed: " << uv_err_name(r) << endl;
        setUVError("Invalid UDP socket", r);
    }

    // R is -1 on error, otherwise return len
    return r ? r : len;
}


bool UDPSocket::setBroadcast(bool enable)
{
    assert(_ptr);
    assert(initialized());
    return uv_udp_set_broadcast(ptr<uv_udp_t>(), enable ? 1 : 0) == 0;
}


bool UDPSocket::setMulticastLoop(bool enable)
{
    assert(_ptr);
    assert(initialized());
    return uv_udp_set_multicast_loop(ptr<uv_udp_t>(), enable ? 1 : 0) == 0;
}


bool UDPSocket::setMulticastTTL(int ttl)
{
    assert(_ptr);
    assert(initialized());
    assert(ttl > 0 && ttl <= 255);
    return uv_udp_set_multicast_ttl(ptr<uv_udp_t>(), ttl) == 0;
}


bool UDPSocket::recvStart()
{
    assert(_ptr);
    assert(initialized());

    // UV_EALREADY means that the socket is already bound but that's okay
    // TODO: No need for boolean value as this method can throw exceptions
    // since it is called internally by bind().
    int r = uv_udp_recv_start(ptr<uv_udp_t>(), UDPSocket::allocRecvBuffer, onRecv);
    if (r && r != UV_EALREADY) {
        setUVError("Cannot start recv on invalid UDP socket", r);
        return false;
    }
    return true;
}


bool UDPSocket::recvStop()
{
    assert(_ptr);
    assert(initialized());

    // This method must not throw since it is called
    // internally via libuv callbacks.
    return uv_udp_recv_stop(ptr<uv_udp_t>()) == 0;
}


void UDPSocket::onRecv(const MutableBuffer& buf, const net::Address& address)
{
    // TraceS(this) << "Recv: " << buf.size() << endl;
    // emitRecv(buf, address);
    onSocketRecv(*this, buf, address);
}


void UDPSocket::setError(const scy::Error& err)
{
    uv::Handle::setError(err);
}


const scy::Error& UDPSocket::error() const
{
    return uv::Handle::error();
}


net::Address UDPSocket::address() const
{
    if (_ptr) {
        struct sockaddr address;
        int addrlen = sizeof(address);
        if (uv_udp_getsockname(ptr<uv_udp_t>(), &address, &addrlen) == 0)
            return net::Address(&address, addrlen);
    }
    return net::Address();
}


net::Address UDPSocket::peerAddress() const
{
    return _peer;
}


net::TransportType UDPSocket::transport() const
{
    return net::UDP;
}


bool UDPSocket::closed() const
{
    return uv::Handle::closed();
}


//
// Callbacks

void UDPSocket::onRecv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
                       const struct sockaddr* addr, unsigned /* flags */)
{
    auto socket = static_cast<UDPSocket*>(handle->data);
    // TraceS(socket) << "On recv: " << nread << endl;

    if (nread < 0) {
        // assert(0 && "unexpected error");
        DebugS(socket) << "Recv error: " << uv_err_name((int)nread) << endl;
        socket->setUVError("UDP error", (int)nread);
        return;
    }

    if (nread == 0) {
        assert(addr == nullptr);
        // Returning unused buffer, this is not an error
        // 11/12/13: This happens on linux but not windows
        // socket->setUVError("End of file", UV_EOF);
        return;
    }

    socket->onRecv(mutableBuffer(buf->base, nread),
                   net::Address(addr, sizeof(*addr)));
}


void UDPSocket::afterSend(uv_udp_send_t* req, int status)
{
    auto sr = reinterpret_cast<internal::SendRequest*>(req);
    auto socket = reinterpret_cast<UDPSocket*>(sr->req.handle->data);
    if (status) {
        DebugS(socket) << "Send error: " << uv_err_name(status) << endl;
        socket->setUVError("UDP send error", status);
    }
    delete sr;
}


void UDPSocket::allocRecvBuffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    auto self = static_cast<UDPSocket*>(handle->data);
    // TraceA("Allocating Buffer: ", suggested_size)

    // Reserve the recommended buffer size
    // XXX: libuv wants us to allocate 65536 bytes for UDP .. hmmm
    // if (suggested_size > self->_buffer.available())
    //    self->_buffer.reserve(suggested_size);
    // assert(self->_buffer.capacity() >= suggested_size);
    assert(self->_buffer.size() >= suggested_size);

    // Reset the buffer position on each read
    // self->_buffer.position(0);
    buf->base = self->_buffer.data();
    buf->len = self->_buffer.size();

    // return uv_buf_init(self->_buffer.data(), suggested_size);
}


void UDPSocket::onError(const scy::Error& error)
{
    DebugS(this) << "Error: " << error.message << endl;
    onSocketError(*this, error);
    close(); // close on error
}


void UDPSocket::onClose()
{
    DebugS(this) << "On close" << endl;
    onSocketClose(*this);
}


uv::Loop* UDPSocket::loop() const
{
    return uv::Handle::loop();
}


} // namespace net
} // namespace scy


/// @\}
