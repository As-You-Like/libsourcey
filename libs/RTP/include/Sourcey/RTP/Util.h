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


#ifndef SOURCEY_RTP_Utilities_H
#define SOURCEY_RTP_Utilities_H


#include "Sourcey/RTP/Types.h"


namespace Sourcey {
namespace Util {
	

static inline UInt8 ExtractBits(UInt8 byte, int bitsCount, int shift) {
  return (byte >> shift) & ((1 << bitsCount) - 1);
}


} // namespace Util
} // namespace Sourcey


#endif // SOURCEY_RTP_Utilities_H