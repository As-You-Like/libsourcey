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


#ifndef SOURCEY_HTTP_Server_H
#define SOURCEY_HTTP_Server_H


#include "Sourcey/Base.h"
#include "Sourcey/Logger.h"
#include "Sourcey/Net/Socket.h"
#include "Sourcey/HTTP/Connection.h"
#include "Sourcey/HTTP/Request.h"
#include "Sourcey/HTTP/Response.h"
#include "Sourcey/HTTP/Parser.h"
#include "Sourcey/Timer.h"

	
namespace scy { 
namespace http {


class Server;
class ServerResponder;
class ServerConnection: public Connection
{
public:
    ServerConnection(Server& server, const net::Socket& socket);
	
	virtual bool send();
		/// Sends the HTTP response
	
	virtual void close();
		/// Closes the HTTP connection
	
protected:
    virtual ~ServerConnection();
		
	virtual void onHeaders();
	virtual void onPayload(Buffer& buffer);
	virtual void onComplete();
	virtual void onClose();
				
	Server& server();

	Poco::Net::HTTPMessage* incomingHeaders();
	Poco::Net::HTTPMessage* outgoingHeaders();

	//
	/// Server callbacks
	void onServerShutdown(void*);
	
protected:
	Server& _server;
	ServerResponder* _responder;	
	bool _upgrade;
	bool _requestComplete;
};


typedef std::vector<ServerConnection*> ServerConnectionList;

	
// -------------------------------------------------------------------
//
class ServerAdapter: public ConnectionAdapter
{
public:
    ServerAdapter(ServerConnection& connection) : 
		ConnectionAdapter(connection, HTTP_REQUEST)
	{
	}
};


// -------------------------------------------------------------------
//
class ServerResponder
	/// The abstract base class for HTTP ServerResponders 
	/// created by HTTP Server.
	///
	/// Derived classes must override the handleRequest() method.
	///
	/// A new HTTPServerResponder object will be created for
	/// each new HTTP request that is received by the HTTP Server.
	///
{
public:
	ServerResponder(ServerConnection& connection) : 
		_connection(connection)
	{
	}

	virtual ~ServerResponder() {}

	virtual void onHeaders(Request& request) {}
	virtual void onPayload(const Buffer& body) {}
	virtual void onRequest(Request& request, Response& response) {}
	virtual void onClose() {};

	ServerConnection& connection()
	{
		return _connection;
	}
		
	Request& request()
	{
		return _connection.request();
	}
	
	Response& response()
	{
		return _connection.response();
	}

protected:
	ServerConnection& _connection;
};


// -------------------------------------------------------------------
//
class ServerResponderFactory
	/// This implementation of a ServerResponderFactory
	/// is used by HTTPServer to create ServerResponder objects.
{
public:
	virtual ServerResponder* createResponder(ServerConnection& connection) = 0;
		/// Factory method for instantiating the ServerResponder
		/// instance using the given ServerConnection.
};


// -------------------------------------------------------------------
//
class Server
	/// DISCLAIMER: This HTTP server is not intended to be standards 
	/// compliant. It was created to be a fast (nocopy where possible)
	/// solution for streaming video to web browsers.
	///
	/// TODO: 
	/// - SSL Server
	/// - Enable responders (controllers?) to be instantiated via
	//    registered routes.
{
public:
	ServerConnectionList connections;
	ServerResponderFactory* factory;
	net::TCPSocket socket;
	net::Address address;
	//Timer timer;

	Server(short port, ServerResponderFactory* factory);
	virtual ~Server();
	
	void start();
	void shutdown();

	UInt16 port();	

	NullSignal Shutdown;

protected:	
	ServerConnection* createConnection(const net::Socket& sock);
	ServerResponder* createResponder(ServerConnection& conn);

	virtual void addConnection(ServerConnection* conn);
	virtual void removeConnection(ServerConnection* conn);

	void onAccept(void* sender, const net::TCPSocket& sock);
	void onClose(void* sender);

	friend class ServerConnection;
};


// ---------------------------------------------------------------------
//
class BadRequestHandler: public ServerResponder
{
public:
	BadRequestHandler(ServerConnection& connection) : 		
		ServerResponder(connection)
	{		
	}

	void onRequest(Request& request, Response& response)
	{
		response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
		connection().send();
	}
};



} } // namespace scy::http


/*
// ---------------------------------------------------------------------
//
class FlashPolicyConnectionHook: public ServerResponder
{
public:
	Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket, const std::string& rawRequest)
	{		
		try 
		{			
			if (rawRequest.find("policy-file-request") != std::string::npos) {
				traceL("HTTPStreamingRequestHandlerFactory") << "Sending Flash Crossdomain XMLSocket Policy" << std::endl;
				return new Net::FlashPolicyRequestHandler(socket, false);
			}
			else if (rawRequest.find("crossdomain.xml") != std::string::npos) {
				traceL("HTTPStreamingRequestHandlerFactory") << "Sending Flash Crossdomain HTTP Policy" << std::endl;
				return new Net::FlashPolicyRequestHandler(socket, true);
			}			
		}
		catch (Exception& exc)
		{
			LogError("ServerConnectionHook") << "Bad Request: " << exc.displayText() << std::endl;
		}	
		return NULL;
	};
};
*/
	
	/*
	void onTimer(void*)
	{
		ServerConnectionList conns = ServerConnectionList(connections);
		for (ServerConnectionList::iterator it = conns.begin(); it != conns.end();) {
			if ((*it)->closed()) {
				traceL("Server", this) << "Deleting Connection: " << (*it) << std::endl;
				//delete *it;
				it = connections.erase(it);
			}
			else
				++it;
		}
	}
	*/

		// This will close the server socket, and should cause the
		// main loop to terminate if there are no active processes.
		//for (int i = 0; i < connections.size(); i++)
		//	delete connections[i];

/*

#include "Poco/Net/HTTPServerResponder.h"
#include "Sourcey/Net/TCPService.h"

#include "Poco/Mutex.h"
#include "Poco/Net/Net.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/HTTPServerResponderFactory.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerResponder.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPResponse.h"


	
class Server;
*/

/*
class Connection;

class ServerConnectionFactory//: public Poco::Net::TCPServerConnectionFactory
	/// This implementation of a TCPServerConnectionFactory
	/// is used by HTTPServer to create ServerConnection objects.
{
public:
	//ServerConnectionFactory(Server* server); //, Poco::Net::HTTPServerParams::Ptr pParams, Poco::Net::HTTPServerResponderFactory::Ptr pFactory
		/// Creates the ServerConnectionFactory.

	//virtual ~ServerConnectionFactory() {};
		/// Destroys the ServerConnectionFactory.

	virtual ServerConnection* createConnection(const net::Socket& socket) = 0;
		/// Creates an instance of ServerConnection
		/// using the given StreamSocket.
	
protected:
	//Poco::Net::HTTPServerParams::Ptr          _pParams;
	//Poco::Net::HTTPServerResponderFactory::Ptr _pFactory;
	Server& _server;
};
*/


/*
	//ServerResponderFactory() {}; //, Poco::Net::HTTPServerParams::Ptr pParams, Poco::Net::HTTPServerResponderFactory::Ptr pFactoryServer* server
		/// Creates the ServerResponderFactory.

	virtual ~ServerResponderFactory() {};
		/// Destroys the ServerResponderFactory.

	
//protected:
	//Poco::Net::HTTPServerParams::Ptr          _pParams;
	//Poco::Net::HTTPServerResponderFactory::Ptr _pFactory;
	//Server& _server;

	*/

				// All we need to do is erase the socket in order to 
				// deincrement the ref counter and destroy the socket.
				//assert(socket->base().refCount() == 1);
				
		//net::TCPSocket& socket = connections.back();
		//traceL("Server", this) << "On Accept: " << &socket.base() << std::endl;
		//socket.Recv += delegate(this, &Server::onSocketRecv);
		//socket.Error += delegate(this, &Server::onSocketError);
		//socket.Close += delegate(this, &Server::onSocketClose);
		//socket.Close += delegate(this, &Server::onSocketClose);

	
	/*
	friend class Connection;

protected:
	void addConnection(Connection* conn) 
	{		
		connections.push_back(conn);
	}

	void removeConnection(Connection* conn) 
	{		
		for (std::vector<Connection*>::iterator it = connections.begin(); it != connections.end(); ++it) {
			if (conn == *it) {
				traceL("Server", this) << "Removing" << std::endl; //: " << socket

				// All we need to do is erase the socket in order to 
				// deincrement the ref counter and destroy the socket.
				//assert(socket->base().refCount() == 1);
				//delete *it;
				connections.erase(it);
				return;
			}
		}
	}
	*/


	
	/*
	Server(Poco::Net::HTTPServerResponderFactory::Ptr pFactory, 
		const Poco::Net::ServerSocket& socket, Poco::Net::HTTPServerParams::Ptr pParams);
		/// Creates the HTTPServer, using the given ServerSocket.
		///
		/// New threads are taken from the default thread pool.

	Server(Poco::Net::HTTPServerResponderFactory::Ptr pFactory, Poco::ThreadPool& threadPool, 
			const Poco::Net::ServerSocket& socket, Poco::Net::HTTPServerParams::Ptr pParams);
		/// Creates the HTTPServer, using the given ServerSocket.
		///
		/// New threads are taken from the given thread pool.
		*/

	/*
	virtual void addConnectionHook(ServerConnectionHook* hook);
		/// Adds a connection hook to process incoming socket requests
		/// before they hit the HTTP server.
	
	virtual Poco::Net::TCPServerConnection* handleSocketConnection(const net::Socket& socket);
		// Called by ServerConnectionFactory to process connection hooks.
	
	ServerConnectionHookVec connectionHooks() const;
	//mutable Poco::FastMutex _mutex;
	//Poco::Net::HTTPServerResponderFactory::Ptr _pFactory;
	//ServerConnectionHookVec _connectionHooks;
	*/


/*




class Server;
class ServerConnectionFactory: public Poco::Net::TCPServerConnectionFactory
	/// This implementation of a TCPServerConnectionFactory
	/// is used by HTTPServer to create ServerConnection objects.
{
public:
	ServerConnectionFactory(Server* server, Poco::Net::HTTPServerParams::Ptr pParams, Poco::Net::HTTPServerResponderFactory::Ptr pFactory);
		/// Creates the ServerConnectionFactory.

	~ServerConnectionFactory();
		/// Destroys the ServerConnectionFactory.

	Poco::Net::TCPServerConnection* createConnection(const net::Socket& socket);
		/// Creates an instance of ServerConnection
		/// using the given StreamSocket.
	
protected:
	Poco::Net::HTTPServerParams::Ptr          _pParams;
	Poco::Net::HTTPServerResponderFactory::Ptr _pFactory;
	Server* _server;
};


// ---------------------------------------------------------------------
//
class ServerConnectionHook
	/// Provides a hook access to incoming socket requests from Poco's
	/// frustratingly limited access HTTP server implementation.
{
public:
	virtual Poco::Net::TCPServerConnection* createConnection(const net::Socket& socket, const std::string& rawRequest) = 0;
};

typedef std::vector<ServerConnectionHook*> ServerConnectionHookVec;

#include "Sourcey/Base.h"
#include "Sourcey/Logger.h"
#include "Sourcey/Net/TCPService.h"

#include "Poco/Mutex.h"
#include "Poco/Net/Net.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/HTTPServerResponderFactory.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerResponder.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPResponse.h"

	
namespace scy { 
namespace http {


class Server;
class ServerConnectionFactory: public Poco::Net::TCPServerConnectionFactory
	/// This implementation of a TCPServerConnectionFactory
	/// is used by HTTPServer to create ServerConnection objects.
{
public:
	ServerConnectionFactory(Server* server, Poco::Net::HTTPServerParams::Ptr pParams, Poco::Net::HTTPServerResponderFactory::Ptr pFactory);
		/// Creates the ServerConnectionFactory.

	~ServerConnectionFactory();
		/// Destroys the ServerConnectionFactory.

	Poco::Net::TCPServerConnection* createConnection(const net::Socket& socket);
		/// Creates an instance of ServerConnection
		/// using the given StreamSocket.
	
protected:
	Poco::Net::HTTPServerParams::Ptr          _pParams;
	Poco::Net::HTTPServerResponderFactory::Ptr _pFactory;
	Server* _server;
};


// ---------------------------------------------------------------------
//
class ServerConnectionHook
	/// Provides a hook access to incoming socket requests from Poco's
	/// frustratingly limited access HTTP server implementation.
{
public:
	virtual Poco::Net::TCPServerConnection* createConnection(const net::Socket& socket, const std::string& rawRequest) = 0;
};

typedef std::vector<ServerConnectionHook*> ServerConnectionHookVec;


// ---------------------------------------------------------------------
//
class Server: public Poco::Net::TCPServer
	/// A subclass of TCPServer that implements a
	/// full-featured multithreaded HTTP server.
	///
	/// A HTTPServerResponderFactory must be supplied.
	/// The ServerSocket must be bound and in listening state.
	///
	/// To configure various aspects of the server, a HTTPServerParams
	/// object can be passed to the constructor.
	///
	/// The server supports:
	///   - HTTP/1.0 and HTTP/1.1
	///   - automatic handling of persistent connections.
	///   - automatic decoding/encoding of request/response message bodies
	///     using chunked transfer encoding.
	///
	/// Please see the TCPServer class for information about
	/// connection and thread handling.
	///
	/// See RFC 2616 <http://www.faqs.org/rfcs/rfc2616.html> for more
	/// information about the HTTP protocol.
{
public:
	Server(Poco::Net::HTTPServerResponderFactory::Ptr pFactory, 
		const Poco::Net::ServerSocket& socket, Poco::Net::HTTPServerParams::Ptr pParams);
		/// Creates the HTTPServer, using the given ServerSocket.
		///
		/// New threads are taken from the default thread pool.

	Server(Poco::Net::HTTPServerResponderFactory::Ptr pFactory, Poco::ThreadPool& threadPool, 
			const Poco::Net::ServerSocket& socket, Poco::Net::HTTPServerParams::Ptr pParams);
		/// Creates the HTTPServer, using the given ServerSocket.
		///
		/// New threads are taken from the given thread pool.

	virtual ~Server();
		/// Destroys the HTTPServer and its HTTPServerResponderFactory.

	virtual void addConnectionHook(ServerConnectionHook* hook);
		/// Adds a connection hook to process incoming socket requests
		/// before they hit the HTTP server.
	
	virtual Poco::Net::TCPServerConnection* handleSocketConnection(const net::Socket& socket);
		// Called by ServerConnectionFactory to process connection hooks.
	
	ServerConnectionHookVec connectionHooks() const;

protected:
	mutable Poco::FastMutex _mutex;
	Poco::Net::HTTPServerResponderFactory::Ptr _pFactory;
	ServerConnectionHookVec _connectionHooks;
};


// ---------------------------------------------------------------------
//
class FlashPolicyConnectionHook: public http::ServerConnectionHook
{
public:
	Poco::Net::TCPServerConnection* createConnection(const net::Socket& socket, const std::string& rawRequest)
	{		
		try 
		{			
			if (rawRequest.find("policy-file-request") != std::string::npos) {
				traceL("HTTPStreamingServerResponderFactory") << "Sending Flash Crossdomain XMLSocket Policy" << std::endl;
				return new Net::FlashPolicyServerResponder(socket, false);
			}
			else if (rawRequest.find("crossdomain.xml") != std::string::npos) {
				traceL("HTTPStreamingServerResponderFactory") << "Sending Flash Crossdomain HTTP Policy" << std::endl;
				return new Net::FlashPolicyServerResponder(socket, true);
			}			
		}
		catch (Exception& exc)
		{
			errorL("ServerConnectionHook") << "Bad Request: " << exc.displayText() << std::endl;
		}	
		return NULL;
	};
};


// ---------------------------------------------------------------------
//
class BadServerResponder: public Poco::Net::HTTPServerResponder
{
public:
	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
	{
		response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
		response.send().flush();
	}
};


} } // namespace scy::http
*/


#endif