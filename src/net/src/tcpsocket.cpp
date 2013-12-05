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

#include "scy/net/tcpsocket.h"
#include "scy/net/tcpsocket.h"
#include "scy/logger.h"
//#if POSIX
//#include <sys/socket.h>
//#endif


using std::endl;


namespace scy {
namespace net {


TCPSocket::TCPSocket() : 
	net::Socket(new TCPBase, false)
{	
	TraceLS(this) << "Create" << endl;	
}


TCPSocket::TCPSocket(TCPBase* base, bool shared) : 
	net::Socket(base, shared) 
{
	TraceLS(this) << "Destroy" << endl;	
}


TCPSocket::TCPSocket(const Socket& socket) : 
	net::Socket(socket)
{
	if (!dynamic_cast<TCPBase*>(_base))
		throw std::runtime_error("Cannot assign incompatible socket");
}
	

TCPBase& TCPSocket::base() const
{
	return static_cast<TCPBase&>(*_base);
}


//
// TCP Base
//


TCPBase::TCPBase(uv::Loop* loop) :
	Stream(loop)
{
	TraceLS(this) << "Create" << endl;
	init();	
}

	
TCPBase::~TCPBase() 
{	
	TraceLS(this) << "Destroy" << endl;	
}


void TCPBase::init()
{
	if (ptr()) return;

	TraceLS(this) << "Init" << endl;
	auto tcp = new uv_tcp_t;
	tcp->data = this;
	_ptr = reinterpret_cast<uv_handle_t*>(tcp);
	_closed = false;
	int r = uv_tcp_init(loop(), tcp);
	if (r)
		setUVError("Cannot initialize TCP socket", r);
}


namespace internal {

	UVStatusCallback(TCPBase, onConnect, uv_connect_t);
	UVStatusCallbackWithType(TCPBase, onAcceptConnection, uv_stream_t);

}


void TCPBase::connect(const net::Address& peerAddress) 
{
	TraceLS(this) << "Connecting to " << peerAddress << endl;
	init();
	_connectReq.reset(new uv_connect_t);
	_connectReq.get()->data = this;
	//assert(_connectReq.data != this);
	//_connectReq.data = this;	
	//auto addr = reinterpret_cast<const sockaddr_in*>(peerAddress.addr());
	int r = uv_tcp_connect(_connectReq.get(), ptr<uv_tcp_t>(), peerAddress.addr(), internal::onConnect);
	if (r) setAndThrowError("TCP connect failed", r);
}


void TCPBase::bind(const net::Address& address, unsigned /* flags */) 
{
	TraceLS(this) << "Binding on " << address << endl;
	init();
	int r;
	switch (address.af()) {
	case AF_INET:
		r = uv_tcp_bind(ptr<uv_tcp_t>(), address.addr());
		break;
	//case AF_INET6:
	//	r = uv_tcp_bind6(ptr<uv_tcp_t>(), *reinterpret_cast<const sockaddr_in6*>(address.addr()));
	//	break;
	default:
		throw std::runtime_error("Unexpected address family");
	}
	if (r) setAndThrowError("TCP bind failed", r);
}


void TCPBase::listen(int backlog) 
{
	TraceLS(this) << "Listening" << endl;
	init();
	int r = uv_listen(ptr<uv_stream_t>(), backlog, internal::onAcceptConnection);
	if (r) setAndThrowError("TCP listen failed", r);
}


bool TCPBase::shutdown()
{
	TraceLS(this) << "Shutdown" << endl;
	return Stream::shutdown();
}


void TCPBase::close()
{
	TraceLS(this) << "Close" << endl;
	Stream::close();
}


void TCPBase::setNoDelay(bool enable) 
{
	init();
	int r = uv_tcp_nodelay(ptr<uv_tcp_t>(), enable ? 1 : 0);
	if (r) setUVError("TCP socket error", r);
}


void TCPBase::setKeepAlive(int enable, unsigned int delay) 
{
	init();
	int r = uv_tcp_keepalive(ptr<uv_tcp_t>(), enable, delay);
	if (r) setUVError("TCP socket error", r);
}


#ifdef _WIN32
void TCPBase::setSimultaneousAccepts(bool enable) 
{
	init();
	int r = uv_tcp_simultaneous_accepts(ptr<uv_tcp_t>(), enable ? 1 : 0);
	if (r) setUVError("TCP socket error", r);
}
#endif


int TCPBase::send(const char* data, int len, int flags) 
{	
	return send(data, len, peerAddress(), flags);
}


int TCPBase::send(const char* data, int len, const net::Address& /* peerAddress */, int /* flags */) 
{
	//assert(len <= net::MAX_TCP_PACKET_SIZE); // libuv handles this for us
	
	TraceLS(this) << "Send: " << len << endl;	
	assert(Thread::currentID() == tid());

	//if (len < 300)
	//	TraceLS(this) << "Send: " << len << ": " << std::string(data, len) << endl;
	//else
	//	TraceLS(this) << "Send long: " << len << ": " << std::string(data, 300) << endl;

	if (!Stream::write(data, len)) {
		WarnL << "Send error" << endl;	
		return -1;
	}

	// R is -1 on error, otherwise return len
	// TODO: Return native error code?
	return len;
}


void TCPBase::acceptConnection()
{
	// Create the socket on the stack.
	// If the socket is not handled it will be destroyed.
	net::TCPSocket socket;
	TraceLS(this) << "Accept connection: " << socket.base().ptr() << endl;
	uv_accept(ptr<uv_stream_t>(), socket.base().ptr<uv_stream_t>()); // uv_accept should always work
	socket.base().readStart();		
	AcceptConnection.emit(instance(), socket);
	if (socket.base().refCount() == 1)
		WarnL << "Accepted connection not handled" << endl;
}


net::Address TCPBase::address() const
{
	//TraceLS(this) << "Get address: " << closed() << endl;

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


net::Address TCPBase::peerAddress() const
{
	//TraceLS(this) << "Get peer address: " << closed() << endl;
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


void TCPBase::setError(const Error& err)
{
	Stream::setError(err);
}

		
const Error& TCPBase::error() const
{
	return Stream::error();
}


net::TransportType TCPBase::transport() const 
{ 
	return net::TCP; 
}
	

//bool TCPBase::initialized() const
//{
//	return Stream::initialized(); //uv::Handle::closed();
//}
	

bool TCPBase::closed() const
{
	return Stream::closed(); //uv::Handle::closed();
}


uv::Loop* TCPBase::loop() const
{
	return uv::Handle::loop();
}


/*
SOCKET TCPBase::sockfd() const
{
	return closed() ? INVALID_SOCKET : ptr<uv_tcp_t>()->socket;
}


bool TCPBase::initialized() const
{
	return !closed();
	//return sockfd() != INVALID_SOCKET;
}
*/


//
// Callbacks
//

void TCPBase::onRead(const char* data, int len)
{
	TraceLS(this) << "On read: " << len << endl;
	
	//if (len < 300)
	//	TraceLS(this) << "Received: " << std::string(data, len) << endl;
	//_buffer.position(0);
	//_buffer.limit(len);
	//onRecv(mutableBuffer(_buffer.data(), len));

	// Note: The const_cast here is relatively safe since the given 
	// data pointer is the underlying _buffer.data() pointer, but
	// a better way should be devised.
	onRecv(mutableBuffer(const_cast<char*>(data), len));
}


void TCPBase::onRecv(const MutableBuffer& buf)
{
	TraceLS(this) << "Recv: " << buf.size() << endl;
	emitRecv(buf, peerAddress()); //MutableBuffer(buf)
}


void TCPBase::onConnect(int status)
{
	TraceLS(this) << "On connect" << endl;
	
	// Error handled by static callback proxy
	if (status == 0) {
		if (readStart())
			emitConnect();
	}
	else
		ErrorLS(this) << "Connection failed: " << error().message << endl;
}


void TCPBase::onAcceptConnection(uv_stream_t*, int status) 
{		
	if (status == 0) {
		TraceLS(this) << "On accept connection" << endl;
		acceptConnection();
	}
	else
		ErrorLS(this) << "Accept connection failed" << endl;
}


void TCPBase::onError(const Error& error) 
{		
	ErrorLS(this) << "Error: " << error.message << endl;	
	emitError(error);
	close(); // close on error
}


void TCPBase::onClose() 
{		
	TraceLS(this) << "On close" << endl;	
	emitClose();
}


} } // namespace scy::net
