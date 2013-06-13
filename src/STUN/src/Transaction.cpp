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


#include "Sourcey/STUN/Transaction.h"
#include "Sourcey/Logger.h"
#include <iostream>


using namespace std;
using namespace Poco;

using Scy::Net::Transaction;
using Scy::Net::IPacketSocket;
using Scy::Net::Address;


namespace Scy {
namespace STUN {


Transaction::Transaction(Runner& runner, 
						 IPacketSocket* socket, 
						 const Address& localAddress, 
						 const Address& peerAddress,
						 long timeout, 
						 int retries) : 
	Net::Transaction<Message>(runner, socket, localAddress, peerAddress, timeout, retries)
{
	LogDebug() << "[STUNTransaction:" << this << "] Initializing" << std::endl;
}


Transaction::~Transaction() 
{
	LogDebug() << "[STUNTransaction:" << this << "] Destroying" << std::endl;	
}


bool Transaction::match(const Message& message) 
{
	//LogDebug() << "[STUNTransaction:" << this << "] Match" << std::endl;	

	return Net::Transaction<Message>::match(message) 
		&& _request.transactionID() == message.transactionID();
}


void Transaction::onResponse()
{
	LogDebug() << "[STUNTransaction:" << this << "] On Response" << std::endl;	
	_response.setType(_request.type());
	_response.setState(Message::SuccessResponse);
	if (_response.get<STUN::ErrorCode>())
		_response.setState(Message::ErrorResponse);
	else if (_response.type() == Message::SendIndication ||
		_response.type() == Message::DataIndication)
		_response.setState(Message::Indication);

	Net::Transaction<Message>::onResponse();
}



} } // namespace Scy::STUN





/*
bool Transaction::match(const Message& message, const Net::Address& localAddress, const Net::Address& peerAddress)
{
	LogDebug() << "[STUNTransaction:" << this << "] Match" << std::endl;	

	return _request.transactionID() == message.transactionID()
		&& _localAddress == localAddress
		&& _peerAddress == peerAddress;
}
*/


/*
	response = response;
void Transaction::onPacketReceived(void* sender, Message& message) //, Net::IPacketSocket& socket, const Net::Address& localAddress, const Net::Address& peerAddress
{
	Net::PacketInfo* source = reinterpret_cast<Net::PacketInfo*>(message.info);
	assert(source);
	if (!source)
		return;

	assert(&socket);
	assert(&socket == &source->socket);

	if (match(message, source->localAddress, source->peerAddress)) {	
		update(message);
		socket.detach(PolymorphicDelegate<Transaction, Message>(this, &Transaction::onPacketReceived));
		Timer::getDefault().stop(TimerCallback<Transaction>(this, &Transaction::onTransactionTimeout));
		setState(this, TransactionState::Success);
		throw StopPropagation();
	}
}
*/

/*
void Transaction::cancel() 
{
	LogDebug() << "[STUNTransaction:" << this << "] Cancelling" << std::endl;
	socket.detach(PolymorphicDelegate<Transaction, Message>(this, &Transaction::onPacketReceived, 0));
	Timer::getDefault().stop(TimerCallback<Transaction>(this, &Transaction::onTransactionTimeout));
}

bool Transaction::receive(const Message& message, const Net::Address& localAddress, const Net::Address& peerAddress)
{	
	if (match(message, localAddress, peerAddress)) {	
		update(message);
		LogDebug() << "[STUNTransaction:" << this << "] Transaction Response Received: " << response.toString() << std::endl;
		Timer::getDefault().stop(TimerCallback<Transaction>(this, &Transaction::onTransactionTimeout));
		setState(this, TransactionState::Success);
		return true;
	}
}
*/