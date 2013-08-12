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


#ifndef SOURCEY_RTCP_COMPOUND_PACKET_H
#define SOURCEY_RTCP_COMPOUND_PACKET_H


#include "Sourcey/RTP/RTCP/Packet.h"
#include "Sourcey/RTP/RTCP/SenderReportPacket.h"
#include "Sourcey/RTP/RTCP/ReceiverReportPacket.h"
#include "Sourcey/RTP/RTCP/SDESPacket.h"
#include "Sourcey/RTP/RTCP/AppPacket.h"
#include "Sourcey/RTP/RTCP/ByePacket.h"
#include "Sourcey/RTP/Types.h"
#include "Sourcey/Packet.h"

#include <vector>


namespace scy {
namespace rtp {
namespace RTCP {

	
class CompoundPacket: public IPacket 
{
public:
	CompoundPacket();
	virtual ~CompoundPacket();

	// TODO: clone()

	int computedLength();

	std::vector<RTCP::Packet*>& packets();
	void addPacket(RTCP::Packet* packet);

	virtual bool read(Buffer& buffer);
	virtual void write(Buffer& buffer) const;
	
	std::string toString() const;	
	void print(std::ostream& os) const;
	
	virtual const char* className() const { return "RTCPCompoundPacket"; }

protected:
	std::vector<RTCP::Packet*> _packets;	

	int _length;

    // masks for RTCP header validation;
    static const UInt16 RTCP_VALID_MASK;
    static const UInt16 RTCP_VALID_VALUE;
};


} // namespace RTCP
} // namespace rtp
} // namespace scy 


#endif