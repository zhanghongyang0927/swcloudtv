///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "StreamBuffer.h"
#include "Frame.h"

#include <rplayer/utils/Logger.h>

#include <assert.h>

using namespace rplayer;

StreamBuffer::StreamBuffer() :
    m_streamType(STREAM_TYPE_UNKNOWN),
    m_currentFrame(0),
    m_expectedPayloadLength(0),
    m_ptsCorrectionDelta(TimeStamp::zero())
{
}

StreamBuffer::~StreamBuffer()
{
    clear();
}

void StreamBuffer::finishCurrentFrame()
{
    assert(m_currentFrame);
    m_completedFrames.push_back(m_currentFrame);
    m_currentFrame = 0;
    m_expectedPayloadLength = 0;
}

void StreamBuffer::clear()
{
    m_streamType = STREAM_TYPE_UNKNOWN;
    m_language = "";
    while (!m_completedFrames.empty()) {
        delete m_completedFrames.front();
        m_completedFrames.pop_front();
    }
    delete m_currentFrame;
    m_currentFrame = 0;
    m_expectedPayloadLength = 0;
    m_ptsCorrectionDelta = TimeStamp::zero();
}

void StreamBuffer::newStream(StreamType streamType, const char *language)
{
    m_streamType = streamType;
    m_language = language ? language : "";

    if (m_currentFrame) {
        // Finish any previous frame if needed.
        RPLAYER_LOG_INFO("Unexpectedly needed to close frame in stream switch");
        finishCurrentFrame();
    }
}

void StreamBuffer::pesHeader(TimeStamp pts, TimeStamp dts, uint32_t pesPayloadLength)
{
    if (m_currentFrame) {
        // Finish any previous frame, if not done already.
        RPLAYER_LOG_INFO("Unexpectedly needed to close frame of size %u (PES payload length is %u), this will add latency", static_cast<uint32_t>(m_currentFrame->m_data.size()), static_cast<uint32_t>(m_expectedPayloadLength));
        finishCurrentFrame();
    }

    if (pts.isValid()) {
        pts += m_ptsCorrectionDelta;
    }
    if (dts.isValid()) {
        dts += m_ptsCorrectionDelta;
    }

    m_currentFrame = new Frame(pts, dts);
    m_expectedPayloadLength = pesPayloadLength;
    m_currentFrame->m_data.reserve(pesPayloadLength);
}

void StreamBuffer::parse(const uint8_t *data, uint32_t size)
{
    if (m_currentFrame) {
        m_currentFrame->m_data.insert(m_currentFrame->m_data.end(), data, data + size);
        // Finish reception of a frame if the PES packet length is reached
        if (m_expectedPayloadLength > 0 && m_currentFrame->m_data.size() >= m_expectedPayloadLength) {
            if (m_currentFrame->m_data.size() != m_expectedPayloadLength) {
                RPLAYER_LOG_ERROR("Frame size/PES payload length mismatch: %u vs %u", static_cast<uint32_t>(m_currentFrame->m_data.size()), static_cast<uint32_t>(m_expectedPayloadLength));
            }
            finishCurrentFrame();
        }
    } else {
        RPLAYER_LOG_WARNING("Unexpected stray data after having closed a frame");
    }
}

void StreamBuffer::reset()
{
    // Should flush the current data and start a new stream.
    // For now, just discard all and restart.
    clear();
}

Frame *StreamBuffer::getFrameIfAvailable()
{
    if (m_completedFrames.empty()) {
        return 0;
    }

    Frame *frame = m_completedFrames.front();
    m_completedFrames.pop_front();
    return frame;
}

StreamType StreamBuffer::getStreamType()
{
    return m_streamType;
}

const std::string StreamBuffer::getLanguage()
{
    return m_language;
}

void StreamBuffer::addPtsCorrectionDelta(TimeStamp ptsCorrectionDelta)
{
    // Accumulate new corrections since they are based on relative time jumps of ingress streams.
    m_ptsCorrectionDelta += ptsCorrectionDelta;
}
