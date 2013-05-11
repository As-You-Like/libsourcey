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


#ifndef SOURCEY_TURN_Types_H
#define SOURCEY_TURN_Types_H


#include "Sourcey/Net/IPacketSocket.h"
#include "Sourcey/Net/Address.h"
#include "Sourcey/STUN/Message.h"


namespace Scy {
namespace TURN {

	
enum AuthenticationState 
{
	Authenticating	= 1,
	Authorized		= 2,
	QuotaReached	= 3,
	Unauthorized	= 4
};


struct Request: public STUN::Message
{
	Net::IPacketSocket& socket;
	Net::Address localAddress;
	Net::Address remoteAddr;

	Request(Net::IPacketSocket& socket, 
			const STUN::Message& message, 
			const Net::Address& localAddress = Net::Address(), 
			const Net::Address& remoteAddr = Net::Address()) :
		STUN::Message(message), 
		socket(socket), 
		localAddress(localAddress), 
		remoteAddr(remoteAddr) {}

	Request(const Request& r) :
		STUN::Message(r), 
		socket(r.socket), 
		localAddress(r.localAddress), 
		remoteAddr(r.remoteAddr) {}

private:
	Request& operator = (const Request&) {}
};


typedef std::vector<Net::IP> IPList;


} } // namespace Scy::TURN


#endif // SOURCEY_TURN_Types_H
