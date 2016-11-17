///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup util
/// @{


#ifndef SCY_UserManager_H
#define SCY_UserManager_H

#include "scy/collection.h"
#include <map>


namespace scy {


struct IUser
{
    virtual std::string username() const= 0;
    virtual std::string password() const= 0;
};


class BasicUser : public IUser
{
public:
    BasicUser(const std::string& username, const std::string& password= "")
        : _username(username)
        , _password(password)
    {
    }

    std::string username() const { return _username; }
    std::string password() const { return _password; }

protected:
    std::string _username;
    std::string _password;
};


typedef std::map<std::string, IUser*> IUserMap;


/// @deprecated
/// This class contains a list of users that have access
/// on the system.
class UserManager : public LiveCollection<std::string, IUser>
{
public:
    typedef LiveCollection<std::string, IUser> Manager;
    typedef Manager::Map Map;

public:
    UserManager(){};
    virtual ~UserManager(){};

    virtual bool add(IUser* user)
    {
        return Manager::add(user->username(), user);
    };
};


} // namespace scy


#endif // SCY_UserManager_H


/// @\}
