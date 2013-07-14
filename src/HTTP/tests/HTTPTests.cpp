#include "Sourcey/Application.h"
#include "Sourcey/HTTP/Server.h"
#include "Sourcey/HTTP/Connection.h"
#include "Sourcey/HTTP/Client.h"
#include "Sourcey/HTTP/WebSocket.h"
#include "Sourcey/Runner.h"
#include "Sourcey/Timer.h"

#include "Sourcey/Base.h"
#include "Sourcey/Logger.h"
#include "Sourcey/Net/SSLManager.h"
#include "Sourcey/Net/SSLContext.h"
#include "Sourcey/Net/Address.h"

#include "assert.h"
#include <iterator>


using namespace std;
using namespace scy;
using namespace scy::net;


/*
// Detect memory leaks on winders
#if defined(_DEBUG) && defined(_WIN32)
#include "MemLeakDetect/MemLeakDetect.h"
#include "MemLeakDetect/MemLeakDetect.cpp"
CMemLeakDetect memLeakDetect;
#endif
*/


namespace scy {
namespace http {

	
#define TEST_SSL 1 //0
#define TEST_HTTP_PORT 1337
#define TEST_HTTPS_PORT 1338


// -------------------------------------------------------------------
//
class BasicResponder: public ServerResponder
{
public:
	BasicResponder(ServerConnection& conn) : 
		ServerResponder(conn)
	{
	}

	void onComplete(Request& request, Response& response) 
	{
		response.body << "Hello universe";
		connection().send();
	}
};


// -------------------------------------------------------------------
//
class ChunkedResponder: public ServerResponder
{
public:
	Timer timer;
	bool gotHeaders;
	bool gotRequest;
	bool gotClose;

	ChunkedResponder(ServerConnection& conn) : 
		ServerResponder(conn), 
		gotHeaders(false), 
		gotRequest(false), 
		gotClose(false)
	{
	}

	~ChunkedResponder()
	{
		assert(gotHeaders);
		assert(gotRequest);
		assert(gotClose);
	}

	void onHeaders(Request& request) 
	{
		gotHeaders = true;
	}

	void onPayload(const Buffer& body)
	{
		// may be empty
	}

	void onComplete(Request& request, Response& response) 
	{
		gotRequest = true;
		timer.Timeout += delegate(this, &ChunkedResponder::onTimer);
		timer.start(100, 100);
	}

	void onClose()
	{
		debugL("ChunkedResponder") << "On connection close" << endl;
		gotClose = true;
		timer.stop();
	}
	
	void onTimer(void*)
	{
		connection().sendRaw("bigfatchunk", 11);

		if (timer.count() == 10)
			connection().close();
	}
};


// -------------------------------------------------------------------
//
class WebSocketResponder: public ServerResponder
{
public:
	bool gotPayload;
	bool gotClose;

	WebSocketResponder(ServerConnection& conn) : 
		ServerResponder(conn), 
		gotPayload(false), 
		gotClose(false)
	{
		debugL("WebSocketResponder") << "Creating" << endl;
	}

	~WebSocketResponder()
	{
		debugL("WebSocketResponder") << "Destroying" << endl;
		assert(gotPayload);
		assert(gotClose);
	}

	void onPayload(const Buffer& body)
	{
		debugL("WebSocketResponder") << "On payload: " << body << endl;

		gotPayload = true;

		// Enco the request back to the client
		connection().sendRaw(body.data(), body.size());
	}

	void onClose()
	{
		debugL("WebSocketResponder") << "On connection close" << endl;
		gotClose = true;
	}
};


// -------------------------------------------------------------------
//
class OurServerResponderFactory: public ServerResponderFactory
{
public:
	ServerResponder* createResponder(ServerConnection& conn)
	{		
		ostringstream os;
		conn.request().write(os);
		string headers(os.str().data(), os.str().length());
		debugL("OurServerResponderFactory") << "Incoming Request: " << headers << endl; // remove me

		if (conn.request().getURI() == "/chunked")
			return new ChunkedResponder(conn);
		else if (conn.request().getURI() == "/websocket")
			return new WebSocketResponder(conn);
		else
			return new BasicResponder(conn);
	}
};


// -------------------------------------------------------------------
//
template <typename SocketT>
class SocketClientEchoTest
{
public:
	typename SocketT socket;
	net::Address address;

	SocketClientEchoTest(const net::Address& addr, bool ghost = false) :
		address(addr)
	{		
		debugL("SocketClientEchoTest") << "Creating: " << addr << endl;

		socket.Recv += delegate(this, &SocketClientEchoTest::onRecv);
		socket.Connect += delegate(this, &SocketClientEchoTest::onConnect);
		socket.Error += delegate(this, &SocketClientEchoTest::onError);
		socket.Close += delegate(this, &SocketClientEchoTest::onClose);
	}

	~SocketClientEchoTest()
	{
		debugL("SocketClientEchoTest") << "Destroying" << endl;

		assert(socket.base().refCount() == 1);
	}

	void start() 
	{
		// Create the socket instance on the stack.
		// When the socket is closed it will unref the main loop
		// causing the test to complete successfully.
		socket.connect(address);
		assert(socket.base().refCount() == 1);
	}

	void stop() 
	{
		//socket.close();
		socket.shutdown();
	}
	
	void onConnect(void* sender)
	{
		debugL("SocketClientEchoTest") << "Connected" << endl;
		assert(sender == &socket);
		socket.send("client > server", 15, WebSocket::FRAME_TEXT);
	}
	
	void onRecv(void* sender, net::SocketPacket& packet)
	{
		assert(sender == &socket);
		string data(packet.data(), 15);
		debugL("SocketClientEchoTest") << "Recv: " << data << endl;	

		// Check for return packet echoing sent data
		if (data == "client > server") {
			debugL("SocketClientEchoTest") << "Recv: Got Return Packet!" << endl;
			
			// Send the shutdown command to close the connection gracefully.
			// The peer disconnection will trigger an error callback which
			// will result is socket closure.
			socket.base().shutdown();
		}
		else
			assert(0 && "not echo response"); // fail...
	}

	void onError(void* sender, const Error& err)
	{
		errorL("SocketClientEchoTest") << "On error: " << err.message << endl;
		assert(sender == &socket);
	}
	
	void onClose(void* sender)
	{
		debugL("SocketClientEchoTest") << "On close" << endl;
		assert(sender == &socket);
	}
};


// -------------------------------------------------------------------
//
class Tests
{
public:
	Application app; 	

	Tests()
	{	
		debugL("Tests") << "#################### Starting" << endl;
#ifdef _MSC_VER
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#if TEST_SSL
			// Init SSL Context 
			SSLContext::Ptr ptrContext = new SSLContext(
				SSLContext::CLIENT_USE, "", "", "", 
				SSLContext::VERIFY_NONE, 9, false, 
				"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");		
#endif
		{		
			/*
			runHTTPClientTest();	
			runClientConnectionChunkedTest();	
			runWebSocketClientServerTest();
			runWebSocketSocketTest();
			runHTTPClientWebSocketTest();	
			runWSClientConnectionTest();	
			runWSSClientConnectionTest();
			*/

			// NOTE: Must be terminated with Crtl-C
			//runHTTPServerTest();
		}
#if TEST_SSL
			// Shutdown SSL
			SSLManager::instance().shutdown();
#endif
		
		debugL("Tests") << "#################### Finalizing" << endl;
		app.cleanup();
		debugL("Tests") << "#################### Exiting" << endl;
	}

	void runLoop() {
		debugL("Tests") << "#################### Running" << endl;
		app.run();
		debugL("Tests") << "#################### Ended" << endl;
	}

	
	// ============================================================================
	// HTTP Client Tests
	//
	struct HTTPClientTest 
	{
		http::Server server;
		http::Client client;
		int numSuccess;
		ClientConnection* conn;

		HTTPClientTest() :
			numSuccess(0),
			conn(0),
			server(TEST_HTTP_PORT, new OurServerResponderFactory) 
		{
		}
		
		template<class ConnectionT>
		ConnectionT* create(bool raiseServer = true, const net::Address& address = net::Address("127.0.0.1", TEST_HTTP_PORT))
		{
			if (raiseServer)
				server.start();

			conn = (ClientConnection*)client.createConnectionT<ConnectionT>(address);		
			conn->Headers += delegate(this, &HTTPClientTest::onHeaders);
			conn->Payload += delegate(this, &HTTPClientTest::onPayload);
			conn->Complete += delegate(this, &HTTPClientTest::onComplete);
			conn->Close += delegate(this, &HTTPClientTest::onClose);
			return (ConnectionT*)conn;
		}

		void shutdown()
		{
			// Stop the client and server to release the loop
			server.shutdown();
			client.shutdown();
		}

		void onHeaders(void*, Response& res)
		{
			debugL("ClientConnectionTest") << "On headers" <<  endl;
			
			// Bounce backwards and forwards a few times :)
			conn->sendRaw("BOUNCE", 6);
		}

		void onPayload(void*, Buffer& buf)
		{
			if (buf.toString() == "BOUNCE")
				numSuccess++;
			
			debugL("ClientConnectionTest") << "On response payload: " << buf << ": " << numSuccess << endl;
			if (numSuccess >= 100) {
				
				debugL("ClientConnectionTest") << "SUCCESS: " << numSuccess << endl;
				conn->close();
			}
			else
				conn->sendRaw("BOUNCE", 6);
		}

		void onComplete(void*, const Response& res)
		{		
			ostringstream os;
			res.write(os);
			debugL("ClientConnectionTest") << "Response complete: " << os.str() << endl;
		}

		void onClose(void*)
		{	
			debugL("ClientConnectionTest") << "Connection closed" << endl;
			shutdown();
		}
	};

	void runClientConnectionTest() 
	{	
		HTTPClientTest test;
		test.create<ClientConnection>()->send(); // default GET request
		runLoop();
	}

	void runClientConnectionChunkedTest() 
	{	
		HTTPClientTest test;		
		ClientConnection* conn = test.create<ClientConnection>();
		conn->request().setURI("/chunked");
		conn->request().body << "BOUNCE" << endl;
		conn->send();
		runLoop();
	}
	
	void runWSClientConnectionTest() 
	{	
		HTTPClientTest test;
		ClientConnection* conn = test.create<WSClientConnection>(false);
		conn->request().setURI("/websocket");
		conn->request().body << "BOUNCE" << endl;
		conn->send();
		runLoop();
	}

	void runWSSClientConnectionTest() 
	{	
		HTTPClientTest test;
		ClientConnection* conn = test.create<WSSClientConnection>(false, net::Address("127.0.0.1", TEST_HTTPS_PORT));
		conn->request().setURI("/websocket");
		conn->request().body << "BOUNCE" << endl;
		conn->send();
		runLoop();
	}
	
	
	// ============================================================================
	// WebSocket Test
	//
	void runClientWebSocketTest() 
	{
		//http::Server srv(TEST_HTTP_PORT, new OurServerResponderFactory);
		//srv.start();

		// ws://echo.websocket.org		
		//SocketClientEchoTest<http::WebSocket> test(net::Address("174.129.224.73", 1339));
		//SocketClientEchoTest<http::WebSocket> test(net::Address("174.129.224.73", 80));

		debugL("Tests") << "TCP Socket Test: Starting" << endl;
		SocketClientEchoTest<http::WebSocket> test(net::Address("127.0.0.1", TEST_HTTP_PORT));
		test.start();

		runLoop();
	}	

	/*
	// ============================================================================
	// WebSocket Test
	//
	void runWebSocketServerTest() 
	{
		http::Server srv(TEST_HTTP_PORT, new OurServerResponderFactory);
		srv.start();

		debugL("Tests") << "TCP Socket Test: Starting" << endl;			
		// ws://echo.websocket.org		
		//SocketClientEchoTest<http::WebSocket> test(net::Address("174.129.224.73", 1339));
		//SocketClientEchoTest<http::WebSocket> test(net::Address("174.129.224.73", 80));
		//SocketClientEchoTest<http::WebSocket> test(net::Address("127.0.0.1", 1340));
		//test.start();

		runLoop();
	}
	*/
	
	// ============================================================================
	// HTTP Server Test
	//
	void runHTTPServerTest() 
	{
		http::Server srv(TEST_HTTP_PORT, new OurServerResponderFactory);
		srv.start();
		
		// Catch close signal to shutdown the server
		// This should free the main loop.
		uv_signal_t* sig = new uv_signal_t;
		sig->data = &srv;
		uv_signal_init(&app.loop, sig);
		uv_signal_start(sig, Tests::onKillHTTPServer, SIGINT);

		runLoop();
	}
	
	static void onPrintHTTPServerHandle(uv_handle_t* handle, void* arg) 
	{
		debugL("HTTPServerTest") << "#### Active HTTPServer Handle: " << handle << endl;
	}

	static void onKillHTTPServer(uv_signal_t *req, int signum)
	{
		debugL("HTTPServerTest") << "Kill Signal: " << req << endl;
	
		// print active handles
		uv_walk(req->loop, Tests::onPrintHTTPServerHandle, NULL);
	
		((http::Server*)req->data)->shutdown();

		uv_signal_stop(req);
	}

};


} } // namespace scy::http


int main(int argc, char** argv) 
{	
	Logger::instance().add(new ConsoleChannel("debug", TraceLevel));
	{
		http::Tests app;
	}
	Logger::uninitialize();
	return 0;
}




		/*
		uv_signal_t sig;
		sig.data = &srv;
		uv_signal_init(app.loop, &sig);
		uv_signal_start(&sig, Tests::onKillHTTPServer, SIGINT);
		*/
			
			/*
			// ============================================================================
			// HTTP Server
			//
			http::Server srv(81, new OurServerResponderFactory);
			srv.start();

			(sock);
			net::TCPSocket sock;	
			runHTTPClientTest();

			uv_signal_t sig;
			sig.data = this;
			uv_signal_init(app.loop, &sig);
			uv_signal_start(&sig, Tests::onKillSignal2, SIGINT);

			runUDPSocketTest();
			runTimerTest();
			//runDNSResolverTest();
			
			debugL("Tests") << "#################### Running" << endl;
			app.run();
			debugL("Tests") << "#################### Ended" << endl;
			*/

			/*		
			http::Server srv(81, new OurServerResponderFactory);
			srv.start();

			//runHTTPClientTest();	
			(sock);
			net::TCPSocket sock;	
			runHTTPClientTest();

			uv_signal_t sig;
			sig.data = this;
			uv_signal_init(app.loop, &sig);
			uv_signal_start(&sig, Tests::onKillSignal2, SIGINT);

			runUDPSocketTest();
			runTimerTest();
			//runDNSResolverTest();
			
			debugL("Tests") << "#################### Running" << endl;
			app.run();
			debugL("Tests") << "#################### Ended" << endl;
			*/
	
		/*
	// ============================================================================
	// HTTP ClientConnection Test
	//
	void runHTTPClientTest() 
	{
		debugL("ClientConnectionTest") << "Starting" << endl;	

		// Setup the transaction
		http::Request req("GET", "http://google.com");
		http::Response res;
		http::ClientConnection txn(&req);
		txn.Complete += delegate(this, &Tests::onComplete);
		txn.DownloadProgress += delegate(this, &Tests::onResponseProgress);	
		txn.send();

		// Run the looop
		app.run();
		util::pause();

		debugL("ClientConnectionTest") << "Ending" << endl;
	}		

	void onComplete(void* sender, http::Response& response)
	{
		debugL("ClientConnectionTest") << "On Complete: " << &response << endl;
	}

	void onResponseProgress(void* sender, http::TransferProgress& progress)
	{
		debugL("ClientConnectionTest") << "On Progress: " << progress.progress() << endl;
	}
		*/
	
	
//using http::Request;
//using http::Response;
//using http::ClientConnection;

/*
struct Result {
	int numSuccess;
	string name;
	Poco::Stopwatch sw;

	void reset() {
		numSuccess = 0;
		sw.reset();
	}
};

static Result Benchmark;
*/