///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RPlayerImpl.h"

#include <rplayer/utils/Logger.h>

#include <stdlib.h>

using namespace rplayer;

static const RPlayer::Feature INITIALLY_ENABLED_FEATURES = RPlayer::FEATURE_NONE;

RPlayer::RPlayer() :
    m_impl(*new RPlayer::Impl())
{
}

RPlayer::~RPlayer()
{
    delete &m_impl;
}

void RPlayer::reinitialize()
{
    // Reset all enabled features
    setEnabledFeatures(INITIALLY_ENABLED_FEATURES);

    // Reset all dynamic state just in case the configuration didn't change
    reset();

    // Only the underrun mitigator has parameters that can be reinitialized.
    m_impl.m_underrunMitigator.reinitialize();
}

void RPlayer::reset()
{
    // Reset all components in order
    m_impl.m_rams.reset();
    m_impl.m_demux.reset();
    m_impl.m_underrunMitigator.reset();
}

void RPlayer::setParameter(const std::string &parameter, const std::string &value)
{
    RPLAYER_LOG_INFO("setParameter('%s':'%s')", parameter.c_str(), value.c_str());

    int v = atoi(value.c_str());
    if (parameter == "enabled_features") {
        Feature f = FEATURE_NONE;
        if (value.find("RAMS") != std::string::npos || value.find("rams") != std::string::npos) {
            f = f | FEATURE_RAMS_DECODER;
        }
        if (value.find("CENC") != std::string::npos || value.find("cenc") != std::string::npos) {
            f = f | FEATURE_CENC_DECRYPTION;
        }
        if (value.find("UNDERRUN") != std::string::npos || value.find("underrun") != std::string::npos) {
            f = f | FEATURE_UNDERRUN_MITIGATION;
        }
        setEnabledFeatures(f);
    } else if (parameter == "audio_clock_granularity_and_jitter") {
        m_impl.m_underrunMitigator.setClockGanularityAndJitter(UnderrunMitigator::AUDIO, TimeStamp::milliseconds(v));
    } else if (parameter == "video_clock_granularity_and_jitter") {
        m_impl.m_underrunMitigator.setClockGanularityAndJitter(UnderrunMitigator::VIDEO, TimeStamp::milliseconds(v));
    } else if (parameter == "min_audio_frame_distance") {
        m_impl.m_underrunMitigator.setMinFrameDistance(UnderrunMitigator::AUDIO, TimeStamp::milliseconds(v));
    } else if (parameter == "min_video_frame_distance") {
        m_impl.m_underrunMitigator.setMinFrameDistance(UnderrunMitigator::VIDEO, TimeStamp::milliseconds(v));
    } else if (parameter == "min_audio_delay") {
        m_impl.m_underrunMitigator.setMinDelay(UnderrunMitigator::AUDIO, TimeStamp::milliseconds(v));
    } else if (parameter == "min_video_delay") {
        m_impl.m_underrunMitigator.setMinDelay(UnderrunMitigator::VIDEO, TimeStamp::milliseconds(v));
    } else if (parameter == "default_audio_filler_frame_duration") {
        m_impl.m_underrunMitigator.setDefaultFillerFrameDuration(UnderrunMitigator::AUDIO, TimeStamp::milliseconds(v));
    } else if (parameter == "default_video_filler_frame_duration") {
        m_impl.m_underrunMitigator.setDefaultFillerFrameDuration(UnderrunMitigator::VIDEO, TimeStamp::milliseconds(v));
    } else if (parameter == "audio_delay") {
        m_impl.m_underrunMitigator.setDelay(UnderrunMitigator::AUDIO, TimeStamp::milliseconds(v));
    } else if (parameter == "video_delay") {
        m_impl.m_underrunMitigator.setDelay(UnderrunMitigator::VIDEO, TimeStamp::milliseconds(v));
    } else if (parameter == "pcr_resync_threshold") {
        m_impl.m_underrunMitigator.setPcrResyncThreshold(TimeStamp::milliseconds(v));
    } else if (parameter == "audio_correction") {
        m_impl.m_underrunMitigator.setCorrectionMode(UnderrunMitigator::AUDIO, value == "adjust_pts" ? UnderrunMitigator::ADJUST_PTS : value == "insert_filler_frames" ? UnderrunMitigator::INSERT_FILLER_FRAMES : UnderrunMitigator::OFF);
    } else if (parameter == "video_correction") {
        m_impl.m_underrunMitigator.setCorrectionMode(UnderrunMitigator::VIDEO, value == "adjust_pts" ? UnderrunMitigator::ADJUST_PTS : value == "insert_filler_frames" ? UnderrunMitigator::INSERT_FILLER_FRAMES : UnderrunMitigator::OFF);
    } else if (parameter == "audio_repeated_frame_count") {
        m_impl.m_underrunMitigator.setAudioRepeatedFrameCount(v);
    }
}

void RPlayer::getStatus(uint64_t &currentStreamTimeIn90kHzTicks, uint32_t &stalledDurationInMs, uint32_t &pcrDelayIn90kHzTicks)
{
    // TODO: If CENC decryption is enabled and underrun mitigation is not, we could query currentStreamTimeIn90kHzTicks from m_demux...
    // However, CENC is unused right now, so this requirement is not exactly urgent.
    if (m_impl.m_enabledFeatures & FEATURE_UNDERRUN_MITIGATION) {
        TimeStamp t = m_impl.m_underrunMitigator.getCurrentStreamTime();
        if (t.isValid()) {
            currentStreamTimeIn90kHzTicks = t.getAs90kHzTicks();
        }
        t = m_impl.m_underrunMitigator.getStalledDuration();
        if (t.isValid()) {
            stalledDurationInMs = t.getAsMilliseconds();
        }
        t = m_impl.m_underrunMitigator.getPcrDelay();
        if (t.isValid()) {
            pcrDelayIn90kHzTicks = t.getAs90kHzTicks();
        }
    }
}

void RPlayer::registerCallback(ICallback *callback)
{
    m_impl.m_underrunMitigator.registerCallback(callback);
}

void RPlayer::registerDecryptEngineFactory(IDecryptEngineFactory &decryptEngineFactory)
{
    m_impl.m_demux.registerDecryptEngineFactory(decryptEngineFactory);
}

void RPlayer::unregisterDecryptEngineFactory(IDecryptEngineFactory &decryptEngineFactory)
{
    m_impl.m_demux.unregisterDecryptEngineFactory(decryptEngineFactory);
}

void RPlayer::registerStreamDecryptEngine(IStreamDecrypt *streamDecryptEngine)
{
    m_impl.m_rams.registerStreamDecryptEngine(streamDecryptEngine);
}

void RPlayer::registerRamsChunkAllocator(IRamsChunkAllocator *ramsChunkAllocator)
{
    m_impl.m_rams.registerRamsChunkAllocator(ramsChunkAllocator);
}

void RPlayer::setEnabledFeatures(Feature enabledFeatures)
{
    if (m_impl.m_enabledFeatures != enabledFeatures) {
        m_impl.m_enabledFeatures = enabledFeatures;

        m_impl.adjustRouting();
    }
}

void RPlayer::setTsPacketOutput(IPacketSinkWithMetaData *packetOut)
{
    if (m_impl.m_packetOut != packetOut) {
        m_impl.m_packetOut = packetOut;

        m_impl.adjustRouting();
    }
}

void RPlayer::registerOutputEventSink(IEventSink *eventOut)
{
    if (m_impl.m_eventOut != eventOut) {
        m_impl.m_eventOut = eventOut;

        m_impl.adjustRouting();
    }
}

void RPlayer::parse(const uint8_t *data, uint32_t size)
{
    if (m_impl.m_packetIn) {
        m_impl.m_packetIn->put(data, size);
    }
}

void RPlayer::setCurrentTime(uint16_t timeInMs)
{
    // Set current time front-to-back
    // The RAMS decoder may cause a burst of data because an output
    // command may have become valid. This will be processed by the
    // underrun mitigation unit, which will see the time increment
    // only later. It must be able to handle that (data arriving
    // 'early'). The other case would be harder: first updating the
    // underrun mitigation unit could make it think there's a data
    // underrun, which actually might not be the case if the RAMS
    // decoder is about to generate some data.
    if (m_impl.m_enabledFeatures & FEATURE_RAMS_DECODER) {
        m_impl.m_rams.setCurrentTime(timeInMs);
    }
    if (m_impl.m_enabledFeatures & FEATURE_UNDERRUN_MITIGATION) {
        m_impl.m_underrunMitigator.setCurrentTime(timeInMs);
    }
}

RPlayer::Impl::Impl() :
    m_packetIn(0),
    m_packetOut(0),
    m_eventOut(0),
    m_enabledFeatures(INITIALLY_ENABLED_FEATURES)
{
}

RPlayer::Impl::~Impl()
{
}

void RPlayer::Impl::adjustRouting()
{
    // Make sure all modules start fresh after a configuration change.
    // Reset all components in order
    m_rams.reset();
    m_demux.reset();
    m_underrunMitigator.reset();

    // Reset their outputs (not really needed but it's good to clean up)
    m_rams.setTsPacketOutput(0);
    m_demux.setTsPacketOutput(0);
    m_demux.setEventOutput(0);
    m_underrunMitigator.setTsPacketOutput(0);
    m_underrunMitigator.setEventOutput(0);

    // Adapt internal routing
    // The order is input -> RAMS decoder -> CENC decryptor -> Underrun mitigator -> output, if enabled.
    // Work back from output to input...
    IPacketSinkWithMetaData *lastOutput = m_packetOut;
    if (m_enabledFeatures & FEATURE_UNDERRUN_MITIGATION) {
        // Put the underrun mitigation component in the pipeline.
        m_underrunMitigator.setTsPacketOutput(lastOutput);
        lastOutput = &m_underrunMitigator;
        // Attach the event output to the underrun mitigation component.
        m_underrunMitigator.setEventOutput(m_eventOut);
    }
    if (m_enabledFeatures & FEATURE_CENC_DECRYPTION) {
        // Put the CENC decryptor in the pipeline.
        m_demux.setTsPacketOutput(lastOutput);
        lastOutput = &m_demux;
        // Attach the event output to the CENC decryptor if underrun mitigation is not enabled.
        if (!(m_enabledFeatures & FEATURE_UNDERRUN_MITIGATION)) {
            m_demux.setEventOutput(m_eventOut);
        }
    }
    if (m_enabledFeatures & FEATURE_RAMS_DECODER) {
        // Put the RAMS decoder in the pipeline.
        m_rams.setTsPacketOutput(lastOutput);
        m_packetIn = &m_rams;
    } else {
        m_packetIn = lastOutput;
    }
}
