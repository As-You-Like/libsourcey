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


#include "Sourcey/SDP/I.h"


using namespace std;


namespace Sourcey {
namespace SDP { 


I::I(const string& src) : Line(Line::I, 9) {
	assert(src.substr(0, 2) == "i=");
	_sessionInformation = Util::trim(src.substr(2, src.length()-2));
}


I::~I() {
}


string I::sessionInformation() {
	return _sessionInformation;
}


void I::setSessionInformation(const string& s) {
	_sessionInformation = s;
}


string I::toString() {
	return "i=" + _sessionInformation;
}


} // namespace Sourcey
} // namespace SDP 
