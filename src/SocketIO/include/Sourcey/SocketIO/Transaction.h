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


#ifndef SOURCEY_SocketIO_Transaction_H
#define SOURCEY_SocketIO_Transaction_H


#include "Sourcey/PacketTransaction.h"
#include "Sourcey/SocketIO/Packet.h"


namespace Scy {
namespace SocketIO {


class Client;


struct Transaction: public PacketTransaction<SocketIO::Packet>
{
	Transaction(SocketIO::Client& client, long timeout = 10000);
	Transaction(SocketIO::Client& client, const SocketIO::Packet& request, long timeout = 10000);
	virtual ~Transaction();
	
	virtual bool send();
	virtual bool match(const Packet& packet);

	virtual void onPotentialResponse(void*, Packet& packet);
	virtual void onResponse();
	virtual void onComplete();

	SocketIO::Client& client;
};


} } // namespace Scy::SocketIO


#endif // SOURCEY_SocketIO_Transaction_H