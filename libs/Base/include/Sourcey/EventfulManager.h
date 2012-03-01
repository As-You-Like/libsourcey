//
// This software is copyright by Sourcey <mail@sourcey.com> and is distributed under a dual license:
// Copyright (C) 2005 Sourcey
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


#ifndef SOURCEY_EventfulManager_H
#define SOURCEY_EventfulManager_H


#include "Sourcey/BasicManager.h"
#include "Sourcey/Signal.h"


namespace Sourcey { 

	
template <class TKey, class TValue>
class EventfulManager: public BasicManager<TKey, TValue>
{	
public:
	typedef BasicManager<TKey, TValue> Base;

public:	
	virtual void onAdd(const TKey&, TValue* item) 
	{
		ItemAdded.dispatch(this, *item);
	}

	virtual void onRemove(const TKey&, TValue* item) 
	{ 
		ItemRemoved.dispatch(this, *item); 
	}

	Signal<TValue&>			ItemAdded;
	Signal<const TValue&>	ItemRemoved;	
};



} // namespace Sourcey


#endif // SOURCEY_EventfulManager_H