///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup av
/// @{


#ifndef SCY_AV_VideoDecoder_H
#define SCY_AV_VideoDecoder_H


#include "scy/base.h"

#ifdef HAVE_FFMPEG

#include "scy/av/types.h"
// #include "scy/av/format.h"
// #include "scy/av/ffmpeg.h"
#include "scy/av/videocontext.h"


namespace scy {
namespace av {


struct VideoDecoder : public VideoContext
{
    VideoDecoder(AVStream* stream);
    virtual ~VideoDecoder();

    virtual void create();
    virtual void open();
    virtual void close();

    /// Decodes a the given input packet.
    /// Input packets should use the raw `AVStream` time base. Time base
    /// conversion will happen internally.
    /// Returns true an output packet was was decoded, false otherwise.
    virtual bool decode(AVPacket& ipacket);

    /// Flushes buffered frames.
    /// This method should be called after decoding
    /// until false is returned.
    virtual void flush();
};


} // namespace av
} // namespace scy


#endif
#endif // SCY_AV_VideoDecoder_H


/// @\}
