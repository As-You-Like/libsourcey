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


#ifndef SOURCEY_SDP_PACKET_H
#define SOURCEY_SDP_PACKET_H


#include "Sourcey/SDP/Message.h"
#include "Sourcey/SDP/Line.h"
#include "Sourcey/SDP/A.h"
#include "Sourcey/SDP/C.h"
#include "Sourcey/SDP/I.h"
#include "Sourcey/SDP/M.h"
#include "Sourcey/SDP/O.h"
#include "Sourcey/SDP/S.h"
#include "Sourcey/SDP/T.h"
#include "Sourcey/SDP/V.h"
#include "Sourcey/SDP/B.h"
#include "Sourcey/SDP/Candidate.h"
#include "Sourcey/SDP/RemoteCandidate.h"
#include "Sourcey/IPacket.h"

#include <vector>


namespace Sourcey {
namespace SDP { 


class Message: public IPacket 
{
public:
	Message();
	Message(const std::string& src);
	Message(const Message& r);
	virtual ~Message();
	
	IPacket* clone() const;

	bool read(Buffer& buf);
	bool read(const std::string& src);
	void write(Buffer& buf) const;

	bool empty() const;
	bool valid() const;

	void addLine(Line* line);

	template<typename T>
	T* getLine(const std::string& key, int index = 0) const 
	{
		int i = 0;
		for (std::vector<Line*>::const_iterator it = _lines.begin(); it != _lines.end(); it++) {
			T* l = dynamic_cast<T*>(*it);
			if (l && (*it)->toString().find(key) != std::string::npos) {
				if (index == i)
					return l;
				i++;
			}
		}
		return NULL;
	}

	template<typename T>
	T* getMediaOrSessionLevelLine(const M& m, const std::string& key, int index = 0) const 
	{
		T* line = m.getLine<T>(key, index);
		if (line)
			return line;
		return getLine<T>(key, index);
	}

	template<typename T>
	std::vector<T*> lines() const 
	{		
		std::vector<T*> l;
		for (std::vector<Line*>::const_iterator it = _lines.begin(); it != _lines.end(); it++) {
			T* line = dynamic_cast<T*>(*it);
			if (line)
				l.push_back(line);
		}
		return l; 
	}

	std::vector<Line*> lines() const;
	std::vector<M*> mediaLines() const;

	void setSessionLevelAttribute(const std::string& type, const std::string& value);
	std::string getSessionLevelAttribute(const std::string& type, int index = 0) const;
	std::string getMediaOrSessionLevelAttribute(const M& m, const std::string& type, int index = 0) const;
	bool hasSessionLevelAttribute(const std::string& type) const;

    virtual std::string contentType() const { return "application/sdp"; }	
	virtual std::string toString() const;
	virtual void print(std::ostream& os) const;

	std::string firstMediaFormat() const;
	bool isMediaFormatAvailable(const std::string& fmt) const;
	bool isICESupported() const;

	virtual const char* className() const { return "SDP::Message"; }
	
private:
	std::vector<Line*> _lines;
};


} // namespace Sourcey
} // namespace SDP 


#endif
