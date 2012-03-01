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


#ifndef SOURCEY_RTCP_PACKETIZER_H
#define SOURCEY_RTCP_PACKETIZER_H


#include "Sourcey/IPacketProcessor.h"
#include "Sourcey/Signal.h"
#include "Sourcey/RTP/Packet.h"


namespace Sourcey {
namespace RTP {
namespace RTCP {


class Packetizer: public RTP::Packetizer
	/// This class extends the RTP Packetizer to include
	/// automated and custom RTCP packet generation.
	///
	/// TODO: Implement me!
{
public:
	Packetizer() :
		_nRTPRcv(0), 
		_nRTPSend(0), 
		_timeLastRTCPSent(0), 
		_rtcpReportInterval(500), 
		_bytesRTPSend(0)
	{ 
	} 
	
	virtual void process(IPacket& packet)
		// Sends a packet to the handler and increments the 
		// sequence counter.
	{
		// TODO: Implement me!

		RTP::Packetizer::process(packet);
		
		if (_rtcpReportInterval.expired()) {
			_rtcpReportInterval.reset();

			// TODO: Send RTCP report...
		}
	}


protected:
	UInt64 _nRTPRcv; 
	UInt64 _nRTPSend; 
	UInt64 _bytesRTPSend; 
	UInt64 _timeLastRTCPSent;
	Timeout _rtcpReportInterval;
};


} // namespace RTCP
} // namespace RTP
} // namespace Sourcey 


#endif