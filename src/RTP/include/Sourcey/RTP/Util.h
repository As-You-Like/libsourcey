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


#ifndef SOURCEY_RTP_Utilities_H
#define SOURCEY_RTP_Utilities_H


#include "Sourcey/RTP/Types.h"


namespace Scy {
namespace Util {
	

static inline UInt8 ExtractBits(UInt8 byte, int bitsCount, int shift) {
  return (byte >> shift) & ((1 << bitsCount) - 1);
}


} // namespace Util
} // namespace Scy


#endif // SOURCEY_RTP_Utilities_H