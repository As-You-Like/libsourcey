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

#ifndef SOURCEY_IRegistry_H
#define SOURCEY_IRegistry_H


#include "Sourcey/Signal.h"

#include <map>


namespace Sourcey {


template<class ItemT> 
class IRegistry 
{
	template<typename T> ItemT* createT() { return new T(); }

public:
	typedef std::map<std::string, ItemT*(*)()> TypeMap;

	IRegistry() {};
	virtual ~IRegistry() {};

    virtual ItemT* createInstance(const std::string& s) 
	{
        TypeMap::iterator it = _types.find(s);
        if (it == _types.end())
            return NULL;
        return it->second();
    }
	
	template<typename T>
    void registerType(const std::string& s)	
	{ 
        _types.insert(std::make_pair(s, &createT<T>));
		TypeRegistered.dispatch(this, s);
    }
	
    virtual void unregisterType(const std::string& s)	
	{ 
        TypeMap::iterator it = _types.find(s);
        if (it == _types.end())
            return;
		_types.erase(it);
		TypeUnregistered.dispatch(this, s);
    }

    TypeMap types() const { return _types; }

	Signal<const std::string&> TypeRegistered;
	Signal<const std::string&> TypeUnregistered;

private:
    TypeMap _types;
};


} // namespace Sourcey


#endif // SOURCEY_IRegistry_H