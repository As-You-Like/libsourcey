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


#include "Sourcey/Idler.h"
#include "Sourcey/Logger.h"


using namespace std;


namespace scy {
namespace uv {

	
Idler::Idler() //uv::Loop& loop) :
	//Base<Base>(loop)
{
	traceL("Idler", this) << "Creating" << endl;
}

	
Idler::~Idler() 
{	
	traceL("Idler", this) << "Destroying" << endl;
}


void Idler::init()
{
	_handle.data = this;
    uv_idle_init(loop(), &_handle);
}
	

void Idler::start() 
{		
    uv_idle_start(&_handle, uv::onIdle);
}


void Idler::stop() 
{
	uv_idle_stop(&_handle);
}


void Idler::onIdle(int status) 
{
	traceL("Idler", this) << "On Idle: " << status << endl;
}


} } // namespace scy::uv