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


#include "Sourcey/SocketIO/Transaction.h"
#include "Sourcey/SocketIO/Client.h"
#include "Sourcey/Logger.h"
#include <iostream>


using namespace std;
using namespace Poco;


namespace Sourcey {
namespace SocketIO {


Transaction::Transaction(Runner& runner, Client& client, int retries, long timeout) : 
	PacketTransaction<Packet>(runner, retries, timeout), client(client)
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Creating" << endl;
}


Transaction::Transaction(Runner& runner, Client& client, const Packet& request, int retries, long timeout) : 
	PacketTransaction<Packet>(runner, request, retries, timeout), client(client)
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Creating" << endl;
}


Transaction::Transaction(Client& client, int retries, long timeout) : 
	PacketTransaction<Packet>(retries, timeout), client(client)
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Creating" << endl;
}


Transaction::Transaction(Client& client, const Packet& request, int retries, long timeout) : 
	PacketTransaction<Packet>(request, retries, timeout), client(client)
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Creating" << endl;
}


Transaction::~Transaction() 
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Destroying" << endl;	
}


bool Transaction::send()
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Sending" << endl;	
	_request.setAck(true);	
	client += packetDelegate(this, &Transaction::onPotentialResponse, 100);
	if (client.send(_request), true)
		return PacketTransaction<Packet>::send();
	return false;
}

	
void Transaction::onPotentialResponse(void*, Packet& packet)
{
	//Log("debug") << "[SocketIOTransaction:" << this << "] Potential Response: " 
	//	<< packet.className() << endl;	
	if (process(packet))
		throw StopPropagation();
}


bool Transaction::match(const Packet& packet) 
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Match: "
		<< "\n\tRequest: " << _request.id()
		<< "\n\tResponse: " << packet.id()
		<< endl;	

	return _request.id() == packet.id();
}


void Transaction::onComplete()
{
	Log("debug") << "[SocketIOTransaction:" << this << "] Closing" << endl;

	client -= packetDelegate(this, &Transaction::onPotentialResponse);
	
	PacketTransaction<Packet>::onComplete();
}


} } // namespace Sourcey::SocketIO