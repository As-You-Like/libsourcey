#ifndef SCY_HTTP_Tests_H
#define SCY_HTTP_Tests_H


#include "scy/base.h"
#include "scy/test.h"
#include "scy/async.h"
#include "scy/timer.h"
#include "scy/idler.h"
#include "scy/filesystem.h"
#include "scy/crypto/hash.h"
#include "scy/net/sslmanager.h"
#include "scy/net/sslcontext.h"
#include "scy/http/server.h"
#include "scy/http/connection.h"
#include "scy/http/client.h"
#include "scy/http/websocket.h"
#include "scy/http/packetizers.h"
#include "scy/http/form.h"
#include "scy/http/util.h"
#include "scy/http/url.h"

#include "../samples/httpechoserver/httpechoserver.h"


using std::cout;
using std::cerr;
using std::endl;
using scy::test::Test;


#define TEST_HTTP_PORT 1337
#define TEST_HTTPS_PORT 1338


namespace scy {


//
/// Generic Callback Context
//

struct CallbackContext
{
    void onClientConnectionComplete(void* sender, const http::Response& response)
    {    
    /// auto conn = reinterpret_cast<http::ClientConnection*>(sender);    /// << conn->readStream<std::stringstream>()->str()
        TraceL << "Server response: " << response  << endl;

    }


    void onClientConnectionDownloadComplete(void* sender, const http::Response& response)
    {    
    /// auto conn = reinterpret_cast<http::ClientConnection*>(sender);

        TraceL << "Server response: " << response << endl;
    }

    void onStandaloneHTTPClientConnectionHeaders(void*, http::Response& res)
    {
        DebugL << "On response headers: " << res << endl;
    }

    void onStandaloneHTTPClientConnectionPayload(void* sender, const MutableBuffer& buffer)
    {
        DebugL << "On payload: " << buffer.size() << ": " << buffer.str() << endl;
    }

    void onStandaloneHTTPClientConnectionComplete(void* sender, const http::Response& response)
    {
        auto self = reinterpret_cast<http::ClientConnection*>(sender);    // << self->readStream<std::stringstream>()->str()
        DebugL << "On response complete: " << response << endl;

    /// Force the connection closure if the other side hasn't already
        self->close();
    }

    void onAssetUploadProgress(void* sender, const double& progress)
    {
        DebugL << "Upload Progress:" << progress << endl;
    }

    void onAssetUploadComplete(void* sender, const http::Response& response)
    {
        auto conn = reinterpret_cast<http::ClientConnection*>(sender);

        DebugL << "Transaction Complete:"
            << "\n\tRequest Head: " << conn->request()
            << "\n\tResponse Head: " << response
            //<< "\n\tResponse Body: " << trans->incomingBuffer()
            << endl;

        //expect(response.success());
    }
};


//
/// HTTP Client Tests
//

/// Initializes a polymorphic HTTP client connection for
/// testing callbacks, and also optionally raises the server.
struct HTTPEchoTest
{
    http::Server server;
    http::Client client;
    http::ClientConnection::Ptr conn;
    RandomDataSource dataSource;
    int numSuccess;
    int numWanted;

    HTTPEchoTest(int numWanted = 1) :
        server(TEST_HTTP_PORT, new OurServerResponderFactory),
        numSuccess(0),
        numWanted(numWanted)
    {
    }

    void raiseServer()
    {
        server.start();
    }

    http::ClientConnection::Ptr createConnection(const std::string& protocol, const std::string& query)
    {
        std::ostringstream url;
        url << protocol << "://127.0.0.1:" << TEST_HTTP_PORT << query << endl;
        conn = client.createConnection(url.str());
        conn->Connect += delegate(this, &HTTPEchoTest::onConnect);
        conn->Headers += delegate(this, &HTTPEchoTest::onHeaders);    // conn->Payload += delegate(this, &HTTPEchoTest::onPayload);
        conn->Incoming.emitter += packetDelegate(this, &HTTPEchoTest::onPayload);

        conn->Complete += delegate(this, &HTTPEchoTest::onComplete);
        conn->Close += delegate(this, &HTTPEchoTest::onClose);
        return conn;
    }

    void start()
    {
        conn->send("PING", 4);
    }

    void shutdown()
    {    
    /// Stop the client and server to release the loop
        server.shutdown();
        client.shutdown();

        expect(numSuccess == numWanted);
    }

    void onConnect()
    {
        DebugL << "On connect" <<  endl;    /// Bounce backwards and forwards
    /// conn->send("PING", 4);
    }

    void onHeaders(http::Response& res)
    {
        DebugL << "On headers" <<  endl;    /// Start the output stream when the socket connects.
    /// dataSource.start();    // dataSource.conn = this->conn;

    }

    void onComplete(const http::Response& res)
    {
        std::ostringstream os;
        res.write(os);
        DebugL << "Response complete: " << os.str() << endl;
    }

    void onPayload(RawPacket& packet)
    {
        std::string data(packet.data(), packet.size());
        DebugL << "On payload: " << packet.size() << ": " << data << endl;

        if (data == "PING")
            numSuccess++;

        if (numSuccess == numWanted) {
            shutdown();
        }
        else {
            conn->send("PING", 4);
        }
    }

    void onClose(void*)
    {
        DebugL << "Connection closed" << endl;
        shutdown();
    }
};


} // namespace scy


#endif // SCY_HTTP_Tests_H

/// @\}
