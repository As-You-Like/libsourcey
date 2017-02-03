///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup stun
/// @{


#ifndef SCY_STUN_Transaction_H
#define SCY_STUN_Transaction_H


#include "scy/net/socket.h"
#include "scy/net/transaction.h"
#include "scy/stun/message.h"


namespace scy {
namespace stun {


class SCY_EXTERN Transaction : public net::Transaction<Message>
{
public:
    Transaction(const net::Socket::Ptr& socket, const net::Address& peerAddress,
                long timeout = 10000, int retries = 1,
                uv::Loop* loop = uv::defaultLoop());

    virtual bool checkResponse(const Message& message);
    virtual void onResponse();

protected:
    virtual ~Transaction();
};
}
} // namespace scy:stun


#endif // SCY_STUN_Transaction_H


/// @\}
