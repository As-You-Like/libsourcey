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


#ifndef SOURCEY_Queue_H
#define SOURCEY_Queue_H



#include "Poco/Condition.h"
#include <queue>


namespace scy {


template<typename T>
class Queue
	// Implements a thread-safe concurrent queue.
{
private:
    std::queue<T> _queue;
	mutable Mutex _mutex;

public:
    void push(const T& data)
    {
		ScopedLock lock(_mutex);
        _queue.push(data);
    }

    bool empty() const
    {
		ScopedLock lock(_mutex);
        return _queue.empty();
    }

    T& front()
    {
		ScopedLock lock(_mutex);
        return _queue.front();
    }
    
    T const& front() const
    {
		ScopedLock lock(_mutex);
        return _queue.front();
    }

    T& back()
    {
		ScopedLock lock(_mutex);
        return _queue.back();
    }
    
    T const& back() const
    {
		ScopedLock lock(_mutex);
        return _queue.back();
    }

    void pop()
    {
		ScopedLock lock(_mutex);
        _queue.pop();
    }

    void popFront()
    {
		ScopedLock lock(_mutex);
        _queue.pop_front();
    }
};


template<typename T>
class ConcurrentQueue
	// Implements a simple thread-safe multiple producer, 
	// multiple consumer queue. 
{
private:
    std::queue<T> _queue;
	mutable Mutex _mutex;
	Poco::Condition _condition;

public:
    void push(T const& data)
    {
		ScopedLock lock(_mutex);
        _queue.push(data);
        lock.unlock();
        _condition.signal();
    }

    bool empty() const
    {
		ScopedLock lock(_mutex);
        return _queue.empty();
    }

    bool tryPop(T& popped_value)
    {
		ScopedLock lock(_mutex);
        if (_queue.empty())
            return false;
        
        popped_value = _queue.front();
        _queue.pop();
        return true;
    }

    void waitAndPpop(T& popped_value)
    {
		ScopedLock lock(_mutex);
        while (_queue.empty())
			_cond.wait(_mutex);
        
        popped_value = _queue.front();
        _queue.pop();
    }
};


} // namespace scy



#endif // SOURCEY_Queue_H