///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "StreamBuffer.h"
#include "UnderrunAlgorithmParams.h"
#include "Passthrough.h"
#include "PtsFiddler.h"
#include "AudioFillerFrameInserter.h"
#include "VideoFillerFrameInserter.h"

#include <rplayer/StreamMetaData.h>
#include <rplayer/underrun_mitigator/UnderrunMitigator.h>
#include <rplayer/ts/IEventSink.h>
#include <rplayer/ts/TsDemux.h>
#include <rplayer/ts/TsMux.h>
#include <rplayer/ts/IDataSource.h>
#include <rplayer/ts/IDataSink.h>
#include <rplayer/utils/Logger.h>

#include <algorithm>

#include <assert.h>

using namespace rplayer;

// The clock management code is largely a copy of the RamsClock class in the RAMS section.
// For more explanation about the synchronization principles, please refer to that code.
// The reason that we don't reuse that code is that the time stamps that are handled are
// different, being ms in the RAMS domain and 90kHz ticks in the TS domain. Also, clock
// management is an essential part of both RAMS and the underrun mitigator, so it's good
// to have the code stay with the component it serves.
// So for one time we violate the rule to not have any duplication in the code base.

static const uint16_t CLOCK_SLOWDOWN_FRACTION = 512; // Power-of-2 speeds-up division and modulo operators but is not essential.

class UnderrunMitigator::Impl: public IEventSink
{
public:
    Impl();
    ~Impl();

    void reinitialize();
    void reset();
    void setCurrentTime(uint16_t timeInMs);
    void put(const uint8_t *data, uint32_t size);
    void setMetaData(const StreamMetaData &metaData);

    void setCorrectionMode(StreamType stream, CorrectionMode mode);

    class Callback: public UnderrunAlgorithmBase::ICallback
    {
    public:
        Callback(UnderrunMitigator::Impl &impl, bool isAudioNotVideo) :
            m_impl(impl),
            m_isAudioNotVideo(isAudioNotVideo)
        {
        }

        void stallDetected(const TimeStamp &stallDuration)
        {
            m_impl.stallDetected(m_isAudioNotVideo, stallDuration);
        }

    private:
        Impl &m_impl;
        bool m_isAudioNotVideo;
    };

    RPlayer::ICallback *m_callback;
    IEventSink *m_eventOut;
    TsDemux m_demux;
    TsMux m_mux;
    UnderrunAlgorithmParams m_videoParams;
    UnderrunAlgorithmParams m_audioParams;
    StreamBuffer m_videoBuffer;
    StreamBuffer m_audioBuffer;
    Callback m_videoCallback;
    Callback m_audioCallback;
    UnderrunAlgorithmBase *m_videoUnderrunAlgorithm;
    UnderrunAlgorithmBase *m_audioUnderrunAlgorithm;
    StreamMetaData m_metaData;

    // Clock management
    bool m_isTimeSet; // Marks whether setCurrentTime() has been called at least once.
    uint16_t m_lastTime; // Keeps track of the last real time as passed to setCurrentTime().
    uint16_t m_clockSlowdownRemainder; // Keeps track of clock slowdown cycles not yet taken into account.
    TimeStamp m_currentMitigatorClock; // Keeps track of the current program clock (PCR) as passed by pcrReceived() or updated by setCurrentTime().
    TimeStamp m_timeOfLastSentOutput; // Keeps track of the time the last output was sent.
    uint32_t m_pcrResyncThreshold; // Threshold to determine unconditional resynchronization of the PCR
    int64_t m_ingressPcrOffset; // Offset that is added to ingress PCR and PTS values in order to correct for PCR jumps.

    TimeStamp m_ingressStreamTime;

    void generateOutput();

    void stallDetected(bool isAudioNotVideo, const TimeStamp &stallDuration);

    // Implements IEventSink (from m_demux)
    void pcrReceived(uint64_t pcr90kHz, int pcrExt27MHz, bool hasDiscontinuity);
    void tableVersionUpdate(int tableId, int version);
    void privateStreamData(PrivateDataType data_type, TimeStamp pts, uint64_t data);
};

UnderrunMitigator::UnderrunMitigator() :
    m_impl(*new Impl())
{
}

UnderrunMitigator::~UnderrunMitigator()
{
    delete &m_impl;
}

void UnderrunMitigator::registerCallback(RPlayer::ICallback *callback)
{
    m_impl.m_callback = callback;
}

void UnderrunMitigator::setCorrectionMode(StreamType stream, CorrectionMode mode)
{
    m_impl.setCorrectionMode(stream, mode);
}

void UnderrunMitigator::setClockGanularityAndJitter(StreamType stream, TimeStamp t)
{
    switch (stream) {
    case AUDIO:
        m_impl.m_audioParams.m_clockGranularityAndJitter = t;
        break;
    case VIDEO:
        m_impl.m_videoParams.m_clockGranularityAndJitter = t;
        break;
    }
}

void UnderrunMitigator::setMinFrameDistance(StreamType stream, TimeStamp t)
{
    switch (stream) {
    case AUDIO:
        m_impl.m_audioParams.m_minFrameDistance = t;
        break;
    case VIDEO:
        m_impl.m_videoParams.m_minFrameDistance = t;
        break;
    }
}

void UnderrunMitigator::setMinDelay(StreamType stream, TimeStamp t)
{
    switch (stream) {
    case AUDIO:
        m_impl.m_audioParams.m_minDelay = t;
        break;
    case VIDEO:
        m_impl.m_videoParams.m_minDelay = t;
        break;
    }
}

void UnderrunMitigator::setDefaultFillerFrameDuration(StreamType stream, TimeStamp t)
{
    switch (stream) {
    case AUDIO:
        m_impl.m_audioParams.m_defaultFillerFrameDuration = t;
        break;
    case VIDEO:
        m_impl.m_videoParams.m_defaultFillerFrameDuration = t;
        break;
    }
}

void UnderrunMitigator::setDelay(StreamType stream, TimeStamp t)
{
    switch (stream) {
    case AUDIO:
        m_impl.m_audioParams.m_delay = t;
        break;
    case VIDEO:
        m_impl.m_videoParams.m_delay = t;
        break;
    }
}

void UnderrunMitigator::setPcrResyncThreshold(TimeStamp t)
{
    m_impl.m_pcrResyncThreshold = t.getAs90kHzTicks();
}

void UnderrunMitigator::setAudioRepeatedFrameCount(unsigned n)
{
    m_impl.m_audioParams.m_repeatedFrameCount = n;
}

TimeStamp UnderrunMitigator::getCurrentStreamTime()
{
    return m_impl.m_ingressStreamTime;
}

TimeStamp UnderrunMitigator::getStalledDuration()
{
    const TimeStamp a(m_impl.m_audioUnderrunAlgorithm->getStalledDuration());
    const TimeStamp b(m_impl.m_videoUnderrunAlgorithm->getStalledDuration());
    return a > b ? a : b;
}

TimeStamp UnderrunMitigator::getPcrDelay()
{
    // Recovery of inserted filler frames, if any, is done by the UnderrunMitigator itself, so we don't need any correction by compositor.
    return TimeStamp();
}

void UnderrunMitigator::reinitialize()
{
    m_impl.reinitialize();
}

void UnderrunMitigator::reset()
{
    m_impl.reset();
}

void UnderrunMitigator::setTsPacketOutput(IPacketSink *packetOut)
{
    m_impl.m_mux.setOutput(packetOut);
}

void UnderrunMitigator::setEventOutput(IEventSink *eventOut)
{
    m_impl.m_eventOut = eventOut;
}

void UnderrunMitigator::setCurrentTime(uint16_t timeInMs)
{
    m_impl.setCurrentTime(timeInMs);
}

void UnderrunMitigator::put(const uint8_t *data, uint32_t size)
{
    m_impl.put(data, size);
}

void UnderrunMitigator::setMetaData(const StreamMetaData &metaData)
{
    m_impl.setMetaData(metaData);
}

UnderrunMitigator::Impl::Impl() :
    m_callback(0),
    m_eventOut(0),
    m_videoCallback(*this, false),
    m_audioCallback(*this, true),
    m_videoUnderrunAlgorithm(0),
    m_audioUnderrunAlgorithm(0),
    m_isTimeSet(false),
    m_lastTime(0),
    m_clockSlowdownRemainder(0),
    m_pcrResyncThreshold(0),
    m_ingressPcrOffset(0)
{
    m_demux.setEventOutput(this);
    m_demux.setVideoOutput(&m_videoBuffer);
    m_demux.setAudioOutput(&m_audioBuffer);

    reinitialize();

    // With real stuttering connectivity, the stream gets out well if the 'delay'
    // parameter is increased to a few seconds. This means that it is theoretically
    // possible to have an automatically adjusted delay when there are underruns
    // in order to 'correct' the user experience. However, latency will be badly
    // affected.
    // Do we need to adjust both audio and video delays simultaneously? (They will
    // be in sync if normally the stream is correct.)
}

UnderrunMitigator::Impl::~Impl()
{
    reset();

    m_demux.setEventOutput(0);
    m_demux.setVideoOutput(0);
    m_demux.setAudioOutput(0);
    m_mux.setVideoInput(0);
    m_mux.setAudioInput(0);

    delete m_videoUnderrunAlgorithm;
    delete m_audioUnderrunAlgorithm;
}

void UnderrunMitigator::Impl::reinitialize()
{
    // First, reset the correction mode
    setCorrectionMode(AUDIO, INSERT_FILLER_FRAMES);
    setCorrectionMode(VIDEO, INSERT_FILLER_FRAMES);

    // Then reset all dynamic parameters
    reset();

    // Reset all parameters to their default state
    m_videoParams.m_clockGranularityAndJitter = TimeStamp::milliseconds(12);
    m_videoParams.m_minFrameDistance = TimeStamp::milliseconds(15);
    m_videoParams.m_minDelay = TimeStamp::milliseconds(0);
    m_videoParams.m_defaultFillerFrameDuration = TimeStamp::milliseconds(45);
    m_videoParams.m_delay = TimeStamp::milliseconds(5);
    m_audioParams.m_clockGranularityAndJitter = TimeStamp::milliseconds(12);
    m_audioParams.m_minFrameDistance = TimeStamp::milliseconds(5);
    m_audioParams.m_minDelay = TimeStamp::milliseconds(0);
    m_audioParams.m_defaultFillerFrameDuration = TimeStamp::milliseconds(1000); // Unused
    m_audioParams.m_delay = TimeStamp::milliseconds(15);
    m_audioParams.m_repeatedFrameCount = 1;
}

void UnderrunMitigator::Impl::reset()
{
    m_demux.reset();
    assert(m_videoUnderrunAlgorithm);
    m_videoUnderrunAlgorithm->clear();
    assert(m_audioUnderrunAlgorithm);
    m_audioUnderrunAlgorithm->clear();
    m_mux.reset();

    // Reset the clock management
    m_isTimeSet = false;
    m_lastTime = 0;
    m_clockSlowdownRemainder = 0;
    m_currentMitigatorClock.invalidate();
    m_timeOfLastSentOutput.invalidate();
    m_ingressPcrOffset = 0;

    // Reset status info
    m_ingressStreamTime.invalidate();
}

void UnderrunMitigator::Impl::setCorrectionMode(StreamType stream, CorrectionMode mode)
{
    switch (stream) {
    case AUDIO:
        delete m_audioUnderrunAlgorithm;
        m_audioUnderrunAlgorithm = 0;

        switch (mode) {
        case OFF:
            m_audioUnderrunAlgorithm = new Passthrough(m_audioBuffer, m_audioParams, m_audioCallback);
            break;

        case ADJUST_PTS:
            m_audioUnderrunAlgorithm = new PtsFiddler(m_audioBuffer, m_audioParams, m_audioCallback);
            break;

        case INSERT_FILLER_FRAMES:
            m_audioUnderrunAlgorithm = new AudioFillerFrameInserter(m_audioBuffer, m_audioParams, m_audioCallback);
            break;
        }

        assert(m_audioUnderrunAlgorithm);
        m_mux.setAudioInput(m_audioUnderrunAlgorithm);
        break;

    case VIDEO:
        delete m_videoUnderrunAlgorithm;
        m_videoUnderrunAlgorithm = 0;

        switch (mode) {
        case OFF:
            m_videoUnderrunAlgorithm = new Passthrough(m_videoBuffer, m_videoParams, m_videoCallback);
            break;

        case ADJUST_PTS:
            m_videoUnderrunAlgorithm = new PtsFiddler(m_videoBuffer, m_videoParams, m_videoCallback);
            break;

        case INSERT_FILLER_FRAMES:
            m_videoUnderrunAlgorithm = new VideoFillerFrameInserter(m_videoBuffer, m_videoParams, m_videoCallback);
            break;
        }

        assert(m_videoUnderrunAlgorithm);
        m_mux.setVideoInput(m_videoUnderrunAlgorithm);
        break;
    }
}

void UnderrunMitigator::Impl::pcrReceived(uint64_t pcr90kHz, int /*pcrExt27MHz*/, bool hasDiscontinuity)
{
    // Synchronize clock
    // A new TS packet has arrived with a new program clock reference that we need to take over.
    // The clock value is in 90kHz units from the TS time base. The origin may differ from the real time.
    bool synchronize = true;
    if (m_isTimeSet && m_currentMitigatorClock.isValid()) {
        // We'll process lead or lag here (assuming these are not more than half the clock range).
        // Positive values indicate a lead (transport stream time is leading the real time).
        // Negative values indicate a lag (transport stream time is lagging the real time).
        // Lagging times we don't take unless they exceed the m_pcrResyncThreshold.
        // We also handle stream PCR discontinuities here, which are very similar to unsignaled stream time jumps.
        int64_t lead = static_cast<int64_t>(pcr90kHz + m_ingressPcrOffset - m_currentMitigatorClock.getAs90kHzTicks());
        if (lead < 0) {
            uint64_t lag = -lead;
            if (m_pcrResyncThreshold != 0 && lag >= m_pcrResyncThreshold) {
                RPLAYER_LOG_INFO("Resyncing large PCR delta: %u", (uint32_t)lag);
                hasDiscontinuity = true; // A discovered time jump should be treated the same way as a properly signaled one.
            }
            synchronize = false;
        }

        if (hasDiscontinuity) {
            m_ingressPcrOffset -= lead; // Equivalent to m_ingressPcrOffset = m_currentMitigatorClock.getAs90kHzTicks() - pcr90kHz (-> m_currentMitigatorClock == pcr90kHz + m_ingressPcrOffset)

            // So now all further ingress time stamps have an offset of 'new PCR' - 'old PCR'. This means pcr90kHz increased by
            // 'lead'. m_ingressPcrOffset is adusted in such a way that we don't see this jump in the output clock. If we want to
            // adjust the PTS and DTS values of the stream the same way, we have to subtract 'lead' from all ingress PTS/DTS values
            // from now on. Therefore we pass the PTS/DTS correction delta to the underrun mitigator stream buffers.

            // Use cases: restarting streams (server simulator), deep buffer stream switching and perhaps for supporting discontinuous or largely lagging streams in the future.
            RPLAYER_LOG_INFO("Resyncing PCR discontinuity: %d", (int32_t)lead);
            TimeStamp t;
            t.setAs90kHzTicks(-lead);
            m_audioBuffer.addPtsCorrectionDelta(t);
            m_videoBuffer.addPtsCorrectionDelta(t);

            synchronize = false; // This actually doesn't matter since m_currentMitigatorClock will not change either way. :-)
        }
    }

    // Synchronize if required.
    if (synchronize) {
        m_currentMitigatorClock.setAs90kHzTicks(pcr90kHz + m_ingressPcrOffset);
    }

    // Update status info
    m_ingressStreamTime.setAs90kHzTicks(pcr90kHz);
}

void UnderrunMitigator::Impl::setCurrentTime(uint16_t currentRealTimeClockInMs)
{
    if (!m_isTimeSet) {
        // First time
        m_lastTime = currentRealTimeClockInMs;
        m_isTimeSet = true;
        return;
    }

    uint16_t delta = currentRealTimeClockInMs - m_lastTime;
    if (delta > 100) {
        RPLAYER_LOG_WARNING("Delta=%d, currentRealTimeClockInMs=%d, m_lastTime=%d", delta, currentRealTimeClockInMs, m_lastTime);
    }
    m_lastTime = currentRealTimeClockInMs;

    if (!m_currentMitigatorClock.isValid()) {
        // We can't generate data unless we have a proper clock.
        return;
    }

    // And correct for any slowdown we need to apply
    m_clockSlowdownRemainder += delta;
    delta -= m_clockSlowdownRemainder / CLOCK_SLOWDOWN_FRACTION;
    m_clockSlowdownRemainder %= CLOCK_SLOWDOWN_FRACTION;

    if (delta == 0) {
        // Nothing to do
        return;
    }

    // Now process the new program clock using the computed real-time delta.
    m_currentMitigatorClock += TimeStamp::milliseconds(delta);

    // Output all data that can be output.
    generateOutput();
}

void UnderrunMitigator::Impl::generateOutput()
{
    assert(m_currentMitigatorClock.isValid());
    assert(m_videoUnderrunAlgorithm);
    assert(m_audioUnderrunAlgorithm);

    // This piece of code makes sure that the mux always outputs data with logical
    // PCR steps that don't exceed 10ms. This way, all timing in the stream can be
    // logically correct even in case of big time jumps in m_currentMitigatorClock
    // (in turn typically caused by big (or many) time jumps in the ingress PCR).
    // The real-time behavior will be taken care of by regularly calling setCurrentTime().
    if (m_timeOfLastSentOutput.isValid()) {
        const int64_t PCR_STEP = 900; // 10ms in 90kHz ticks
        while (static_cast<int64_t>(m_currentMitigatorClock.getAs90kHzTicks() - m_timeOfLastSentOutput.getAs90kHzTicks()) > PCR_STEP) {
            m_timeOfLastSentOutput += TimeStamp::ticks(PCR_STEP);
            m_mux.muxPackets(m_timeOfLastSentOutput, TsMux::MUX_PCR, 1);
        }
    }

    // Send all (remaining) data using the latest (current) PCR but don't send the PCR itself yet.
    unsigned packetsSent = m_mux.muxPackets(m_currentMitigatorClock, TsMux::MUX_ALL & ~TsMux::MUX_PCR, ~0U);
    if (packetsSent) {
        // And if anything was sent, send the PCR last so all frames sent will be formally on time.
        m_timeOfLastSentOutput = m_currentMitigatorClock;
        m_mux.muxPackets(m_currentMitigatorClock, TsMux::MUX_FORCE_PCR, 1);
    }
}

void UnderrunMitigator::Impl::stallDetected(bool isAudioNotVideo, const TimeStamp &stallDuration)
{
    if (m_callback) {
        std::string id;
        if (m_metaData.getId() == StreamMetaData::NO_ID) {
            id += "TS"; // Regular TS
        } else {
            int tens = m_metaData.getId() / 10;
            int ones = m_metaData.getId() % 10;
            id += "RAMS";
            id += "0123456789"[tens];
            id += "0123456789"[ones];
        }

        m_callback->stallDetected(id, isAudioNotVideo, stallDuration);
    }
}

void UnderrunMitigator::Impl::put(const uint8_t *data, uint32_t size)
{
    // Accumulate any data
    m_demux.put(data, size);
}

void UnderrunMitigator::Impl::setMetaData(const StreamMetaData &metaData)
{
    // Store the metadata for next output actions.
    // Actually, because the demuxed data is buffered, the metadata might not yet
    // apply to the data in the next output action. To solve this, we would need to
    // attach the metadata to packets in the buffer and apply them whenever they are
    // read from the buffer. This is a lot of effort for probably little gain. But
    // *if* there is a disturbing metadata mismatch between input and output, we may
    // need to do so...
    m_metaData = metaData;
}

void UnderrunMitigator::Impl::tableVersionUpdate(int /*tableId*/, int /*version*/)
{
}

void UnderrunMitigator::Impl::privateStreamData(PrivateDataType /*data_type*/, TimeStamp /*pts*/, uint64_t /*data*/)
{
    // TODO: save data and defer forwarding to m_eventOut
    // We may forward immediately with some PTS correction, but we don't really know yet when the data will be displayed
    // since the outgoing PCR may differ from the incoming...
}
