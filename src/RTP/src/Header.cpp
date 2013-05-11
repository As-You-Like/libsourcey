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


#include "Sourcey/RTP/Header.h"
#include "Sourcey/RTP/Packet.h"
#include "Sourcey/ByteOrder.h"
#include "Sourcey/Logger.h"


using namespace std;
using namespace Scy::Util;


namespace Scy {
namespace RTP {


Header::Header() :
	version(kRtpVersionNumber),
	padding(false),
	extension(0),
	csrcCount(0),
	marker(false),
	payloadType(0),
	sequenceNumber(0),
	timestamp(0),
	ssrc(0)
#ifdef USING_RTP_TCP // not properly implemented
	, tcpFriendlyMode(false),
	sendingTimestamp(0),
	rttEstimate(0),
#endif 
{
}


size_t Header::size() const 
{  
	//assert(this->csrcList.size() > 0);
	assert(this->csrcList.size() < kRtpMaxSources);
	size_t sz = kRtpBaseHeaderSize + (int)csrcList.size() * kBytesPerCSRC;
#ifdef USING_RTP_TCP
	sz += 8; // 4(rtt) + 4(sendts)
#endif
	return sz;
}
	

int Header::read(const UInt8* buffer, int bufferSize)
{
	if (bufferSize < kRtpBaseHeaderSize) {
		LogError() << "RTP: Header: Buffer size too small: "
			<< bufferSize << ": " << kRtpBaseHeaderSize << endl;
		return -1;
	}

	int version = ExtractBits(buffer[0], 2, 6);
	if (version != kRtpVersionNumber) {
		LogError() << "RTP: Header: Wrong version number: " 
			<< version << ": " << kRtpVersionNumber << endl;
		return -1;
	}

	this->padding = ExtractBits(buffer[0], 1, 5) != 0;
	this->extension = ExtractBits(buffer[0], 1, 4) != 0;
	this->csrcCount = ExtractBits(buffer[0], 4, 0);

	this->marker = ExtractBits(buffer[1], 1, 7) != 0;
	this->payloadType = ExtractBits(buffer[1], 7, 0);

	this->sequenceNumber = GetBE16(buffer + 2);
	this->timestamp = GetBE32(buffer + 4);
	this->ssrc = GetBE32(buffer + 8);

	assert(this->csrcCount < 16);

	if (bufferSize < this->size()) {
		LogError() << "RTP: Header: Buffer size too small for header: "
			<< bufferSize << ": " << size() << endl;
		return -1;
	}
	for (int i = 0; i < this->csrcCount; i++) {
		this->csrcList.push_back(GetBE32(buffer + i * 4 + 12));
	}
	return size();
}


void Header::write(UInt8* buffer, int bufferSize) const
{  
	assert(this->csrcList.size() == this->csrcCount);
	assert(this->csrcCount < kRtpMaxSources);
	assert(this->payloadType < 1 << 7);
	assert(this->size() < bufferSize);

	buffer[0] = (kRtpVersionNumber << 6) |
      ((UInt8)this->padding << 5) |
      ((UInt8)this->extension << 4) |
      this->csrcCount;
	buffer[1] = ((UInt8)this->marker << 7) |
      this->payloadType;
	SetBE16(buffer + 2, this->sequenceNumber);
	SetBE32(buffer + 4, this->timestamp);
	SetBE32(buffer + 8, this->ssrc);

	for (int i = 0; i < csrcList.size(); i++) {
		SetBE32(buffer + i * 4 + 12, this->csrcList[i]);
	}
}

	
bool Header::read(Buffer& buffer)
{
	assert(buffer.position() == 0);
	int sz = read(reinterpret_cast<const UInt8*>(buffer.data()), static_cast<int>(buffer.size()));
	buffer.consume(sz);
	return sz > 0;
}


void Header::write(Buffer& buffer) const
{	
	assert(buffer.position() == 0);
	assert(buffer.capacity() > this->size());
	write(reinterpret_cast<UInt8*>(buffer.data()), static_cast<int>(buffer.capacity()));
	buffer.setSize(this->size());
}


} // namespace RTP
} // namespace Scy 
