///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup socketio
/// @{


#include "scy/socketio/transaction.h"
#include "scy/logger.h"
#include "scy/socketio/client.h"
#include <iostream>


using std::endl;


namespace scy {
namespace sockio {


Transaction::Transaction(Client& client, long timeout)
    : PacketTransaction<Packet>(timeout, 0, client.ws().socket->loop())
    , client(client)
{
    STrace << "Create" << endl;
}


Transaction::Transaction(Client& client, const Packet& request, long timeout)
    : PacketTransaction<Packet>(request, timeout, 0, client.ws().socket->loop())
    , client(client)
{
    STrace << "Create" << endl;
}


Transaction::~Transaction()
{
    STrace << "Destroy" << endl;
}


bool Transaction::send()
{
    STrace << "Send: " << _request.id() << endl;
    _request.setAck(true);
    client += packetSlot(this, &Transaction::onPotentialResponse, -1, 100);
    if (client.send(_request))
        return PacketTransaction<Packet>::send();
    return false;
}


void Transaction::onPotentialResponse(sockio::Packet& packet)
{
    STrace << "On potential response: " << packet.id() << endl;
    PacketTransaction<Packet>::handlePotentialResponse(packet);
}


bool Transaction::checkResponse(const Packet& packet)
{
    STrace << "Check response: " << packet.id() << endl;
    return _request.id() == packet.id();
}


void Transaction::onResponse()
{
    STrace << "On success" << endl;
    client -= packetSlot(this, &Transaction::onPotentialResponse);
    PacketTransaction<Packet>::onResponse();
}


} // namespace sockio
} // namespace scy


/// @\}
