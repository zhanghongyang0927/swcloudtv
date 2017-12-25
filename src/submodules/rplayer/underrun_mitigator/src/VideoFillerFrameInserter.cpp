///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "VideoFillerFrameInserter.h"
#include "H264VideoFillerFrameCreator.h"
#include "Mpeg2VideoFillerFrameCreator.h"
#include "UnderrunAlgorithmParams.h"
#include "Frame.h"

#include <rplayer/utils/Logger.h>

#include <assert.h>
#include <string.h>

using namespace rplayer;

VideoFillerFrameInserter::VideoFillerFrameInserter(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback) :
    UnderrunAlgorithmBase(source, params, callback),
    m_delay(TimeStamp::zero()),
    m_fillerFrameCreator(NULL)
{
}

VideoFillerFrameInserter::~VideoFillerFrameInserter()
{
    delete m_fillerFrameCreator;
}

void VideoFillerFrameInserter::processNewFrame(Frame *frame)
{
    if (!m_fillerFrameCreator || m_fillerFrameCreator->getStreamType() != getStreamType()) {
        delete m_fillerFrameCreator;
        m_fillerFrameCreator = 0;

        switch (getStreamType()) {
            case STREAM_TYPE_MPEG2_VIDEO:
                m_fillerFrameCreator = new Mpeg2VideoFillerFrameCreator();
                break;

            case STREAM_TYPE_H264_VIDEO:
                m_fillerFrameCreator = new H264VideoFillerFrameCreator();
                break;

            default:
                break;
        }
    }

    if (m_fillerFrameCreator) {
        m_fillerFrameCreator->processIncomingFrame(frame);
    }
}

Frame *VideoFillerFrameInserter::generateFillerFrame()
{
    if (m_fillerFrameCreator) {
        return m_fillerFrameCreator->create();
    }
    else {
        return NULL;
    }
}

//
// Suppose the ingress stream contains frames 1, 2, 3 and 4.
// They have frame durations of D1, D2, D3 and D4, respectively.
// First DTS is DTS1, so DTS2 == DTS1 + D1.
// In the following scenario, we assume that we need to insert 2 filler frames,
// with a duration of DFF1 and DFF2. The result should be a sequence having the
// following time stamps:
// frame 1: DTS1; m_delay == 0
// frame 2: DTS2 (== DTS1 + D1); m_delay == 0
// filler frame 1: DTS2 + D2 (== DTS3 but we don't have that one yet); m_delay == DFF1
// filler frame 2: DTS2 + D2 + DFF1 (== DTS3 + DFF1); m_delay == DFF1 + DFF2
// frame 3: DTS3 + DFF1 + DFF2 (== DTS2 + D2 + DFF1 + DFF2)
// frame 4: DTS4 + DFF1 + DFF2
//
// Audio and video behave differently, typically. Audio is a stream with a fixed sample rate and it is important that all
// audio frames are scheduled back-to-back. For video, things are different. Although typically video has a nominal frame
// rate, frames can be sped-up or slowed down almost at will. The days that video playback needed to be constant frame rate
// are gone and nowadays all decoders should be capable of displaying arbitrary and fluctuating frame rates. There are only
// limits to the lowest and highest frame rates that a decoder can handle. On the other hand, video frames cannot be removed,
// typically, because they build on top of one another. Therefore, with video we need to speed-up playback rather than remove
// frames if we want to reduce the built-up delay. But the insertion of filler video frames is less strict on timing so filler
// frames can be inserted at more or less arbitrary times. This means that the value of Dx is not important here. For small
// delays in video, we can also fall back to 'PTS fiddling', i.e. delay the DTS by a small amount so there won't be an underrun.
//
// *** For discontinuous (video) streams:
// The default filler frame insertion duration DFFD is used, but actually this will be made duration of the frame before that.
// First call to getNextFrame(), m_delay == 0, checkSource() returns frame 1.
// processNewFrame() extracts info to be able to create a matching filler frame, FF, without a valid duration.
// m_lastDts will be set to DTS1.
// The frame is displayed, its PTS and DTS remain unchanged, equal to DTS1.
//
// Second call to getNextFrame(), m_delay == 0, checkSource() returns frame 2.
// processNewFrame() may update its filler frame creation info.
// m_lastDts will be set to DTS2.
// The frame is displayed, its PTS and DTS remain unchanged, equal to DTS2.
//
// Third call to getNextFrame(), m_delay == 0, checkSource() returns 0.
// m_lastDts is valid and we assume for now that we need a filler frame.
// The filler frame is generated by generateFillerFrame() and has no valid duration.
// Its PTS is set to m_lastDts (DTS2) + DFFD. This means that effectively the previous frame is extended to DFFD.
// m_delay is set to DFFD.
//
// Fourth call to getNextFrame(), m_delay == DFFD, checkSource() returns 0.
// m_lastDts is valid and we assume for now that we need another filler frame.
// This is generated by generateFillerFrame() and has no valid duration.
// Its PTS is set to m_lastDts (DTS2) + 2 * DFFD so the previous filler frame also has a duration of DFFD.
// m_delay is increased to 2 * DFFD.
//
// Fifth call to getNextFrame(), m_delay == 2 * DFFD, checkSource() returns frame 3.
// processNewFrame() may update its filler frame creation info.
// m_lastDts will be set to DTS3. Let's assume we're not able to recover yet.
// The frame is displayed, its PTS is now set to DTS3 + 2 * DFFD. This is correct. The last filler frame now effectively has
// a duration that originally was the duration of frame 2.
//
// When to insert filler frames and when to recover?
// Typically, a frame needs to be fully present before the PCR (program clock reference) reaches its DTS (decode time stamp).
// The minDelay parameter allows for some specific headroom (and latency) to the decoder.
// So a correctly timed frame should have a eDTS >= PCR + minDelay. Consequentially, a frame with eDTS < PCR + minDelay is too late.
// In our code, the egress DTS (eDTS) is equal to the ingress DTS (iDTS) + m_delay + getParams().m_delay, hence the addition of m_delay
// and getParams().m_delay in the equations.
//
// We need to insert a filler frame if the eDTS of the next frame otherwise *could* be too late. This is, the next iteration
// (at PCR + 10ms) having an eDTS < PCR + 10ms + minDelay. Some jitter in the internal processing may increase the 10ms value.
// The eDTS of the next frame will be m_lastDts + defaultFillerFrameDuration + delay. So if this is less than PCR + 10ms, we need to
// insert a filler frame.
//
// Recovery can be done if not only the current frame is on time but the next frame is as well. The problem is that we don't have the next
// frame yet so we don't know if it arrives on time. However, we may assume that if the current frame is more than a frame time ahead of
// presentation, the next frame probably will be on time as well. But it may be 'just in time', risking another filler frame to be inserted
// soon. Regretfully, there is not much we can do about that except for increasing the delay.
// The frame time is extrapolated from the previous frame duration, which is probably quite near correct.
//

Frame *VideoFillerFrameInserter::getNextFrame(TimeStamp pcr)
{
    Frame *frame = checkSource();
    if (frame) {
        TimeStamp dts = frame->m_dts.isValid() ? frame->m_dts : frame->m_pts;
        if (!dts.isValid()) {
            RPLAYER_LOG_WARNING("Cannot process frame with invalid PTS");
            return frame;
        }

        // Analyze the frame so we can create a matching filler frame.
        processNewFrame(frame);

        // Check whether there's an underrun already
        TimeStamp eDts = dts + m_delay + getParams().m_delay; // Compute the expected egress DTS
        if (eDts < pcr + getParams().m_minDelay) {
            // We have an underrun while a frame is present; this should normally never happen in running streams.
            // At start-up it may occur, though, because we may not be able to insert filler frames yet.
            TimeStamp lag = pcr + getParams().m_minDelay - eDts;
            m_delay += lag; // PTS/DTS will be adjusted later on.
            RPLAYER_LOG_INFO("Regular video frame has underrun of %dms, adapting PTS, delay=%ums", static_cast<int32_t>(lag.getAsMilliseconds()), static_cast<uint32_t>(m_delay.getAsMilliseconds()));
        }

        // Check whether we're able to recover latency
        if (m_delay > TimeStamp::zero() && m_lastDts.isValid()) {
            TimeStamp duration = dts - m_lastDts; // Expected duration of this frame
            if (eDts >= pcr + getParams().m_minDelay + getParams().m_clockGranularityAndJitter + duration) {
                TimeStamp correction = (duration > getParams().m_minFrameDistance) ? (duration - getParams().m_minFrameDistance) : TimeStamp::zero();
                if (m_delay >= correction) {
                    m_delay -= correction;
                } else {
                    m_delay = TimeStamp::zero();
                }
                RPLAYER_LOG_INFO("Recovering latency by speeding-up playback, delay=%ums", static_cast<uint32_t>(m_delay.getAsMilliseconds()));
            }
        }

        m_lastDts = dts;

        // Transform to egress PTS/DTS
        frame->m_pts += m_delay + getParams().m_delay;
        if (frame->m_dts.isValid()) {
            frame->m_dts += m_delay + getParams().m_delay;
        }

        // Inform the base class on any delay for the current frame.
        if (m_delay > TimeStamp::zero()) {
            UnderrunAlgorithmBase::notifyDelay(m_delay);
        }
    } else {
        // We have no frame (yet); check whether we need to (and can) create a filler frame.
        if (m_lastDts.isValid()) {
            TimeStamp nextPts = m_lastDts + getParams().m_defaultFillerFrameDuration + m_delay + getParams().m_delay; // The last frame will be extended to the default filler frame duration.
            if (nextPts < pcr + getParams().m_minDelay + getParams().m_clockGranularityAndJitter) {
                frame = generateFillerFrame();
                if (frame) {
                    assert(frame->m_data.size() > 0);
                    m_delay += getParams().m_defaultFillerFrameDuration;
                    frame->m_pts = nextPts;
                    RPLAYER_LOG_INFO("Inserting filler frame after %ums, delay=%ums", static_cast<uint32_t>(getParams().m_defaultFillerFrameDuration.getAsMilliseconds()), static_cast<uint32_t>(m_delay.getAsMilliseconds()));
                }
            }
        }
    }

    return frame;
}
