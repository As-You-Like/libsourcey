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


#ifndef SOURCEY_NET_SocketBase_H
#define SOURCEY_NET_SocketBase_H


#include "Sourcey/Base.h"
#include "Sourcey/Memory.h"
#include "Sourcey/IPacket.h"
#include "Sourcey/Signal.h"
#include "Sourcey/Net/Types.h"
#include "Sourcey/Net/Address.h"


namespace scy {
namespace net {


class Socket;
class SocketPacket;
class SocketObserver;
class SocketEmitter;


class SocketBase
	/// SocketBase is the abstract base interface from      
	/// which all socket contexts derive.
{
public:
	virtual void connect(const Address& address) = 0;
	virtual bool shutdown() { throw Exception("Not implemented by protocol"); };
	virtual void close() = 0;

	virtual void bind(const Address& address, unsigned flags = 0) = 0;
	virtual void listen(int backlog = 64) { };
		
	virtual int send(const char* data, int len, int flags = 0) = 0;
	virtual int send(const char* data, int len, const Address& peerAddress, int flags = 0) = 0;

	virtual int send(const IPacket& packet, int flags = 0);
	virtual int send(const IPacket& packet, const Address& peerAddress, int flags = 0);
	
	virtual Address address() const = 0;
		/// The locally bound address.

	virtual Address peerAddress() const = 0;
		/// The connected peer address.

	virtual net::TransportType transport() const = 0;
		/// The transport protocol: TCP, UDP or SSLTCP.
		/// See TransportType definition.
	
	void* opaque;
		/// Optional owned client data pointer.
		/// The pointer is not managed by the socket base.
	
	virtual void emitConnect();
	virtual void emitRecv(Buffer& buf, const Address& peerAddr);
	virtual void emitError(int syserr, const std::string& message);
	virtual void emitClose();

	virtual void duplicate() = 0;
	virtual void release() = 0;
	virtual int refCount() const = 0;
	
	virtual void addObserver(SocketObserver& observer, bool shared = false);
	virtual void removeObserver(SocketObserver& observer);
	virtual void sortObservers();

protected:
	virtual ~SocketBase() {};

	std::vector<SocketObserver*> _observers;
	
	friend class Socket;
	friend class SocketObserver;
};


typedef std::vector<SocketBase*> SocketBaseList;


// -------------------------------------------------------------------
//
struct PacketInfo: public IPacketInfo
	/// An abstract interface for packet sources to
	/// provide extra information about packets.
{ 
	Socket& socket;
		/// The source socket

	Address peerAddress;	
		/// The peerAddress is for UDP compatibility.
		/// For TCP it will match the connected address.

	PacketInfo(Socket& socket, const Address& peerAddress) :
		socket(socket), 
		peerAddress(peerAddress) {}		

	PacketInfo(const PacketInfo& r) : 
		socket(r.socket), 
		peerAddress(r.peerAddress) {}

	virtual ~PacketInfo() {}; 
	
	virtual IPacketInfo* clone() const {
		return new PacketInfo(*this);
	}
};


// -------------------------------------------------------------------
//
class Socket
	/// Socket is a wrapper class for accessing the underlying 
	/// reference counted SocketBase instance.
	///
	/// Each Socket class has its own SocketBase instance. 
	/// See UDPSocket, TCPSocket and SSLSocket.
	///
	/// Socket exposes all basic SocketBase operations and can  
	/// be extended as necessary for different protocols.
{
public:
	Socket(const Socket& socket, SocketEmitter* emitter = 0);
		/// Attaches the socket context from the other socket and
		/// increments the reference count of the socket context.
		
	Socket(SocketBase* base, bool shared, SocketEmitter* emitter = 0);
		/// Creates the Socket from the given SocketBase and attaches
		/// the given or default SocketEmitter.
		///
		/// If shared is true we increment the SocketBase reference
		/// count. If not we do not increment the reference count,
		/// which means are taking ownership, and the destruction
		/// of this class  will be directly responsible for the
		/// destruction of the given SocketBase.

	Socket& operator = (const Socket& socket);
		/// Assignment operator.
		///
		/// Releases the socket's socket context and
		/// attaches the socket context from the other socket and
		/// increments the reference count of the socket context.

	virtual Socket& assign(SocketBase* base, bool shared);
		/// Assigns the SocketBase instance for this socket.
		/// Any methods that assigns the base instance should 
		/// assign() so thhat subclasses can manage instance
		/// pointer changes.
		
	virtual ~Socket();
		/// Destroys the Socket and releases the socket context.

	SocketBase& base() const;
		/// Returns the SocketBase for this socket.

	SocketEmitter& emitter() const;
		/// Returns the SocketEmitter for this socket.
		
	virtual void connect(const Address& address);
	virtual bool shutdown();
	virtual void close();

	virtual void bind(const Address& address);
	virtual void listen(int backlog = 64);
		
	virtual int send(const char* data, int len, int flags = 0);
	virtual int send(const char* data, int len, const Address& peerAddress, int flags = 0);

	virtual int send(const IPacket& packet, int flags = 0);
	virtual int send(const IPacket& packet, const Address& peerAddress, int flags = 0);
	
	virtual Address address() const;
		/// The locally bound address.

	virtual Address peerAddress() const;
		/// The connected peer address.

	virtual net::TransportType transport() const;
		/// The transport protocol: TCP, UDP or SSLTCP.
		/// See TransportType definition.
	
	NullSignal Connect;
		/// Signals that the socket is connected.

	Signal<SocketPacket&> Recv;
		/// Signals when data is received by the socket.

	Signal2<int, const std::string&> Error;
		/// Signals that the socket is closed in error.
		/// This signal will be sent just before the 
		/// Closed signal.

	NullSignal Close;
		/// Signals that the underlying socket is closed,
		/// maybe in error.
		
	//void duplicate();
	//void release();
	//int refCount() const;
	int isNull() const;

protected:		
	Socket(SocketEmitter* emitter = 0);
		/// Creates a NULL socket.

	friend class SocketBase;

	SocketBase* _base;
	SocketEmitter* _emitter;
};


// -------------------------------------------------------------------
//
class SocketObserver
	/// SocketObserver is the short and sweet socket event handling 
	/// interface which is also directly responsible for incrementing and 
	/// deincrementing the reference count of the underlying SocketBase.
	/// 
	/// This class can also be extended to implement custom processing 
	/// for received socket data before it is dispatched to the application.
	/// See the SocketEmitter, PacketSocket and Transaction classes for ideas.
	///
	/// TODO: SocketBase pointer here
	///
{
public:
	SocketObserver(int priority = 0);
	
	virtual ~SocketObserver();

	virtual void onConnect() = 0;
	virtual void onRecv(Buffer& buf, const Address& peerAddr) = 0;
	virtual void onError(int syserr, const std::string& message) = 0;
	virtual void onClose() = 0;
	
	int priority;
		/// A higher priority gives the current observer
		/// precedence in the socket callback chain.

	static bool compareProiroty(const SocketObserver* l, const SocketObserver* r) {
		return l->priority > r->priority;
	}
};


// -------------------------------------------------------------------
//
class SocketEmitter: public SocketObserver
	/// SocketEmitter is an proxy layer which is attached to a
	/// SocketBase instance to handle socket events.
	///
	/// SocketEmitters are directly responsible for incrementing and 
	/// deincrementing the reference count of the managing SocketBase.
	/// 
	/// This class can also be extended to implement custom processing 
	/// for received socket data before it is dispatched to the application.
	/// See the PacketSocketEmitter and Transaction classes for ideas.
	///
	/// TODO: Rename to SocketObserver, and extend as SocketEmitter with signals
	///
{
public:
	SocketEmitter(Socket* socket = NULL, int priority = 0);
		/// Creates the SocketEmitter
		/// The Socket instance can be NULL, but it must be set 
		/// before any callbacks come back.
	
	virtual ~SocketEmitter();

	virtual void onConnect();
	virtual void onRecv(Buffer& buf, const Address& peerAddr);
	virtual void onError(int syserr, const std::string& message);
	virtual void onClose();
	
	Socket* socket;
};


// -------------------------------------------------------------------
//
class SocketPacket: public RawPacket 
	/// SocketPacket is the  default packet type emitted by sockets.
	/// SocketPacket provides peer address information and a buffer
	/// reference for nocopy binary operations.
	///
	/// The referenced packet buffer and data are only guaranteed 
	/// for the duration of the receive callback.
{	
public:
	Buffer& buffer;
		/// The received data

	PacketInfo* info;
		/// PacketInfo pointer

	SocketPacket(Socket& socket, Buffer& buffer, const Address& peerAddress) : 
		RawPacket(NULL, new PacketInfo(socket, peerAddress), buffer.data(), buffer.size()),
		buffer(buffer)
	{
		info = (PacketInfo*)RawPacket::info;
	}

	SocketPacket(const SocketPacket& that) : 
		RawPacket(that), info(that.info), buffer(that.buffer)
	{
	}
	
	virtual ~SocketPacket() 
	{
	}

	virtual void print(std::ostream& os) const 
	{ 
		os << className() << ": " << info->peerAddress << std::endl; 
	}

	virtual IPacket* clone() const 
	{
		// make babies!
		return new SocketPacket(*this);
	}	

	virtual bool read(Buffer& buf) 
	{ 
		assert(0 && "write only"); 
		return false;
	}

	virtual void write(Buffer& buf) const 
	{	
		buf.write(_data, _size); 
	}
	
	virtual const char* className() const 
	{ 
		return "SocketPacket"; 
	}
};


} } // namespace scy::net


#endif // SOURCEY_NET_Socket_H



	//virtual Socket& assign(SocketBase* ptr);
	//virtual Socket& assign(const Socket& ptr);


 // { throw Exception("Not implemented by protocol"); }; // { throw Exception("Not implemented by protocol"); };

/*
template<class SocketT = SocketBase>
class SocketHandle: public Handle<SocketT>
	/// SocketHandle is a disposable socket wrapper for
	/// SocketBase types which can be created on the stack
	/// for easy reference counted memory management for 
	/// the underlying socket instance.
{
public:		
	typedef SocketT Base;
	typedef std::vector<SocketHandle<SocketT>> List;

	SocketHandle() :
		// NOTE: Only compiles for derived SocketBase implementations
		Handle<SocketT>(new SocketT)
	{
	}

	SocketHandle(SocketBase* ptr) :
		Handle<SocketT>(ptr)
	{
		assertInstance(ptr);
	}

	SocketHandle(const SocketHandle& ptr) : 
		Handle<SocketT>(ptr)
	{
		assertInstance(ptr);
	}

	SocketHandle& operator = (SocketBase* ptr)
	{
		assertInstance(ptr);
		Handle<SocketT>::operator = (socket);
		return *this;
	}
		
	SocketHandle& operator = (const SocketHandle& socket)
	{
		assertInstance(socket.base());
		Handle<SocketT>::operator = (socket);
		return *this;
	}

	~SocketHandle()
	{
	}
	
	void assertInstance(const SocketBase* ptr) 
	{	
		if (!dynamic_cast<const SocketT*>(ptr))
			throw Exception("Cannot assign incompatible socket");
	}
};
*/



/*
//template<class SocketT = SocketBase>
class SocketHandle: public Handle<SocketBase>
	/// SocketHandle is a disposable socket wrapper for
	/// SocketBase types which can be created on the stack
	/// for easy reference counted memory management for 
	/// the underlying socket instance.
{
public:		
	typedef SocketBase Base;
	typedef std::vector<SocketBase> List;

	SocketHandle(SocketBase* ptr) :
		Handle<SocketBase>(ptr)
	{
		assertInstance(ptr);
	}

	SocketHandle(const SocketHandle& socket) : 
		Handle<SocketBase>(socket)
	{
		assertInstance(socket.base());
	}

	virtual ~SocketHandle()
	{
	}

	SocketHandle& operator = (SocketBase* ptr)
	{
		assertInstance(ptr);
		Handle<SocketBase>::operator = (ptr);
		return *this;
	}
		
	SocketHandle& operator = (const SocketHandle& socket)
	{
		assertInstance(socket.base());
		Handle<SocketBase>::operator = (socket);
		return *this;
	}
	
	virtual void assertInstance(const SocketBase* ptr) 
	{	
		if (!dynamic_cast<const SocketBase*>(ptr))
			throw Exception("Cannot assign incompatible socket");
	}

protected:
	SocketHandle() 
		//:
		// NOTE: Only compiles for derived SocketBase implementations
		//Handle<SocketT>(new SocketT)
	{
	}
};

	template<class T>
	SocketT* as()
		/// Attempt to cast and return the internal socket 
	{
		return dynamic_cast<T*>(get());
	}
*/

/*

	template<class T>
	SocketT* as()
		/// Attempt to cast and return the internal socket 
	{
		return dynamic_cast<T*>(get());
	}
*/

/*
	template<class T>
	bool is()
	{
		return as<T>() != NULL;
	}
	*/


	//Signal2<Buffer&, const net::PacketInfo&> MulticastRecv;
		/// Signals data received by the socket.
		/// The address argument is for UDP compatibility.
		/// For TCP it will always return the peerAddress.

/* //CountedBase
	typedef Handle<SocketBase> SocketHandle;
class SocketHandle: public CountedBase //CountedObject
	/// SocketBase is the abstract base interface from      
	/// which all socket contexts derive.
{
	typedef Handle<SocketBase> SocketHandle;
public:
}

SSLSocket::SSLSocket(const SocketHandle& socket) : 
	net::Socket(socket)
{
	if (!dynamic_cast<SSLBase*>(get()))
		throw Exception("Cannot assign incompatible socket");
}
*/
	//virtual void bind6(const Address& address, unsigned flags = 0) { throw Exception("Not implemented by protocol"); };