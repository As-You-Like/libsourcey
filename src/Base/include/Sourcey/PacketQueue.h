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


#ifndef SOURCEY_PacketQueue_H
#define SOURCEY_PacketQueue_H


#include "Sourcey/PacketStream.h"
#include "Sourcey/InterProcess.h"


namespace scy {
	

//
// Synchronization Packet Queue
//


class SyncPacketQueue: public PacketProcessor, public SyncQueue<IPacket>
{
public:
	SyncPacketQueue(uv::Loop& loop, int maxSize = 1024, int dispatchTimeout = DEFAULT_TIMEOUT);
	SyncPacketQueue(int maxSize = 1024, int dispatchTimeout = DEFAULT_TIMEOUT);
	virtual ~SyncPacketQueue();
	
	//virtual void close();

	virtual void process(IPacket& packet);

	PacketSignal Emitter;

protected:	
	virtual void emit(IPacket& packet);

	virtual void onStreamStateChange(const PacketStreamState&);
};


//
// Asynchronous Packet Queue
//


class AsyncPacketQueue: public PacketProcessor, public AsyncQueue<IPacket>
{
public:
	AsyncPacketQueue(int maxSize = 1024, int dispatchTimeout = DEFAULT_TIMEOUT);
	virtual ~AsyncPacketQueue();

	virtual void process(IPacket& packet);
	
	PacketSignal Emitter;

protected:	
	virtual void emit(IPacket& packet);

	virtual void onStreamStateChange(const PacketStreamState&);
};


} // namespace scy


#endif // SOURCEY_PacketQueue_H





/*
//
// Synchronized Packet Stream
//


class PacketStream: public PacketStream
{
public:
	PacketStream(uv::Loop& loop, const std::string& name = "");
	PacketStream(const std::string& name = "");

	virtual ~PacketStream(); // make ref count and protected?

	virtual void close();

protected:		
	virtual void onFinalPacket(IPacket& packet);

	SyncPacketQueue* _queue;
};
*/