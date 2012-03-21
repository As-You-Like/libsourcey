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


#include "Sourcey/TURN/server/Server.h"
#include "Sourcey/Logger.h"
#include "Sourcey/Util.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>


using namespace std;
using namespace Poco;


namespace Sourcey {
namespace TURN {


IAllocation::IAllocation(const FiveTuple& tuple, 
						 const std::string& username, 
						 UInt32 lifetime) : 
	_createdAt(static_cast<UInt32>(time(0))), 
	_updatedAt(static_cast<UInt32>(time(0))), 
	_username(username), 
	_lifetime(lifetime), 
	_bandwidthLimit(0),
	_bandwidthUsed(0),
	_tuple(tuple)
{	
}


IAllocation::~IAllocation() 
{
	Log("debug") << "[Allocation:" << this << "] Destroying" << endl;	
	_permissions.clear();
}


void IAllocation::updateUsage(UInt32 numBytes)
{
	FastMutex::ScopedLock lock(_mutex);
	_updatedAt = static_cast<UInt32>(time(0));
	_bandwidthUsed += numBytes;
}


UInt32 IAllocation::timeRemaining()
{
	FastMutex::ScopedLock lock(_mutex);
	UInt32 remaining = static_cast<UInt32>(_lifetime - (time(0) - _updatedAt));
	return remaining > 0 ? remaining : 0;
}


bool IAllocation::expired() 
{
	return timeRemaining() == 0;
}


bool IAllocation::isOK() 
{
	return timeRemaining() > 0 
		&& bandwidthRemaining() > 0;
}


void IAllocation::setLifetime(UInt32 lifetime)
{
	FastMutex::ScopedLock lock(_mutex);
	_lifetime = lifetime;
	_updatedAt = static_cast<UInt32>(time(0));
	Log("debug") << "[Allocation:" << this << "] Updating Lifetime: " << _lifetime << endl;
}


void IAllocation::setBandwidthLimit(UInt32 numBytes)
{
	FastMutex::ScopedLock lock(_mutex);
	_bandwidthLimit = numBytes;
}


UInt32 IAllocation::bandwidthLimit()
{
	FastMutex::ScopedLock lock(_mutex);
	return _bandwidthLimit;
}


UInt32 IAllocation::bandwidthUsed()
{
	FastMutex::ScopedLock lock(_mutex);
	return _bandwidthUsed;
}


UInt32 IAllocation::bandwidthRemaining()
{
	FastMutex::ScopedLock lock(_mutex);
	return _bandwidthLimit > 0
		? (_bandwidthLimit > _bandwidthUsed 
			? _bandwidthLimit - _bandwidthUsed : 0) : 99999999;
}


FiveTuple& IAllocation::tuple() 
{ 
	FastMutex::ScopedLock lock(_mutex);
	return _tuple; 
}


std::string IAllocation::username() const 
{ 
	FastMutex::ScopedLock lock(_mutex);
	return _username; 
}


UInt32 IAllocation::lifetime() const 
{ 
	FastMutex::ScopedLock lock(_mutex);
	return _lifetime; 
}


PermissionList IAllocation::permissions() const 
{ 
	FastMutex::ScopedLock lock(_mutex);
	return _permissions; 
}


void IAllocation::addPermission(const Net::IP& ip) 
{
	FastMutex::ScopedLock lock(_mutex);

	// If the permission is already in the list then refresh it.
	for (PermissionList::iterator it = _permissions.begin(); it != _permissions.end(); ++it) {
		if ((*it).ip == ip) {		
			Log("debug") << "[Allocation:" << this << "] Refreshing permission: " << ip.toString() << endl;
			(*it).refresh();
			return;
		}
	}

	// Otherwise create it...
	Log("debug") << "[Allocation:" << this << "] Creating permission: " << ip.toString() << endl;
	_permissions.push_back(Permission(ip));
}


void IAllocation::addPermissions(const IPList& ips)
{
	for (IPList::const_iterator it = ips.begin(); it != ips.end(); ++it) {
		addPermission(*it);
	}
}


void IAllocation::removePermission(const Net::IP& ip) 
{
	FastMutex::ScopedLock lock(_mutex);

	for (PermissionList::iterator it = _permissions.begin(); it != _permissions.end();) {
		if ((*it).ip == ip) {
			it = _permissions.erase(it);
			return;
		} else 
			++it;
	}	
}


void IAllocation::removeAllPermissions()
{
	FastMutex::ScopedLock lock(_mutex);
	_permissions.clear();
}


void IAllocation::removeExpiredPermissions() 
{
	FastMutex::ScopedLock lock(_mutex);
	for (PermissionList::iterator it = _permissions.begin(); it != _permissions.end();) {
		if ((*it).timeout.expired()) {
			Log("info") << "[Allocation:" << this << "] Removing Expired Permission: " << (*it).ip.toString() << endl;
			it = _permissions.erase(it);
		} else 
			++it;
	}
}


bool IAllocation::hasPermission(const Net::IP& peerIP) 
{
	for (PermissionList::iterator it = _permissions.begin(); it != _permissions.end(); ++it) {
		if (*it == peerIP) {		
			Log("debug") << "[Allocation:" << this << "] Has permission for: " << peerIP.toString() << endl;
			return true;
		}
	}
	Log("debug") << "[Allocation:" << this << "] No permission for: " << peerIP.toString() << endl;
	return false;
}


} } // namespace Sourcey::TURN