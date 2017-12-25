///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "LatencyDataParser.h"

#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/Logger.h>

#include <string.h>

using namespace rplayer;

LatencyDataParser::LatencyDataParser() :
    m_eventOut(0)
{
}

LatencyDataParser::~LatencyDataParser()
{
}

void LatencyDataParser::setEventOut(IEventSink *eventOut)
{
    m_eventOut = eventOut;
}

void LatencyDataParser::newStream(StreamType /*streamType*/, const char */*language*/)
{
}

void LatencyDataParser::pesHeader(TimeStamp pts, TimeStamp /*dts*/, uint32_t /*pesPayloadLength*/)
{
    m_lastPts = pts;
}

void LatencyDataParser::parse(const uint8_t *data, uint32_t size)
{
    RPLAYER_LOG_DEBUG("Got data size:%d", size);

    if (!m_eventOut && size < 3) {
        return;
    }

    BitReader stream_data(data, size, 0);
    uint32_t n_entries = stream_data.read(8);

    size -= 1;

    uint32_t i = 0;
    while (size >= 2 && i < n_entries) {
        uint32_t event_type = stream_data.read(8);
        uint32_t event_data_length = stream_data.read(8);
        size -= 2;
        switch (event_type) {
        case 0x0: // KEYPRESS
            if (event_data_length == 8 && size >= 8) {
                uint64_t event_data = stream_data.read(32);
                event_data <<= 32;
                event_data |= stream_data.read(32);
                m_eventOut->privateStreamData(IEventSink::KEY_PRESS, m_lastPts, event_data);
                size -= 8;
            } else {
                // The data is most likely corrupted, it should be 8 bytes long
                RPLAYER_LOG_WARNING("KEYPRESS parse failed event_data_length %d != 8 and/or size:%d < 8", event_data_length, size);
            }
            break;
        case 0x1: // FIRSTPAINT
            m_eventOut->privateStreamData(IEventSink::FIRST_PAINT, m_lastPts, 0);
            break;
        case 0x2: // APP_COMPLETE
            m_eventOut->privateStreamData(IEventSink::APP_COMPLETE, m_lastPts, 0);
            break;
        default:
            RPLAYER_LOG_WARNING("Unsupported event_type:%d", event_type);
        }
        i++;
    }
}

void LatencyDataParser::reset()
{
}
