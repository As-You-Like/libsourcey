///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup json
/// @{


#ifndef SCY_JSON_ISerializable_h
#define SCY_JSON_ISerializable_h


#include "scy/json/json.h"
#include <ostream>


namespace scy {
namespace json {


class SCY_EXTERN ISerializable
{
public:
    virtual ~ISerializable(){};
    virtual void serialize(json::value& root) = 0;
    virtual void deserialize(json::value& root) = 0;
};


inline bool serialize(ISerializable* pObj, std::string& output)
{
    if (pObj == NULL)
        return false;

    json::value serializeRoot;
    pObj->serialize(serializeRoot);

    json::StyledWriter writer;
    output = writer.write(serializeRoot);

    return true;
}


inline bool deserialize(ISerializable* pObj, std::string& input)
{
    if (pObj == NULL)
        return false;

    json::value deserializeRoot;
    json::Reader reader;

    if (!reader.parse(input, deserializeRoot))
        return false;

    pObj->deserialize(deserializeRoot);

    return true;
}


} // namespace json
} // namespace scy


#endif // SCY_JSON_ISerializable_h
