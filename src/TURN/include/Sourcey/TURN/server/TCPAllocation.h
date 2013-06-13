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


#ifndef SOURCEY_TURN_SERVER_TCPAllocation_H
#define SOURCEY_TURN_SERVER_TCPAllocation_H


#include "Sourcey/TURN/server/ServerAllocation.h"
#include "Sourcey/TURN/server/TCPClientConnection.h"
#include "Sourcey/TURN/server/TCPPeerConnection.h"
#include "Sourcey/Net/TCPSocket.h"
#include "Sourcey/Net/SocketAcceptor.h"


namespace Scy {
namespace TURN {

	
class Server;
class TCPControlConnection;


class TCPAllocation: public ServerAllocation
{
public:
	TCPAllocation(Server& server, 
				  Net::TCPPacketSocket* control, 
				  const FiveTuple& tuple, 
				  const std::string& username, 
				  const UInt32& lifetime);
	virtual ~TCPAllocation();
	
	bool handleRequest(const Request& request);	
	void handleConnectRequest(const Request& request);
	void handleConnectionBindRequest(const Request& request);
		
	bool isOK();

	void sendToControl(STUN::Message& message);

	Net::TCPPacketSocket* control();
	Net::Address relayedAddress() const;
	ClientConnectionManager& clients();
	PeerConnectionManager& peers();
	
	bool onTimer();
	void onControlDisconnect(void* sender);
	void onPeerAccepted(void* sender, Poco::Net::StreamSocket& sock, Net::Reactor& reactor);
	void onPeerConnectSuccess(void* sender); 
	void onPeerConnectError(void* sender, int& errnum, const std::string& error);
	//void onPeerDisconnected(void* sender);
	//void onClientDisconnect(void* sender);

	virtual const char* className() const { return "TCPAllocation"; };

protected:
	Net::TCPPacketSocket*	_control;
	ClientConnectionManager	_clients;
	PeerConnectionManager	_peers;
	Net::TCPSocketAcceptor	_acceptor;
	mutable Poco::FastMutex	_mutex;
};


} } //  namespace Scy::TURN


#endif // SOURCEY_TURN_SERVER_TCPAllocation_H