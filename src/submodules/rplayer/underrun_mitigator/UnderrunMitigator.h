///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/RPlayer.h>
#include <rplayer/IPacketSink.h>
#include <rplayer/ts/TimeStamp.h>

#include <inttypes.h>

namespace rplayer {

struct IEventSink;

class UnderrunMitigator : public IPacketSinkWithMetaData
{
public:
    UnderrunMitigator();
    ~UnderrunMitigator();

    // Callback for stall duration messages
    void registerCallback(RPlayer::ICallback *callback);

    // Settings
    enum StreamType { AUDIO, VIDEO };
    enum CorrectionMode { OFF, ADJUST_PTS, INSERT_FILLER_FRAMES };
    void setCorrectionMode(StreamType stream, CorrectionMode mode);
    void setClockGanularityAndJitter(StreamType stream, TimeStamp t);
    void setMinFrameDistance(StreamType stream, TimeStamp t);
    void setMinDelay(StreamType stream, TimeStamp t);
    void setDefaultFillerFrameDuration(StreamType stream, TimeStamp t);
    void setDelay(StreamType stream, TimeStamp t);
    void setPcrResyncThreshold(TimeStamp t);
    void setAudioRepeatedFrameCount(unsigned n);

    // Status query
    TimeStamp getCurrentStreamTime(); // Current ingress PCR value. (Egress PCR may have a different time base after resyncs.)
    TimeStamp getStalledDuration(); // Total accumulated stalled duration.
    TimeStamp getPcrDelay(); // Egress PCR correction (only non-zero if filler frames were inserted and need correction by the compositor).

    // Bring the underrun mitigator back into its state similar to after construction
    // This will reset all parameters and dynamic state but it won't
    // unregister any registered TS or event output or the like.
    void reinitialize();

    // Call this to start processing a new stream. Required for proper operation.
    void reset();

    // Call this to process TS, could be one or more packets.
    void put(const uint8_t *data, uint32_t size);

    void setMetaData(const StreamMetaData &metaData);

    // Registration of a TS output that will receive the mitigated or forwarded transport stream.
    void setTsPacketOutput(IPacketSink *packetOut);

    // Registration of an event output that will receive latency events when applicable (referring to packets that actually leave the UnderrunMitigator).
    void setEventOutput(IEventSink *eventOut);

    // Set current real time in ms. The time may (and will) wrap around. This is no problem.
    // It should be continuous, however, meaning that any difference in the real time should
    // equal the difference in the time passed.
    // The origin of the absolute value does not matter.
    // If used, this method must be called immediately prior to each series of calls to put()
    // for time management to properly operate.
    // A real-time thread may/can/will additionally call this on regular basis.
    void setCurrentTime(uint16_t currentRealTimeClockInMs);

private:
    UnderrunMitigator(const UnderrunMitigator &);
    UnderrunMitigator &operator=(const UnderrunMitigator &);

    class Impl;
    Impl &m_impl;
};

} // namespace
