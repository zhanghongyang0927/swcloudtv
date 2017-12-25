///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TimeStamp.h>
#include <rplayer/ts/TsCommon.h>

#include <inttypes.h>


namespace rplayer {

//
// This interface is a callback from the TsDemux to the user.
// It is called whenever a piece of data or metadata is available for the respective stream.
//

struct IDataSink
{
    IDataSink() {}
    virtual ~IDataSink() {}

    // Called upon detection or selection of a new stream in the PMT.
    virtual void newStream(StreamType streamType, const char *language) = 0;

    // Called when a new PES header is received for this stream.
    // (newStream will have been called at least once.)
    // pesPayloadLength indicates the expected length of the PES packet payload data (to be
    // passed through calls to parse()). It may be 0 in case of video streams, which means
    // that the size is unknwon.
    virtual void pesHeader(TimeStamp pts, TimeStamp dts, uint32_t pesPayloadLength) = 0;

    // Called when data is demultiplexed for this stream
    // (pesHeader will have been called at least once.)
    virtual void parse(const uint8_t *data, uint32_t size) = 0;

    // Called upon a call to TsDemux::reset() and when a stream is replaced by another.
    // It indicates a discontinuous stream, so some resynchronization is to be expected.
    // It marks the end of the current stream and the potential start of another.
    // If a new stream starts, newStream() will be called.
    virtual void reset() = 0;
};

} // namespace rplayer
