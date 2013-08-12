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


#include "Sourcey/Net/TCPBase.h"
#include "Sourcey/Net/TCPSocket.h"
#include "Sourcey/Logger.h"


using namespace std;


namespace scy {
namespace net {


TCPBase::TCPBase(uv::Loop& loop) :
	Stream(loop) //, new uv_tcp_t
{
	traceL("TCPBase", this) << "Creating" << endl;
	init();	
	//_handle->data = this;
	//uv_tcp_init(&loop, handle<uv_tcp_t>());	
	//traceL("TCPBase", this) << "Creating: OK" << endl;
}

	
TCPBase::~TCPBase() 
{	
	traceL("TCPBase", this) << "Destroying" << endl;	
}


void TCPBase::init()
{
	if (closed()) {		
		traceL("TCPBase", this) << "Init" << endl;
		uv_tcp_t* tcp = new uv_tcp_t;
		tcp->data = this;
		_handle = reinterpret_cast<uv_handle_t*>(tcp);
		uv_tcp_init(loop(), tcp);
		//assert(tcp->data == this);		
		//_connectReq.data = instance();	
		traceL("TCPBase", this) << "Init: OK" << endl;
	}
}


namespace internal {

	UVStatusCallback(TCPBase, onConnect, uv_connect_t);
	UVStatusCallbackWithType(TCPBase, onAcceptConnection, uv_stream_t);

}


void TCPBase::connect(const net::Address& peerAddress) 
{
	traceL("TCPBase", this) << "Connecting to " << peerAddress << endl;
	init();
	assert(_connectReq.data != this);
	_connectReq.data = this;	
	const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(peerAddress.addr());
	int r = uv_tcp_connect(&_connectReq, handle<uv_tcp_t>(), *addr, internal::onConnect);
	if (r) setAndThrowLastError("TCP connect failed");
}


void TCPBase::bind(const net::Address& address, unsigned flags) 
{
	traceL("TCPBase", this) << "Binding on " << address << endl;
	init();
	int r;
	const sockaddr_in& addr = reinterpret_cast<const sockaddr_in&>(*address.addr());
	switch (address.af()) {
	case AF_INET:
		r = uv_tcp_bind(handle<uv_tcp_t>(), *reinterpret_cast<const sockaddr_in*>(address.addr()));
		break;
	case AF_INET6:
		r = uv_tcp_bind6(handle<uv_tcp_t>(), *reinterpret_cast<const sockaddr_in6*>(address.addr()));
		break;
	default:
		throw Exception("Unexpected address family");
	}
	if (r) setAndThrowLastError("TCP bind failed");
}


void TCPBase::listen(int backlog) 
{
	traceL("TCPBase", this) << "Listening" << endl;
	init();
	int r = uv_listen(handle<uv_stream_t>(), backlog, internal::onAcceptConnection);
	if (r) setAndThrowLastError("TCP listen failed");
}


bool TCPBase::shutdown()
{
	traceL("TCPBase", this) << "Shutdown" << endl;
	return Stream::shutdown();
}


void TCPBase::close()
{
	traceL("TCPBase", this) << "Close" << endl;
	Stream::close();
}


void TCPBase::setNoDelay(bool enable) 
{
	init();
	int r = uv_tcp_nodelay(handle<uv_tcp_t>(), enable ? 1 : 0);
	if (r) setLastError();
}


void TCPBase::setKeepAlive(int enable, unsigned int delay) 
{
	init();
	int r = uv_tcp_keepalive(handle<uv_tcp_t>(), enable, delay);
	if (r) setLastError();
}


#ifdef _WIN32
void TCPBase::setSimultaneousAccepts(bool enable) 
{
	init();
	int r = uv_tcp_simultaneous_accepts(handle<uv_tcp_t>(), enable ? 1 : 0);
	if (r) setLastError();
}
#endif


int TCPBase::send(const char* data, int len, int flags) 
{	
	return send(data, len, peerAddress(), flags);
}


int TCPBase::send(const char* data, int len, const net::Address& peerAddress, int flags) 
{
	//if (len < 200)
	//	traceL("TCPBase", this) << "Send: " << string(data, len) << endl;

	//assert(peerAddress == this->peerAddress());
	//assert(initialized());
	assert(len <= net::MAX_TCP_PACKET_SIZE);
	
	//_debugOutput.append(data, len);
	//if (_debugOutput.length() > 200)
	//	traceL("TCPBase", this) << "Sending: " << string(_debugOutput.c_str(), 200) << std::endl;
	
	if (!Stream::write(data, len)) {
		warnL("TCPBase", this) << "Send error" << endl;	
		return -1;
	}

	// R is -1 on error, otherwise return len
	return len;
}


void TCPBase::acceptConnection()
{
	// Create the socket on the stack.
	// If the socket is not handled it will be destroyed.
	net::TCPSocket socket;
	traceL("TCPBase", this) << "Accept connection: " << socket.base().handle() << endl;
	int r = uv_accept(handle<uv_stream_t>(), socket.base().handle<uv_stream_t>());
	assert(r == 0); // uv_accept should always work
	socket.base().readStart();		
	AcceptConnection.emit(instance(), socket);
	if (socket.base().refCount() == 1)
		traceL("TCPBase", this) << "Accept connection: Not handled" << endl;
}


net::Address TCPBase::address() const
{
	//traceL("TCPBase", this) << "Get address: " << closed() << endl;

	if (closed())
		return net::Address();
		//throw Exception("Invalid TCP socket: No address");
	
	struct sockaddr_storage address;
	int addrlen = sizeof(address);
	int r = uv_tcp_getsockname(handle<uv_tcp_t>(),
								reinterpret_cast<sockaddr*>(&address),
								&addrlen);
	if (r)
		return net::Address();
		//throwLastError("Invalid TCP socket: No address");

	return net::Address(reinterpret_cast<const sockaddr*>(&address), addrlen);
}


net::Address TCPBase::peerAddress() const
{
	//traceL("TCPBase", this) << "Get peer address: " << closed() << endl;
	if (closed())
		return net::Address();
		//throw Exception("Invalid TCP socket: No peer address");

	struct sockaddr_storage address;
	int addrlen = sizeof(address);
	int r = uv_tcp_getpeername(handle<uv_tcp_t>(),
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
	

bool TCPBase::closed() const
{
	return uv::Handle::closed();
}


SOCKET TCPBase::sockfd() const
{
	return closed() ? INVALID_SOCKET : handle<uv_tcp_t>()->socket;
}


bool TCPBase::initialized() const
{
	return sockfd() != INVALID_SOCKET;
}


//
// Callbacks
//

void TCPBase::onRead(const char* data, int len)
{
	traceL("TCPBase", this) << "On read: " << len << endl;
	_buffer.position(0);
	_buffer.limit(len);
	onRecv(_buffer);
}


void TCPBase::onRecv(Buffer& buf)
{
	traceL("TCPBase", this) << "On recv: " << buf.available() << endl;
	emitRecv(buf, peerAddress());
}


void TCPBase::onConnect(int status)
{
	traceL("TCPBase", this) << "On connect" << endl;
	
	// Error handled by static callback proxy
	if (status == 0) {
		if (readStart())
			emitConnect();
	}
	else
		errorL("TCPBase", this) << "Connection failed: " << error().message << endl;
}


void TCPBase::onAcceptConnection(uv_stream_t* handle, int status) 
{		
	if (status == 0) {
		traceL("TCPBase", this) << "On accept connection" << endl;
		acceptConnection();
	}
	else
		errorL("TCPBase", this) << "Accept connection failed" << endl;
}


void TCPBase::onError(const Error& error) 
{		
	errorL("TCPBase", this) << "On error: " << error.message << endl;	
	emitError(error);
	close(); // close on error
}


void TCPBase::onClose() 
{		
	errorL("TCPBase", this) << "On close" << endl;	
	emitClose();
}


} } // namespace scy::net