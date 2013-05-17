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


#ifndef SOURCEY_Token_H
#define SOURCEY_Token_H


#include "Sourcey/Types.h"
#include "Sourcey/Timeout.h"
#include "Sourcey/CryptoProvider.h"


namespace Scy {


class Token: public Timeout
	/// Implements a token that expires 
	/// after the specified duration.
{
public:
	Token(long duration) : 
		Timeout(duration), _id(CryptoProvider::generateRandomKey(32)) {}

	Token(const std::string& id = CryptoProvider::generateRandomKey(32), long duration = 10000) : 
		Timeout(duration), _id(id) {}
	
	std::string id() const { return _id; }
	
	bool operator == (const Token& r) const {
		return id()  == r.id();
	}
	
	bool operator == (const std::string& r) const {
		return id() == r;
	}

protected:
	std::string _id;
};


typedef std::vector<Token> TokenList;


} // namespace Scy


#endif
