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


#ifndef SOURCEY_UV_TCPClientSocket_H
#define SOURCEY_UV_TCPClientSocket_H


#include "Sourcey/UV/UVPP.h"
#include "Sourcey/UV/TCPSocket.h"

#include "Sourcey/Logger.h"
#include "Sourcey/Stateful.h"
#include "Sourcey/Net/Address.h"


namespace Scy {
namespace UV {
	
	
struct ClientState: public State 
{
	enum Type 
	{
		None				= 0x00,
		Connecting			= 0x01,
		Connected			= 0x04,
		Handshaking			= 0x08,
		Online				= 0x10,
		Disconnected		= 0x20
	};

	std::string str(unsigned int id) const 
	{ 
		switch(id) {
		case None:			return "None";
		case Connecting:	return "Connecting";
		case Connected:		return "Connected";
		case Handshaking:	return "Handshaking";
		case Online:		return "Online";
		case Disconnected:	return "Disconnected";
		}
		return "undefined"; 
	};
};


class TCPClientSocket: public TCPSocket, public StatefulSignal<ClientState>
{
public:
	TCPClientSocket(uv_loop_t* loop = uv_default_loop());
	virtual ~TCPClientSocket();	
	
	virtual void connect(const Net::Address& peerAddress);	
	virtual void close();

	virtual void onConnected(int status);

	virtual void setErrno(const uv_err_t& err);
};


} } // namespace Scy::UV


#endif // SOURCEY_UV_TCPClientSocket_H
