//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
// LibSourcey is is distributed under a dual license that allows free, 
// open source use and closed source use under a standard commercial
// license.
//
// Non-Commercial Use:
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// Commercial Use:
// Please contact mail@sourcey.com
//


#ifndef SOURCEY_State_H
#define SOURCEY_State_H


#include "Sourcey/Signal.h"

//#include <string>
//#include <iostream>


namespace Sourcey {


struct State 
{ 
	typedef unsigned int ID;

	State(ID id = 0);

	virtual ID id() const;
	virtual void set(ID id);
	virtual std::string message() const;
	virtual void setMessage(const std::string& message);
	
	virtual std::string str(ID id) const;
	virtual std::string toString() const;

	virtual bool between(ID lid, ID rid) const
	{ 	
		int id = this->id();
		return id >= lid 
			&& id <= rid;
	}
		
	virtual bool equals(ID id) const
	{ 	
		return this->id() == id;
	}

	bool operator == (const State& r) 
	{ 
		return id() == r.id();
	}	
	
    friend std::ostream& operator << (std::ostream& os, const State& state) 
	{
		os << state.toString();
		return os;
    }

protected:
	ID _id;
	std::string _message;
};


// ---------------------------------------------------------------------
//
struct MutexState: public State
{ 
	MutexState(ID id = 0);
	MutexState(const MutexState& r) : State(r) {}

	virtual ID id() const { Poco::FastMutex::ScopedLock lock(_mutex); return _id;	}
	virtual void set(ID id) { Poco::FastMutex::ScopedLock lock(_mutex); _id = id; }
	virtual std::string message() const { Poco::FastMutex::ScopedLock lock(_mutex);	return _message; }
	virtual void setMessage(const std::string& message) { Poco::FastMutex::ScopedLock lock(_mutex);	_message = message; }

protected:
	mutable Poco::FastMutex	_mutex;
};


// ---------------------------------------------------------------------
//
struct StateSignal: public MutexState
{ 
	StateSignal(ID id = 0);
	StateSignal(const StateSignal& r);
		
	virtual bool change(ID id);
	virtual bool canChange(ID id);	
	virtual void onChange(ID id, ID prev);

	Signal2<const ID&, const ID&> Change;
		/// Fired when the state changes to signal 
		/// the new and previous states.

protected:	
	virtual void set(ID id);
};




	//virtual ID id() const { Poco::FastMutex::ScopedLock lock(_mutex); return _id;	}
	//virtual void set(ID id) { Poco::FastMutex::ScopedLock lock(_mutex); _id = id; }
	//virtual std::string message() const { Poco::FastMutex::ScopedLock lock(_mutex);	return _message; }
	//virtual void setMessage(const std::string& message) { Poco::FastMutex::ScopedLock lock(_mutex);	_message = message; }

/*
// ---------------------------------------------------------------------
//
template<typename T>
class Stateful
	/// This class implements a simple state machine.
	/// T should be a derived State.
	///
	/// This class is not inherently thread safe. 
	/// Synchronization is left to the implementation.
{
public:
	virtual bool stateEquals(ID id) const
	{ 	
		return _state.id() == id;
	}

	virtual bool stateBetween(ID lid, ID rid) const
	{ 	
		return _state.id() >= lid 
			&& _state.id() <= rid;
	}

	virtual T& state() { return _state; }
	virtual T state() const { return _state; }

protected:
	virtual bool canChangeState(ID id) 
	{
		if (state().id() != id) 
			return true;
		return false;
	}
	
	virtual void onStateChange(T&, const T&) 
	{
	}

	virtual bool setState(ID id, const std::string& message = "") 
	{ 	
		if (canChangeState(id)) {
			T& state = this->state();
			T oldState = state;
			state.set(id);	
			state.setMessage(message);	
			onStateChange(state, oldState);
			return true;
		}
		return false;
	}

	virtual void setStateMessage(const std::string& message) 
	{ 
		state().setMessage(message);	
	}

protected:
	T _state;
};


// ---------------------------------------------------------------------
//
template<typename T>
class MutexStateful: public Stateful<T>
	/// This class adds thread safety the base
	/// Stateful implementation.
{
public:
	virtual bool stateEquals(ID id) const
	{ 	
		return state().id() == id;
	}

	virtual bool stateBetween(ID lid, ID rid) const
	{ 	
		int id = state().id();
		return id >= lid 
			&& id <= rid;
	}

	virtual T& state() 
	{ 
		Poco::FastMutex::ScopedLock lock(_mutex);
		return Stateful<T>::_state; 
	}

	virtual T state() const 
	{ 
		Poco::FastMutex::ScopedLock lock(_mutex);
		return Stateful<T>::_state; 
	}

protected:
	mutable Poco::FastMutex	_mutex;
};


// ---------------------------------------------------------------------
//
template<typename T>
class StatefulSignal: public MutexStateful<T>
	/// This class adds a StateChange signal
	/// to the base implementation.
{
public:
	Signal2<T&, const T&> StateChange;
		/// Fired when the state changes to signal the
		/// new and old states.

protected:
	virtual bool setState(void* sender, ID id, const std::string& message = "") 
		/// This method is used to send the state signal
		/// after a successful state change.
	{ 
		T oldState = MutexStateful<T>::state();
		if (Stateful<T>::setState(id, message)) {
			StateChange.dispatch(sender, MutexStateful<T>::state(), oldState);
			return true;
		}
		return false;
	}
};
*/


} // namespace Sourcey


#endif // SOURCEY_Stateful_H