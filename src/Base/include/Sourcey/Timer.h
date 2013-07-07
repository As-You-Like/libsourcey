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


#ifndef SOURCEY_UV_Timer_H
#define SOURCEY_UV_Timer_H


#include "Sourcey/UV/UVPP.h"
#include "Sourcey/UV/Base.h"
#include "Sourcey/Types.h"
#include "Sourcey/Signal.h"


namespace scy {
namespace uv {


class Timer: public uv::Base<>
	/// Wraps libev's ev_timer watcher. Used to get woken up at a specified time
	/// in the future.
{
public:
	Timer(Int64 timeout, Int64 interval = 0, uv_loop_t* loop = uv_default_loop());
	Timer(uv_loop_t* loop = uv_default_loop());
	virtual ~Timer();
	
	virtual bool start(Int64 interval);
	virtual bool start(Int64 timeout, Int64 interval);
		/// Start the timer, an interval value of zero will only trigger
		/// once after timeout.

	virtual bool stop();
		/// Stops the timer.
	
	virtual bool restart();
		/// Restarts the timer, even if it hasn't been started yet.
		/// An interval or interval must be set or an exception will be thrown.
	
	virtual bool again();
		// Stop the timer, and if it is repeating restart it using the
		// repeat value as the timeout. If the timer has never been started
		// before it returns -1 and sets the error to UV_EINVAL.

	virtual void setInterval(Int64 interval);
		/// Set the repeat value. Note that if the repeat value is set from
		/// a timer callback it does not immediately take effect. If the timer
		/// was non-repeating before, it will have been stopped. If it was repeating,
		/// then the old repeat value will have been used to schedule the next timeout.

	virtual bool active() const;
	
	virtual Int64 timeout() const;
	virtual Int64 interval() const;
	
	Int64 count();

	void onTimeout();
	
	NullSignal Timeout;	

protected:	
	virtual void init();
	//virtual void close();

	//void updateState();
	Int64 _count;
	Int64 _timeout;
	Int64 _interval;
	//bool _active;
};


//
// UV Callbacks
//

UVEmptyStatusCallback(Timer, onTimeout, uv_timer_t);


} } // namespace scy::uv


#endif // SOURCEY_UV_Timer_H


