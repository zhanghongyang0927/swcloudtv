///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "Frame.h"

#include <rplayer/ts/IDataSink.h>

#include <list>
#include <string>

namespace rplayer {

class StreamBuffer: public IDataSink
{
public:
    StreamBuffer();
    ~StreamBuffer();

    // Use clear() when a new stream is started
    // This might be necessary for proper operation when opening new streams /or/ could be handled by newStream()
    void clear();

    // IDataSink implementation
    void newStream(StreamType streamType, const char *language);
    void pesHeader(TimeStamp pts, TimeStamp dts, uint32_t pesPayloadLength);
    void parse(const uint8_t *data, uint32_t size);
    void reset();

    // Check if a full frame is available and return it if so; returns 0 otherwise.
    Frame *getFrameIfAvailable();

    // Get cached data from newStream()
    StreamType getStreamType();
    const std::string getLanguage();

    // Add a PTS/DTS correction delta to any accumulated corrections already passed.
    // This is so new frames entering the StreamBuffer that refer to a new time base
    // can be adapted to 'our' current, existing time base.
    void addPtsCorrectionDelta(TimeStamp ptsCorrectionDelta);

private:
    StreamBuffer(const StreamBuffer &);
    StreamBuffer &operator=(const StreamBuffer &);

    void finishCurrentFrame();

    StreamType m_streamType;
    std::string m_language;
    std::list<Frame*> m_completedFrames;
    Frame *m_currentFrame;
    uint32_t m_expectedPayloadLength;
    TimeStamp m_ptsCorrectionDelta;
};

} // namespace
