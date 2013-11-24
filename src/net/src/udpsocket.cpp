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


#include "scy/net/udpsocket.h"
#include "scy/net/types.h"
#include "scy/logger.h"


using namespace std;


namespace scy {
namespace net {


UDPSocket::UDPSocket() : 
	net::Socket(new UDPBase, false)
{
}


UDPSocket::UDPSocket(UDPBase* base, bool shared) : 
	net::Socket(base, shared) 
{
}


UDPSocket::UDPSocket(const Socket& socket) : 
	net::Socket(socket)
{
	if (!dynamic_cast<UDPBase*>(_base))
		throw std::runtime_error("Cannot assign incompatible socket");
}
	

UDPBase& UDPSocket::base() const
{
	return static_cast<UDPBase&>(*_base);
}


//
// UDP Base
//


UDPBase::UDPBase() :
	_buffer(65536)
{
	traceL("UDPBase", this) << "Create" << endl;
	init();
}


UDPBase::~UDPBase()
{
	traceL("UDPBase", this) << "Destroy" << endl;
}


void UDPBase::init() 
{
	if (ptr()) return;
	
	traceL("TCPBase", this) << "Init" << endl;
	uv_udp_t* udp = new uv_udp_t;
	udp->data = this; //instance();
	_closed = false;
	_ptr = reinterpret_cast<uv_handle_t*>(udp);
	int r = uv_udp_init(loop(), udp);
	if (r)
		setUVError("Cannot initialize UDP socket", r);
}


void UDPBase::connect(const Address& peerAddress) 
{
	_peer = peerAddress;

	// Send the Connected signal to mimic TCP behaviour  
	// since socket implementations are interchangable.
	emitConnect();
}


void UDPBase::close()
{
	traceL("UDPBase", this) << "Closing" << endl;	
	recvStop();
	uv::Handle::close();
}


void UDPBase::bind(const Address& address, unsigned flags) 
{	
	traceL("UDPBase", this) << "Binding on " << address << endl;

	int r;
	switch (address.af()) {
	case AF_INET:
		r = uv_udp_bind(ptr<uv_udp_t>(), address.addr(), flags);
		break;
	//case AF_INET6:
	//	r = uv_udp_bind6(ptr<uv_udp_t>(), address.addr(), flags);
	//	break;
	default:
		throw std::runtime_error("Unexpected address family");
	}
	if (r)
		setUVError("Invalid UDP socket", r); 
	else
		recvStart();
}


int UDPBase::send(const char* data, int len, int flags) 
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


int UDPBase::send(const char* data, int len, const Address& peerAddress, int /* flags */) 
{	
	traceL("UDPBase", this) << "Send: " << len << ": " << peerAddress << endl;
	//assert(len <= net::MAX_UDP_PACKET_SIZE); // libuv handles this for us

	if (_peer.valid() && _peer != peerAddress) {
		errorL("UDPBase", this) << "Peer not authorized: " << peerAddress << endl;
		return -1;
	}

	if (!peerAddress.valid()) {
		errorL("UDPBase", this) << "Peer not valid: " << peerAddress << endl;
		return -1;
	}
	
	int r;	
	auto sr = new internal::SendRequest;
	sr->buf = uv_buf_init((char*)data, len); // TODO: memcpy data?
	r = uv_udp_send(&sr->req, ptr<uv_udp_t>(), &sr->buf, 1, peerAddress.addr(), UDPBase::afterSend);

#if 0
	switch (peerAddress.af()) {
	case AF_INET:
		r = uv_udp_send(&sr->req, ptr<uv_udp_t>(), &sr->buf, 1, peerAddress.addr(), UDPBase::afterSend);
		break;
	case AF_INET6:
		r = uv_udp_send6(&sr->req, ptr<uv_udp_t>(), &sr->buf, 1,
			*reinterpret_cast<const sockaddr_in6*>(peerAddress.addr()), UDPBase::afterSend);
		break;
	default:
		throw std::runtime_error("Unexpected address family");
	}
#endif
	if (r)
		setUVError("Invalid UDP socket", r); 

	// R is -1 on error, otherwise return len
	return r ? r : len;
}

	
bool UDPBase::setBroadcast(bool flag)
{
	if (!ptr()) return false;
	return uv_udp_set_broadcast(ptr<uv_udp_t>(), flag ? 1 : 0);
}


bool UDPBase::setMulticastLoop(bool flag)
{
	if (!ptr()) return false;
	return uv_udp_set_broadcast(ptr<uv_udp_t>(), flag ? 1 : 0);
}


bool UDPBase::setMulticastTTL(int ttl)
{
	assert(ttl > 0 && ttl < 255);
	if (!ptr()) return false;
	return uv_udp_set_broadcast(ptr<uv_udp_t>(), ttl);
}


bool UDPBase::recvStart() 
{
	// UV_EALREADY means that the socket is already bound but that's okay
	int r = uv_udp_recv_start(ptr<uv_udp_t>(), UDPBase::allocRecvBuffer, onRecv);
	if (r && r != UV_EALREADY) {
		setUVError("Invalid UDP socket", r);
		return false;
	}  
	return true;
}


bool UDPBase::recvStop() 
{
	if (!ptr())
		return false;
	return uv_udp_recv_stop(ptr<uv_udp_t>()) == 0;
}


void UDPBase::onRecv(const MutableBuffer& buf, const net::Address& address)
{
	traceL("UDPBase", this) << "Recv: " << buf.size() << endl;	

	emitRecv(buf, address);
}


void UDPBase::setError(const Error& err)
{
	uv::Handle::setError(err);
}

		
const Error& UDPBase::error() const
{
	return uv::Handle::error();
}


net::Address UDPBase::address() const
{	
	if (!active())
		return net::Address();
		//throw std::runtime_error("Invalid UDP socket: No address");

	if (_peer.valid())
		return _peer;
	
	struct sockaddr address;
	int addrlen = sizeof(address);
	int r = uv_udp_getsockname(ptr<uv_udp_t>(), &address, &addrlen);
	if (r)
		return net::Address();
		//throwLastError("Invalid UDP socket: No address");

	return Address(&address, addrlen);
}


net::Address UDPBase::peerAddress() const
{
	if (!_peer.valid())
		return net::Address();
		//throw std::runtime_error("Invalid UDP socket: No peer address");
	return _peer;
}


net::TransportType UDPBase::transport() const 
{ 
	return net::UDP; 
}
	

bool UDPBase::closed() const
{
	return uv::Handle::closed();
}


//
// Callbacks
//


void UDPBase::onRecv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned /* flags */) 
{	
	UDPBase* socket = static_cast<UDPBase*>(handle->data);
	traceL("UDPBase", socket) << "Handle recv: " << nread << endl;
			
	if (nread < 0) {
		assert(0 && "unexpected error");
		socket->setUVError("End of file", UV_EOF);
		return;
	}

	if (nread == 0) {
		assert(addr == NULL);
		socket->setUVError("End of file", UV_EOF);
		return;
	}
	
	// EOF or Error
	//if (nread == -1)
	//	socket->setUVError(r);
	
	//socket->_buffer.data()
	socket->onRecv(mutableBuffer(buf->base, nread), net::Address(addr, sizeof *addr));
}


void UDPBase::afterSend(uv_udp_send_t* req, int status) 
{
	auto sr = reinterpret_cast<internal::SendRequest*>(req);
	auto socket = reinterpret_cast<UDPBase*>(sr->req.handle->data);	
	if (status) 
		socket->setUVError("Stream send error", status);
	delete sr;
}


void UDPBase::allocRecvBuffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	auto self = static_cast<UDPBase*>(handle->data);	
	//traceL("UDPBase", self) << "Allocating Buffer: " << suggested_size << endl;	
	
	// Reserve the recommended buffer size
	// XXX: libuv wants us to allocate 65536 bytes for UDP .. hmmm
	//if (suggested_size > self->_buffer.available())
	//	self->_buffer.reserve(suggested_size); 
	//assert(self->_buffer.capacity() >= suggested_size);
	assert(self->_buffer.size() >= suggested_size);

	// Reset the buffer position on each read
	//self->_buffer.position(0);
	buf->base = self->_buffer.data();
	buf->len = self->_buffer.size();

	//return uv_buf_init(self->_buffer.data(), suggested_size);
}


void UDPBase::onError(const Error& error) 
{		
	errorL("UDPBase", this) << "Error: " << error.message << endl;	
	emitError(error);
	close(); // close on error
}


void UDPBase::onClose() 
{		
	errorL("UDPBase", this) << "On close" << endl;	
	emitClose();
}


uv::Loop* UDPBase::loop() const
{
	return uv::Handle::loop();
}




} } // namespace scy::net