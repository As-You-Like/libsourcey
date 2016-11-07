#include "scy/net/tcpsocket.h"
#include "scy/net/sslsocket.h"


namespace scy {
namespace net {


template <class SocketT>/// The TCP echo server accepts a template argument
/// of either a TCPSocket or a SSLSocket.
class EchoServer
{
public:
    typename SocketT::Ptr socket;
    typename Socket::Vec sockets;

    EchoServer() :
        socket(std::make_shared<SocketT>())
    {
    }

    ~EchoServer()
    {
        shutdown();
    }

    void start(const std::string& host, std::uint16_t port)
    {
        auto ssl = dynamic_cast<SSLSocket*>(socket.get());
        if (ssl)
            ssl->useContext(SSLManager::instance().defaultServerContext());

        socket->bind(Address(host, port));
        socket->listen();
        socket->AcceptConnection += delegate(this, &EchoServer::onAcceptConnection);
    }

    void shutdown()
    {
        socket->close();
        sockets.clear();
    }

    void onAcceptConnection(const TCPSocket::Ptr& sock)
    {    
    /// std::static_pointer_cast<SocketT>(ptr)
        sockets.push_back(sock);
        auto& socket = sockets.back();
        DebugL << "On accept: " << socket << std::endl;
        socket->Recv += sdelegate(this, &EchoServer::onClientSocketRecv);
        socket->Error += sdelegate(this, &EchoServer::onClientSocketError);
        socket->Close += sdelegate(this, &EchoServer::onClientSocketClose);
    }

    void onClientSocketRecv(void* sender, const MutableBuffer& buffer, const Address& peerAddress)
    {
        auto socket = reinterpret_cast<net::Socket*>(sender);
        DebugL << "On recv: " << socket << ": "
            << buffer.str() << std::endl;    /// Echo it back
        socket->send(bufferCast<const char*>(buffer), buffer.size());
    }

    void onClientSocketError(void* sender, const Error& error)
    {
        InfoL << "On error: " << error.errorno << ": " << error.message << std::endl;
    }

    void onClientSocketClose(void* sender)
    {
        DebugL << "On close" << std::endl;
        releaseSocket(reinterpret_cast<net::Socket*>(sender));
    }

    void releaseSocket(net::Socket* sock)
    {
        for (typename Socket::Vec::iterator it = sockets.begin(); it != sockets.end(); ++it) {
            if (sock == it->get()) {
                DebugL << "Removing: " << sock<< std::endl;

                // All we need to do is erase the socket in order to
                // deincrement the ref counter and destroy the socket->
                // assert(sock->base().refCount() == 1);
                sockets.erase(it);
                return;
            }
        }
        assert(0 && "unknown socket");
    }
};


// Some generic server types
typedef EchoServer<TCPSocket> TCPEchoServer;
typedef EchoServer<SSLSocket> SSLEchoServer;


} } // namespace scy::net
