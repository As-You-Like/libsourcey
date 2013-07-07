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


#ifndef SOURCEY_SDP_LINE_O_H
#define SOURCEY_SDP_LINE_O_H


#include "Sourcey/SDP/Line.h"
#include <iostream>


namespace scy {
namespace SDP { 


class O: public Line {
public:
	O(const std::string& src);
	O(const std::string& username, std::string sessionID, std::string version, std::string netType, std::string addressType, std::string address);
	virtual ~O();

	std::string username();
	void setUsername(const std::string& username);

	std::string sessionId();
	void setSessionId(const std::string& sessionID);

	std::string version();
	void setVersion(const std::string& version);

	std::string netType();
	void setNetType(const std::string& netType);

	std::string addrType();
	void setAddrType(const std::string& addressType);

	std::string address();
	void setAddr(const std::string& address);

	virtual std::string toString();

private:
	std::string _username;
	std::string _sessionID;
	std::string _version;
	std::string _netType;
	std::string _addressType;
	std::string _address;
};


} // namespace scy
} // namespace SDP 


#endif
