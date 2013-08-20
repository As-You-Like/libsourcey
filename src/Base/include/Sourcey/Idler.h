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


#ifndef SOURCEY_Idler_H
#define SOURCEY_Idler_H


#include "Sourcey/UV/UVPP.h"
#include "Sourcey/Interface.h"

#include <functional>
//#include <condition_variable>

namespace scy {
	

struct Idler
{
	Idler(uv::Loop& loop = uv::defaultLoop()) :
		ptr(&loop, new uv_idle_t)
	{
		uv_idle_init(&loop, ptr.handle<uv_idle_t>());
		uv_unref(ptr.handle()); // don't increment main loop
	}

	struct CallbackRef
	{
		std::function<void()> func;
		Idler* self;
	} callback;
			
	void start(std::function<void()> target)
	{		
		assert(!ptr.active());	
		callback.func = target;
		callback.self = this;
		ptr.handle()->data = &callback;
		if (uv_idle_start(ptr.handle<uv_idle_t>(), [](uv_idle_t* req, int) {
			auto callback = reinterpret_cast<CallbackRef*>(req->data);
			callback->func(); // (callback->self);
		})) ptr.setAndThrowLastError("Cannot initialize idler");
	}

	void stop()
	{
		uv_idle_stop(ptr.handle<uv_idle_t>());
	}

	uv::Handle ptr;
};


} // namespace scy


#endif // SOURCEY_Idler_H


/*
public:
	
	//virtual void start();	
	//virtual void onIdle();
		// Called by the event loop thread  
		// to do asynchronous processing.

struct Idler {
	uv_idle_t handle;
	idle_callback cb;

	Idler(uv_loop_t * loop, idle_callback cb) {
		handle.data = this;
		this->cb = cb;

		uv_idle_init(loop, &handle);
		uv_idle_start(&handle, static_idle);
	}

	void idle(uv_idle_t& handle, int status) {
		if (cb(status) == 0) {
			uv_idle_stop(&handle);
			delete this;
		}
	}

	static void static_idle(uv_idle_t* handle, int status) {
		//printf("idle static notify\n");
		((Idler *)(handle->data))->idle(*handle, status); 
	}
};
*/
