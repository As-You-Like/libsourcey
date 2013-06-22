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


#include "Sourcey/Net/TCPPacketStreamConnection.h"
#include "Sourcey/Net/Types.h"
#include "Sourcey/Logger.h"


using namespace std;
using namespace Poco;


namespace scy {
namespace Net {


TCPPacketStreamConnection::TCPPacketStreamConnection(PacketStream* stream, 
													 const Poco::Net::StreamSocket& socket, 
													 //bool closeOnDisconnect, 
													 bool resetOnConnect) :
	Poco::Net::TCPServerConnection(socket),
	_stream(stream), 
	//_closeOnDisconnect(closeDisconnect), 
	_resetOnConnect(resetOnConnect) 
{
	LogDebug() << "Creating" << endl;
}
	

TCPPacketStreamConnection::~TCPPacketStreamConnection() 
{ 
	LogDebug() << "Destroying" << endl;
	stop();
}


void TCPPacketStreamConnection::run() 
{
	LogDebug() << "Starting" << endl;
	
	try 
	{
		_stream->StateChange += delegate(this, &TCPPacketStreamConnection::onStreamStateChange);	
		_stream->attach(packetDelegate(this, &TCPPacketStreamConnection::onStreamPacket));
		if (_resetOnConnect)
			_stream->reset();

		_stop.wait();

		assert(_stream == NULL);
	}
	catch (Exception& exc) {
		LogError() << "Error: " << exc.displayText() << endl;
	}

	LogDebug() << "Exiting..."<< endl;
}


void TCPPacketStreamConnection::stop()
{
	FastMutex::ScopedLock lock(_mutex);
	
	if (_stream) {
		_stream->StateChange -= delegate(this, &TCPPacketStreamConnection::onStreamStateChange);	
		_stream->detach(packetDelegate(this, &TCPPacketStreamConnection::onStreamPacket));
		_stream = NULL;
		_stop.set();
	}
}


int TCPPacketStreamConnection::send(const char* data, size_t size)
{						
	// Drop empty packets.
	if (!size) {			
		LogWarn() << "Dropping Empty Packet" << endl;
		return 0;
	}

	// Split oversize packets.
	else if (size > MAX_TCP_PACKET_SIZE) {
		
		/*
		LogTrace() << "Splitting Oversize Data Packet: " 
			<< "\n\tSize: " << size
			<< "\n\tMax Size: " << MAX_TCP_PACKET_SIZE
			<< endl;
			*/
		
		int index = 0;
		int read = 0;
		while (index < size) {		
			read = min<int>(size - read, MAX_TCP_PACKET_SIZE);
			/*
			LogTrace() << "Splitting Oversize Data Packet: " 
				<< "\n\tCurrent Index: " << index
				<< "\n\tRead Bytes: " << read
				<< "\n\tRemaining: " << (size - read)
				<< endl;
				*/
			socket().sendBytes(data + index, read);
			index += read;
		}
		
		return size;
	}

	//LogTrace() << "Sending Packet: " 
	//	<< size << ": " << string(data, min(50, size)) << endl;	
	return socket().sendBytes(data, size);
}


void TCPPacketStreamConnection::onStreamPacket(void*, IPacket& packet) 
{
	try {
		// If the packet is a DataPacket we can access
		// the data pointer directly.
		DataPacket* p = packet.as<DataPacket>();
		if (p) {
			send((const char*)p->data(), p->size());
		}
		
		// Otherwise copy the packet onto an output buffer
		// before sending it out over the network.
		else {
			size_t size = packet.size();
			Buffer buf(size ? size : 1500);
			packet.write(buf);
			send((const char*)buf.data(), buf.size());
		}
	}
	catch (Exception& exc) {
		LogError() << "Error: " << exc.displayText() << endl;		
		//if (_stream && _closeOnDisconnect)
		//	_stream->close();
		//else
		stop();

		// Rethrow the exception causing the stream to enter 
		// into error state.
		exc.rethrow();
	}
}
	

void TCPPacketStreamConnection::onStreamStateChange(void*, PacketStreamState& state, const PacketStreamState&)
{		
	LogDebug() << "Session State Changed: " << state << endl;

	if (state.id() == PacketStreamState::Closing)
		stop();
}


} } // namespace scy::Net