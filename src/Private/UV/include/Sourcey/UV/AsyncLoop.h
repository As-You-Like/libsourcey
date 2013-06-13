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


#ifndef SOURCEY_UV_AsyncLoop_H
#define SOURCEY_UV_AsyncLoop_H


#include "Sourcey/UV/UVPP.h"

//#include "TCPContext.h"

#include "Poco/Thread.h"


namespace Sourcey {
namespace UV {


class AsyncLoop: public Poco::Runnable
{
public:
	AsyncLoop(LoopHandle* loop = DefaultLoop);
	virtual ~AsyncLoop();
	
	virtual void run();
	
	//virtual TCPContext* allocateTCPContext();
	
	virtual LoopHandle* loop();

protected:
	LoopHandle*		_loop;
	bool			_stop;
	Poco::Thread	_thread;
};


} } // namespace Sourcey::UV


#endif // SOURCEY_UV_AsyncLoop_H
