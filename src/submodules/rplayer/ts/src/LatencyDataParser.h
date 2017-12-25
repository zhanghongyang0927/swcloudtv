///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/IDataSink.h>
#include <rplayer/ts/IEventSink.h>

#include <inttypes.h>

namespace rplayer {

class LatencyDataParser: public IDataSink
{
public:
    LatencyDataParser();
    ~LatencyDataParser();

    void setEventOut(IEventSink *eventOut);

    // Implementation of IDataSink
    void newStream(StreamType streamType, const char *language);
    void pesHeader(TimeStamp pts, TimeStamp dts, uint32_t pesPayloadLength);
    void parse(const uint8_t *data, uint32_t size);
    void reset();

private:
    IEventSink *m_eventOut;
    TimeStamp m_lastPts;
};

} // namespace rplayer
