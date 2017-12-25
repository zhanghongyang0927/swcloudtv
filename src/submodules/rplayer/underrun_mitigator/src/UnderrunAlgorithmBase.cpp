///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "UnderrunAlgorithmBase.h"
#include "StreamBuffer.h"
#include "Frame.h"

#include <rplayer/utils/Logger.h>

#include <assert.h>
#include <string.h>

using namespace rplayer;

UnderrunAlgorithmBase::UnderrunAlgorithmBase(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback) :
    m_source(source),
    m_params(params),
    m_callback(callback),
    m_currentFrame(0),
    m_nRead(0),
    m_previousDelay(TimeStamp::zero()),
    m_accumulatedStalledDuration(TimeStamp::zero())
{
}

UnderrunAlgorithmBase::~UnderrunAlgorithmBase()
{
    clear();
}

StreamType UnderrunAlgorithmBase::getStreamType()
{
    return m_source.getStreamType();
}

const std::string UnderrunAlgorithmBase::getLanguage()
{
    return m_source.getLanguage();
}

bool UnderrunAlgorithmBase::isNewFrame(TimeStamp &pts, TimeStamp &dts)
{
    // If this returns true, a new PES header is inserted; PTS and optionally DTS are expected to be set then.

    if (m_currentFrame) {
        if (m_nRead == 0) {
            pts = m_currentFrame->m_pts;
            dts = m_currentFrame->m_dts;
            return true;
        }
    }

    return false;
}

const uint8_t *UnderrunAlgorithmBase::getData()
{
    if (m_currentFrame) {
        return &m_currentFrame->m_data[m_nRead];
    }

    return 0;
}

uint32_t UnderrunAlgorithmBase::getBytesAvailable(TimeStamp pcr)
{
    // This is the method that is called first to check it any data is available.
    // Only after having called this, the TsMux calls methods like isNewFrame(), getData() and readBytes().
    // Therefore, this is the moment to check whether any current data is still present or if not, if any
    // new data is available or if not, if any data needs to be created.
    // If getBytesAvailable() returns non-zero, data *will* be multiplexed this iteration.
    if (!m_currentFrame) {
        m_currentFrame = getNextFrame(pcr);
    }

    if (m_currentFrame) {
        return m_currentFrame->m_data.size() - m_nRead;
    }

    return 0;
}

void UnderrunAlgorithmBase::readBytes(uint32_t n)
{
    if (m_currentFrame) {
        m_nRead += n;
        if (m_nRead >= m_currentFrame->m_data.size()) {
            delete m_currentFrame;
            m_currentFrame = 0;
            m_nRead = 0;
        }
    }
}

void UnderrunAlgorithmBase::clear()
{
    m_source.clear();
    delete m_currentFrame;
    m_currentFrame = 0;
    m_nRead = 0;
    m_previousDelay = TimeStamp::zero();
    m_accumulatedStalledDuration = TimeStamp::zero();
}

Frame *UnderrunAlgorithmBase::checkSource()
{
    return m_source.getFrameIfAvailable();
}

const UnderrunAlgorithmParams &UnderrunAlgorithmBase::getParams() const
{
    return m_params;
}

void UnderrunAlgorithmBase::notifyDelay(const TimeStamp &delay)
{
    if (delay <= TimeStamp::zero()) {
        // We ignore cases when there is no delay.
        return;
    }

    // The stall is computed by looking at the current delay vs. the previous delay.
    // If the delay increased, there was a stall. If it decreased, there is latency
    // mitigation going on (speed-up of playback) and no stall is reported.
    TimeStamp stallTime = delay - m_previousDelay;
    m_previousDelay = delay;

    if (stallTime <= TimeStamp::zero()) {
        // Don't report when there is no stall.
        return;
    }

    // Accumulate the measured maximum delay into the total stall duration.
    m_accumulatedStalledDuration += stallTime;

    m_callback.stallDetected(stallTime);
}

TimeStamp UnderrunAlgorithmBase::getStalledDuration()
{
    return m_accumulatedStalledDuration;
}
