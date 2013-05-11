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


#include "Sourcey/Symple/Address.h"
#include "Sourcey/Util.h"
#include "sstream"
#include "assert.h"


using namespace std;


namespace Scy {
namespace Symple {


Address::Address() :
	JSON::Value(Json::objectValue)
{
}


Address::Address(const Address& r) :
	JSON::Value(r)
{
}


Address::Address(const JSON::Value& r) :
	JSON::Value(r)
{
}


Address::~Address()
{
}


/*
Address::Address(const string& id)
{
	parse(id);
}
	

bool Address::parse(const string& id)
{
	StringVec elems;
	Util::split(id, ':', elems);
	if (elems.size() >= 1)
		group = elems[0];
	if (elems.size() >= 2)
		user = elems[1];
	if (elems.size() >= 3)
		id = elems[2];
	return valid();
}
*/


string Address::id() const 
{
	return get("id", "").asString();
}


string Address::user() const 
{
	return get("user", "").asString();
}


string Address::name() const 
{
	return get("name", "").asString();
}


string Address::group() const 
{
	return get("group", "").asString();
}


void Address::setID(const std::string& id) 
{
	(*this)["id"] = id;
}


void Address::setUser(const std::string& user) 
{
	(*this)["user"] = user;
}


void Address::setName(const std::string& name) 
{
	(*this)["name"] = name;
}


void Address::setGroup(const std::string& group) 
{
	(*this)["group"] = group;
}


bool Address::valid() const
{
	return !id().empty()
		&& !user().empty();
}


void Address::print(ostream& os) const
{
	os << group();
	os << ":";
	os << user();
	os << "(";
	os << name();
	os << ")";
	os << ":";
	os << id();
}


/*
string Address::toString() const
{
	ostringstream os;
	print(os);
	return os.str(); 
}
*/


bool Address::operator == (const Address& r)
{
	return group() == r.group()
		&& user() == r.user()
		&& id() == r.id();
}


/*
bool Address::operator == (string& r)
{
	return toString() == r;
}
*/


} // namespace Symple 
} // namespace Scy