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


#ifndef SOURCEY_PacketStream_H
#define SOURCEY_PacketStream_H


#include "Sourcey/Logger.h"
#include "Sourcey/Stateful.h"
#include "Sourcey/IStartable.h"
#include "Sourcey/PacketEmitter.h"
#include "Sourcey/IPacketProcessor.h"

#include "Poco/Mutex.h"
#include "Poco/Event.h"


namespace scy {


struct PacketAdapterReference
	/// Provides a reference to a PacketEmitter instance.
{
	PacketEmitter* ptr;
	int order;
	bool freePointer;	
	bool syncState;

	PacketAdapterReference(PacketEmitter* ptr = NULL, int order = 0, 
		bool freePointer = true, bool syncState = false) : 
		ptr(ptr), order(order), freePointer(freePointer), 
		syncState(syncState) {}
		
	static bool CompareOrder(const PacketAdapterReference& l, const PacketAdapterReference& r) 
	{
		return l.order < r.order;
	}
};


typedef std::vector<PacketAdapterReference> PacketAdapterList;


struct PacketStreamState: public State 
{
	enum Type 
	{
		None = 0,
		Running,
		Stopped,
		Resetting,
		Closing,
		Closed,
		Error,
	};

	std::string str(unsigned int id) const 
	{ 
		switch(id) {
		case None:				return "None";
		case Running:			return "Running";
		case Stopped:			return "Stopped";
		case Resetting:			return "Resetting";
		case Closing:			return "Closing";
		case Closed:			return "Closed";
		case Error:				return "Error";
		default:				assert(false);
		}
		return "undefined"; 
	}
};


class PacketStream: public PacketEmitter, public StatefulSignal<PacketStreamState>, public IStartable
	/// This class provides an interface for processing packets.
	/// A packet stream consists of a single PacketEmitter,
	/// one or many IPacketProcessor instances, and one or many 
	/// callback receivers.
{ 
public:	
	PacketStream(const std::string& name = ""); 
	virtual ~PacketStream();
	
	virtual void start();
	virtual void stop();

	virtual void reset();
		/// Resets the internal state of all packet dispatchers
		/// in the stream. This is useful for correcting timestamp 
		/// and metadata generation in some cases.

	virtual void close();
		/// Closes the stream and transitions the internal state
		/// to Disconnected. This method is called by the destructor.
	
	virtual void attach(PacketEmitter* source, bool freePointer = true, bool syncState = false);
		/// Attaches a source packet emitter to the stream.
		/// If freePointer is true, the pointer will be deleted when
		/// the stream is closed.
		/// If syncState and the PacketEmitter inherits from IStratable
		/// the source's start() and stop() methods will be called
		/// when the stream  is started and stopped.

	virtual bool detach(PacketEmitter* source);
		/// Detaches a source packet emitter to the stream.
		/// NOTE: If you use this method the pointer will not be
		/// freed when the stream closes, even if freePointer was  
		/// true when calling attach().

	virtual void attach(IPacketProcessor* proc, int order = 0, bool freePointer = true);
		/// Attaches a source packet emitter to the stream.
		/// The order value determines the order in which stream 
		/// processors are called.
		/// If freePointer is true, the pointer will be deleted when
		/// the stream closes.

	virtual bool detach(IPacketProcessor* proc);
		/// Detaches a source packet processor to the stream.
		/// NOTE: If you use this method the pointer will not be
		/// freed when the stream closes, even if freePointer was  
		/// true when calling attach().
	
	virtual void attach(const PacketDelegateBase& delegate);
	virtual bool detach(const PacketDelegateBase& delegate);

	virtual int numSources() const;
	virtual int numProcessors() const;
	virtual int numAdapters() const;

	PacketAdapterList adapters() const;
		/// Returns a list of all stream sources and processors.
	
	virtual std::string name() const;
		/// Returns the name of the stream.
	
	virtual bool isRunning() const;
		/// Returns true is the stream is in the Running state.
	
	virtual bool waitForReady();
		/// Locks until the internal ready event is signalled.
		/// This enables safe stream adapter access after calling
		/// stop() by waiting until the current adapter queue
		/// iteration is complete.
	
	virtual void setClientData(void* data);
	virtual void* clientData() const;
		/// Provides the ability to associate a non managed
		/// arbitrary client data pointer with the stream.
	
	template <class AdapterT>
	AdapterT* getSource(int index = 0)
	{
		int x = 0;
		for (unsigned i = 0; i < _sources.size(); i++) {
			AdapterT* source = dynamic_cast<AdapterT*>(_sources[i].ptr);
			if (source) {
				if (index == x)
					return source;
				else x++;
			}
		}
		return NULL;
	}

	template <class AdapterT>
	AdapterT* getProcessor(int index = 0)
	{
		int x = 0;
		for (unsigned i = 0; i < _processors.size(); i++) {
			AdapterT* processor = dynamic_cast<AdapterT*>(_processors[i].ptr);
			if (processor) {
				if (index == x)
					return processor;
				else x++;
			}
		}
		return NULL;
	}

protected:
	virtual void cleanup();
		/// Detaches all stream adapters, and frees 
		/// pointers if the adapters are managed.
	
	virtual void onSourcePacket(void* sender, IPacket& packet);
	virtual void onDispatchPacket(void* sender, IPacket& packet);
	virtual void onStateChange(PacketStreamState& state, const PacketStreamState& oldState);

protected:
	mutable Poco::FastMutex	_mutex;

	std::string _name;
	Poco::Event _ready;
	PacketAdapterList _sources;
	PacketAdapterList _processors;
	void* _clientData;
};


} // namespace scy


#endif // SOURCEY_PacketStream_H