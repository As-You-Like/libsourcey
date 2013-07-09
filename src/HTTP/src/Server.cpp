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



using namespace std;


#include "Sourcey/HTTP/Server.h"
#include "Sourcey/HTTP/WebSocket.h"
#include "Sourcey/Logger.h"
#include "Sourcey/Util.h"


namespace scy { 
namespace http {

	
Server::Server(short port, ServerResponderFactory* factory) :
	address("0.0.0.0", port),
	factory(factory)
{
	traceL("Server", this) << "Creating" << endl;
}


Server::~Server()
{
	traceL("Server", this) << "Destroying" << endl;
	shutdown();
}

	
void Server::start()
{	
	assert(socket.base().refCount() == 1);

	// TODO: Register self as an observer
	socket.base().AcceptConnection += delegate(this, &Server::onAccept);	
	socket.Close += delegate(this, &Server::onClose);
	socket.bind(address);
	socket.listen();

	traceL("Server", this) << "Server listening on " << port() << endl;		

	//timer.Timeout += delegate(this, &Server::onTimer);
	//timer.start(5000, 5000);
}


void Server::shutdown() 
{		
	traceL("Server", this) << "Shutdown" << endl;

	socket.base().AcceptConnection -= delegate(this, &Server::onAccept);	
	socket.Close -= delegate(this, &Server::onClose);
	socket.close();

	//timer.stop();
	if (!connections.empty())
		Shutdown.emit(this);

	// Connections must remove themselves
	assert(connections.empty());
	assert(socket.base().refCount() == 1);
}


UInt16 Server::port()
{
	return socket.address().port();
}	


ServerConnection* Server::createConnection(const net::Socket& sock)
{
	return new ServerConnection(*this, sock);
}


ServerResponder* Server::createResponder(ServerConnection& conn)
{
	// The initial HTTP request headers have already
	// been parsed by now, but the request body may 
	// be incomplete (especially if chunked).
	return factory->createResponder(conn);
}


void Server::addConnection(ServerConnection* conn) 
{		
	traceL("Server", this) << "Adding Connection: " << conn << endl;
	connections.push_back(conn);
}


void Server::removeConnection(ServerConnection* conn) 
{		
	traceL("Server", this) << "Removing Connection: " << conn << endl;
	for (ServerConnectionList::iterator it = connections.begin(); it != connections.end(); ++it) {
		if (conn == *it) {
			connections.erase(it);
			return;
		}
	}
	assert(0 && "unknown connection");
}


void Server::onAccept(void* sender, const net::TCPSocket& sock)
{	
	traceL("Server", this) << "On server accept" << endl;
	ServerConnection* conn = createConnection(sock);
	assert(conn);
}


void Server::onClose(void* sender) 
{
	traceL("Server", this) << "On server socket close" << endl;
}


// -------------------------------------------------------------------
// Server Connection
//
ServerConnection::ServerConnection(Server& server, const net::Socket& socket) : 
	Connection(socket), 
	_server(server), 
	_responder(NULL)
{	
	traceL("ServerConnection", this) << "Creating" << endl;

	setAdapter(new ServerAdapter(*this));
	server.Shutdown += delegate(this, &ServerConnection::onServerShutdown);
	server.addConnection(this);
}

	
ServerConnection::~ServerConnection() 
{	
	traceL("ServerConnection", this) << "Destroying" << endl;

	if (_responder)
		delete _responder;
}

	
void ServerConnection::close()
{
	_server.Shutdown -= delegate(this, &ServerConnection::onServerShutdown);
	_server.removeConnection(this);

	Connection::close(); // close and destroy
}


void ServerConnection::onServerShutdown(void*)
{
	close();
}


Poco::Net::HTTPMessage* ServerConnection::incomingHeaders() 
{ 
	return static_cast<Poco::Net::HTTPMessage*>(_request);
}


Poco::Net::HTTPMessage* ServerConnection::outgoingHeaders() 
{ 
	return static_cast<Poco::Net::HTTPMessage*>(_response);
}


bool ServerConnection::send()
{
	traceL("ServerConnection", this) << "Respond" << endl;

	// TODO: Detect end of message and close()

	// KLUDGE: Temp solution for quick sending small requests only.
	// Use Connection::sendRaw() for nocopy binary stream.
	string body(_response->body.str());
	_response->setContentLength(body.length());
	return sendRaw(body.data(), body.length());
}

			
Server& ServerConnection::server()
{
	return _server;
}
	

//
// Connection callbacks
//

void ServerConnection::onHeaders() 
{
	traceL("ServerConnection", this) << "On headers" << endl;	
	
	// Upgrade the connection if required
	if (_request->hasToken("Connection", "upgrade") && 
		Poco::icompare(_request->get("Upgrade", ""), "websocket") == 0)
	{			
		traceL("ServerConnection", this) << "Upgrading to WebSocket" << endl;

		WebSocketServerAdapter* wsAdapter = new WebSocketServerAdapter;
		socket().setAdapter(wsAdapter);

		ostringstream oss;
		_request->write(oss);
		Buffer buffer(oss.str().data(), oss.str().length());		

		// Send the handshake request to the WS adapter for handling.
		// If the request fails the connection socket will be closed
		// resulting in destruction.
		socket().adapter().onSocketRecv(buffer, socket().peerAddress());

		// TODO: Need to handle error here?
	}

	// When headers have been parsed we instantiate the request handler
	_responder = _server.createResponder(*this);
	assert(_responder);
	_responder->onHeaders(*_request);
}


void ServerConnection::onPayload(Buffer& buffer)
{
	traceL("ServerConnection", this) << "On payload: " << buffer.size() << endl;	

	assert(_responder);
	_responder->onPayload(buffer);
}


void ServerConnection::onComplete() 
{
	traceL("ServerConnection", this) << "On complete" << endl;	

	// The HTTP request is complete.
	// The request handler can give a response.
	assert(_responder);
	_responder->onRequest(*_request, *_response);
}


void ServerConnection::onClose() 
{
	if (_responder)
		_responder->onClose();

	Connection::onClose();
}



//
// Parser callbacks
//

/*
void ServerConnection::onParserHeadersEnd() 
{
	// When headers have been parsed we instantiate the request handler
	_responder = _server.createResponder(*this);
	assert(_responder);
	_responder->onHeaders(*_request);
}


void ServerConnection::onParserChunk(const char* buf, size_t len)
{
	traceL("ServerConnection", this) << "On Parser Chunk" << endl;	
}


void ServerConnection::onParserEnd() 
{
	traceL("ServerConnection", this) << "On Request Complete" << endl;	

	//traceL("ServerConnection", this) << "On Message Complete 1" << endl;	
	//traceL("ServerConnection", this) << "On Message Complete 2" << endl;
	//traceL("ServerConnection", this) << "On Message Complete 2: " << _request->getKeepAlive() << endl;	
	//traceL("ServerConnection", this) << "On Message Complete 2: " << _response->getKeepAlive() << endl;
}
*/


} } // namespace scy::http





	

	/*
	

	bool res = sendRaw(body.data(), body.length());

	// Set Connection: Close unless otherwise stated
	if (!isExplicitKeepAlive(_request) || 
		_response->hasContentLength()) {
		traceL("ServerConnection", this) << "Respond: No keepalive" << endl;	
		_response->setKeepAlive(false);
	}
	*/
	/* 	
	// KLUDGE: Temp solution for quick sending small requests only.
	// Use Connection::sendRaw() for nocopy binary stream.
	bool res = sendRaw(
		_response->body.str().data(),
		_response->body.str().length());
	
	// Close unless keepalive is set
	if (!_response->getKeepAlive()) {
		traceL("Client", this) << "Closing: No keepalive" << endl; 
		close();
	}	
	*/






	/*

Server::Server(Poco::Net::HTTPServerResponderFactory::Ptr pFactory, const Poco::Net::ServerSocket& socket, Poco::Net::HTTPServerParams::Ptr pParams):
	Poco::Net::TCPServer(new ServerConnectionFactory(this, pParams, pFactory), socket, pParams),
	_pFactory(pFactory)
{
}


Server::Server(Poco::Net::HTTPServerResponderFactory::Ptr pFactory, Poco::ThreadPool& threadPool, 
		const Poco::Net::ServerSocket& socket, Poco::Net::HTTPServerParams::Ptr pParams):
	Poco::Net::TCPServer(new ServerConnectionFactory(this, pParams, pFactory), threadPool, socket, pParams),
	_pFactory(pFactory)
{
}


Server::~Server()
{
	{		
		Poco::FastMutex::ScopedLock lock(_mutex);
		scy::util::ClearVector(_connectionHooks);
	}
}


void Server::addConnectionHook(ServerConnectionHook* hook)
{	
	Poco::FastMutex::ScopedLock lock(_mutex);
	_connectionHooks.push_back(hook);
}

	
ServerConnectionHookVec Server::connectionHooks() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	return _connectionHooks;
}


Poco::Net::TCPServerConnection* Server::handleSocketConnection(const Poco::Net::StreamSocket& socket)
{	
	Poco::Net::TCPServerConnection* conn = NULL;


	// TODO: How does POCO handle these half-open requests
	try 
	{
		ServerConnectionHookVec hooks = connectionHooks();
		if (!hooks.empty()) {
			traceL("HTTPServer") << "Reading Request" << endl;
			Poco::Net::StreamSocket ss(socket);
			char buffer[4096];
			ss.setReceiveTimeout(Poco::Timespan(2,0));
			int size = ss.receiveBytes(buffer, sizeof(buffer), MSG_PEEK);
			string rawRequest(buffer, size);	
			for (int i = 0; i < hooks.size(); i++) {
				conn = hooks[i]->createConnection(socket, rawRequest);
				if (conn)
					break;
			}
		}
	}
	catch (Exception& exc) {
		errorL("HTTPServer") << "Hook Error: " << exc.displayText() << endl;
	}
	return conn;
}


// ---------------------------------------------------------------------
//
ServerConnectionFactory::ServerConnectionFactory(Server* server, Poco::Net::HTTPServerParams::Ptr pParams, Poco::Net::HTTPServerResponderFactory::Ptr pFactory):
	_server(server),
	_pParams(pParams),
	_pFactory(pFactory)
{
	poco_check_ptr (pFactory);
}


ServerConnectionFactory::~ServerConnectionFactory()
{
}


Poco::Net::TCPServerConnection* ServerConnectionFactory::createConnection(const Poco::Net::StreamSocket& socket)
{
	Poco::Net::TCPServerConnection* conn = _server->handleSocketConnection(socket);
	if (conn) {		
		traceL("ServerConnectionFactory") << "Hook Overriding" << endl;
		return conn;
	}
	return new Poco::Net::ServerConnection(socket, _pParams, _pFactory);
}
	*/