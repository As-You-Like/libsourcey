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


#include "Sourcey/HTTP/Connection.h"
#include "Sourcey/HTTP/Server.h"
#include "Sourcey/HTTP/Client.h"
#include "Sourcey/Logger.h"

#include <assert.h>


using namespace std;


namespace scy { 
namespace http {


Connection::Connection(const net::Socket& socket) : 
	_socket(socket), 
	_closed(false),
	_shouldSendHeader(true),
	_timeout(30 * 60 * 1000) // 30 secs
{	
	traceL("Connection", this) << "create: " << &_socket << endl;

	_socket.Recv += delegate(this, &Connection::onSocketRecv);
	_socket.Error += delegate(this, &Connection::onSocketError);
	_socket.Close += delegate(this, &Connection::onSocketClose);

	// Attach the outgoing stream to the socket
	Outgoing.emitter() += delegate(&_socket, &net::Socket::send);
}

	
Connection::~Connection() 
{	
	traceL("Connection", this) << "destroy" << endl;

	// Connections must be close()d
	assert(_closed);
}


int Connection::sendData(const char* buf, size_t len, int flags)
{
	traceL("Connection", this) << "send: " << len << endl;
	return _socket.send(buf, len, flags);
}


int Connection::sendData(const std::string& buf, int flags)
{
	traceL("Connection", this) << "send: " << buf.length() << endl;
	return _socket.send(buf.c_str(), buf.length(), flags);
}


int Connection::sendHeader()
{
	if (!_shouldSendHeader)
		return 0;
	assert(outgoingHeader());
	
	ostringstream os;
	outgoingHeader()->write(os);
	std::string head(os.str().c_str(), os.str().length());

	_timeout.start();
	_shouldSendHeader = false;
	
	traceL("Connection", this) << "send header: " << head << endl; // remove me
	return _socket.base().send(head.c_str(), head.length());
}


void Connection::close()
{
	traceL("Connection", this) << "close" << endl;	
	assert(!_closed);
	assert(_socket.base().refCount() == 1);
	
	_closed = true;	

	Outgoing.emitter() -= delegate(&_socket, &net::Socket::send);	
	Outgoing.close();
	Incoming.close();

	_socket.Recv -= delegate(this, &Connection::onSocketRecv);
	_socket.Error -= delegate(this, &Connection::onSocketError);
	_socket.Close -= delegate(this, &Connection::onSocketClose);
	_socket.close();

	onClose();
		
	traceL("Connection", this) << "close: Deleting" << endl;	
	//delete this;
	deleteLater<Connection>(this); // destroy it
}


void Connection::setError(const Error& err) 
{ 
	traceL("Connection", this) << "set error: " << err.message << endl;	
	_socket.setError(err);
	//_error = err;
}


void Connection::onClose()
{
	traceL("Connection", this) << "on close" << endl;	

	Close.emit(this);
}


void Connection::onSocketRecv(void*, net::SocketPacket& packet)
{		
	_timeout.stop();
			
	if (Incoming.emitter().refCount()) {
		//RawPacket p(packet.data(), packet.size());
		//Incoming.write(p);
		Incoming.write(packet.data(), packet.size());
	}

	// Handle payload data
	onPayload(mutableBuffer(packet.data(), packet.size()));
}


void Connection::onSocketError(void*, const Error& error) 
{
	traceL("Connection", this) << "on socket error" << endl;

	// Handle the socket error locally
	setError(error);
}


void Connection::onSocketClose(void*) 
{
	traceL("Connection", this) << "on socket close" << endl;

	// Close the connection if the socket is closed
	close();
}


Request& Connection::request()
{
	return _request;
}

	
Response& Connection::response()
{
	return _response;
}

	
net::Socket& Connection::socket()
{
	return _socket;
}
	

Buffer& Connection::incomingBuffer()
{
	return static_cast<net::TCPBase&>(_socket.base()).buffer();
}

	
bool Connection::closed() const
{
	return _closed;
}

	
bool Connection::shouldSendHeader() const
{
	return _shouldSendHeader;
}


void Connection::shouldSendHeader(bool flag)
{
	_shouldSendHeader = flag;
}

	
bool Connection::expired() const
{
	return _timeout.expired();
}


//
// HTTP Client Connection Adapter
//


ConnectionAdapter::ConnectionAdapter(Connection& connection, http_parser_type type) : 
	_connection(connection),
	_parser(type)
{	
	traceL("ConnectionAdapter", this) << "create: " << &connection << endl;
	_parser.setObserver(this);
	if (type == HTTP_REQUEST)
		_parser.setRequest(&connection.request());
	else
		_parser.setResponse(&connection.response());
}


ConnectionAdapter::~ConnectionAdapter()
{
	traceL("ConnectionAdapter", this) << "destroy: " << &_connection << endl;
}


int ConnectionAdapter::send(const char* data, int len, int flags)
{
	traceL("ConnectionAdapter", this) << "send: " << len << endl;
	
	try
	{
		// Send headers on initial send
		if (_connection.shouldSendHeader()) {
			int res = _connection.sendHeader();

			// The initial packet may be empty to 
			// push the headers through
			if (len == 0)
				return res;
		}

		// Other packets should not be empty
		assert(len > 0);

		// Send body / chunk
		//if (len < 300)
		//	traceL("ConnectionAdapter", this) << "send data: " << std::string(data, len) << endl;
		//if (len > 300)
		//	traceL("ConnectionAdapter", this) << "send long data: " << std::string(data, 300) << endl;
		return socket->base().send(data, len, flags);
	} 
	catch (std::exception&/*Exception&*/ exc) 
	{
		errorL("ConnectionAdapter", this) << "Send error: " << exc.what()/*message()*/ << endl;

		// Swallow the exception, the socket error will 
		// cause the connection to close on next iteration.
	}
	
	return -1;
}


void ConnectionAdapter::onSocketRecv(const MutableBuffer& buf, const net::Address& /* peerAddr */)
{
	traceL("ConnectionAdapter", this) << "on socket recv: " << buf.size() << endl;	
	
	try {
		// Parse incoming HTTP messages
		_parser.parse(bufferCast<const char *>(buf), buf.size());
	} 
	catch (std::exception&/*Exception&*/ exc) {
		errorL("ConnectionAdapter", this) << "HTTP parser error: " << exc.what()/*message()*/ << endl;

		if (socket)
			socket->close();
	}	
}


//
// Parser callbacks
//

void ConnectionAdapter::onParserHeader(const std::string& /* name */, const std::string& /* value */) 
{
}


void ConnectionAdapter::onParserHeadersEnd() 
{
	traceL("ConnectionAdapter", this) << "on headers end" << endl;	

	_connection.onHeaders();	

	// Set the position to the end of the headers once
	// they have been handled. Subsequent body chunks will
	// now start at the correct position.
	//_connection.incomingBuffer().position(_parser._parser.nread); // should be redundant
}


void ConnectionAdapter::onParserChunk(const char* buf, size_t len)
{
	traceL("ClientConnection", this) << "on parser chunk: " << len << endl;	

	// Dispatch the payload
	net::SocketAdapter::onSocketRecv(mutableBuffer(const_cast<char*>(buf), len), socket->peerAddress());
}


void ConnectionAdapter::onParserError(const ParserError& err)
{
	warnL("ConnectionAdapter", this) << "on parser error: " << err.message << endl;	

	// Close the connection on parser error
	_connection.setError(err.message);
	//_connection.close();
}


void ConnectionAdapter::onParserEnd()
{
	traceL("ConnectionAdapter", this) << "on parser end" << endl;	

	_connection.onMessage();
}

	
Parser& ConnectionAdapter::parser()
{
	return _parser;
}


Connection& ConnectionAdapter::connection()
{
	return _connection;
}


} } // namespace scy::http