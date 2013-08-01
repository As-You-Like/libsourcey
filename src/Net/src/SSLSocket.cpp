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


#include "Sourcey/Net/SSLSocket.h"
#include "Sourcey/Net/SSLManager.h"
#include "Sourcey/Logger.h"


using namespace std;


namespace scy {
namespace net {


SSLSocket::SSLSocket() : 
	net::Socket(new SSLBase, false)
{
}


SSLSocket::SSLSocket(SSLBase* base, bool shared) : 
	net::Socket(base, shared) 
{
}


SSLSocket::SSLSocket(const Socket& socket) : 
	net::Socket(socket)
{
	if (!dynamic_cast<SSLBase*>(_base))
		throw Exception("Cannot assign incompatible socket");
}
	

SSLBase& SSLSocket::base() const
{
	return static_cast<SSLBase&>(*_base);
}


// -------------------------------------------------------------------
//
SSLBase::SSLBase() : 
	// TODO: Using client context, should assert no bind()/listen() on this socket
	_context(SSLManager::instance().defaultClientContext()), 
	_session(nil), 
	_sslAdapter(this)
{
	traceL("SSLBase", this) << "Creating" << endl;
	assert(_handle);
	_handle->data = this;
	_connectReq.data = this;
}


SSLBase::SSLBase(SSLContext::Ptr context) : 
	_context(context), 
	_session(nil), 
	_sslAdapter(this)
{
	traceL("SSLBase", this) << "Creating" << endl;
	assert(_handle);
	_handle->data = this;
	_connectReq.data = this;
}
	

SSLBase::SSLBase(SSLContext::Ptr context, SSLSession::Ptr session) : 
	_context(context), 
	_session(session), 
	_sslAdapter(this)
{
	traceL("SSLBase", this) << "Creating" << endl;
	assert(_handle);
	_handle->data = this;
	_connectReq.data = this;
}

	
SSLBase::~SSLBase() 
{	
	traceL("SSLBase", this) << "Destroying" << endl;
}


int SSLBase::available() const
{
	assert(initialized());
	return _sslAdapter.available();
}


void SSLBase::close()
{
	traceL("SSLBase", this) << "Close" << endl;
	TCPBase::close();
}


bool SSLBase::shutdown()
{
	traceL("SSLBase", this) << "Shutdown" << endl;
	try {
		// Try to gracefully shutdown the SSL connection
		_sslAdapter.shutdown();
	}
	catch (...) {}
	return TCPBase::shutdown();
}


int SSLBase::send(const char* data, int len, int flags) 
{	
	return send(data, len, peerAddress(), flags);
}


int SSLBase::send(const char* data, int len, const net::Address& peerAddress, int flags) 
{	
	assert(len <= net::MAX_TCP_PACKET_SIZE);	
	assert(initialized());
	traceL("SSLBase", this) << "Send: " << len << endl;
	
	// Send unencrypted data to the SSL context
	_sslAdapter.addOutgoingData(data, len);
	_sslAdapter.flush();
	return len;
}


SSLSession::Ptr SSLBase::currentSession()
{
	if (_sslAdapter._ssl) {
		SSL_SESSION* session = SSL_get1_session(_sslAdapter._ssl);
		if (session) {
			if (_session && session == _session->sslSession()) {
				SSL_SESSION_free(session);
				return _session;
			}
			else return new SSLSession(session);
		}
	}
	return 0;
}

	
void SSLBase::useSession(SSLSession::Ptr session)
{
	_session = session;
}


bool SSLBase::sessionWasReused()
{
	if (_sslAdapter._ssl)
		return SSL_session_reused(_sslAdapter._ssl) != 0;
	else
		return false;
}


//
// Callbacks
// 

void SSLBase::onRead(const char* data, int len)
{
	traceL("SSLBase", this) << "On SSL read: " << len << endl;

	// SSL encrypted data is sent to the SSL conetext
	_sslAdapter.addIncomingData(data, len);
	_sslAdapter.flush();
}


void SSLBase::onConnect(int status)
{
	traceL("SSLBase", this) << "On connect" << endl;
	if (status) {
		setLastError();
		return;
	}
	else
		readStart();
 
	SSL* ssl = SSL_new(_context->sslContext());

	// TODO: Automatic SSL session handling.
	// Maybe add a stored session to the network manager.
	if (_session)
		SSL_set_session(ssl, _session->sslSession());
 
	SSL_set_connect_state(ssl);
	SSL_do_handshake(ssl);
 
	_sslAdapter.init(ssl);
	_sslAdapter.flush();

	SocketBase::emitConnect();
	traceL("SSLBase", this) << "On connect: OK" << endl;
}


} } // namespace scy::net





/*
void SSLBase::connect(const Address& peerAddress) 
{
	if (!_context) 
		throw Exception("Cannot connect without SSL context");

	traceL("SSLBase", this) << "Connecting to " << peerAddress << endl;
	init();
	const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(peerAddress.addr());
	assert(_connectReq.data == this);
	int r = uv_tcp_connect(&_connectReq, (uv_tcp_t*)stream(), *addr, uv::onConnected);
	if (r) {
		uv_err_t err = uv_last_error(loop());
		setError(err);
		throw Exception(uv_strerror(err)); // TODO: make exception setError option
	}
}
*/