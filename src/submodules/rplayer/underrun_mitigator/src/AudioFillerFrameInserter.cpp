///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "AudioFillerFrameInserter.h"
#include "AacFillerFrameCreator.h"
#include "MpegAudioFillerFrameCreator.h"
#include "Ac3FillerFrameCreator.h"
#include "UnderrunAlgorithmParams.h"
#include "Frame.h"

#include <rplayer/utils/Logger.h>

#include <assert.h>
#include <string.h>

using namespace rplayer;

AudioFillerFrameInserter::AudioFillerFrameInserter(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback) :
    UnderrunAlgorithmBase(source, params, callback),
    m_repeatCount(0),
    m_delay(TimeStamp::zero()),
    m_fillerFrameCreator(0)
{
}

AudioFillerFrameInserter::~AudioFillerFrameInserter()
{
    delete m_fillerFrameCreator;
}

void AudioFillerFrameInserter::processNewFrame(Frame *frame)
{
    m_repeatCount = 0;

    if (!m_fillerFrameCreator || m_fillerFrameCreator->getStreamType() != getStreamType()) {
        delete m_fillerFrameCreator;
        m_fillerFrameCreator = 0;

        RPLAYER_LOG_INFO("Creating new audio filler frame inserter");

        switch (getStreamType()) {
        case STREAM_TYPE_AAC_AUDIO:
            m_fillerFrameCreator = new AacFillerFrameCreator();
            break;

        case STREAM_TYPE_AC3_AUDIO:
            m_fillerFrameCreator = new Ac3FillerFrameCreator();
            break;

        case STREAM_TYPE_MPEG1_AUDIO:
        case STREAM_TYPE_MPEG2_AUDIO:
            m_fillerFrameCreator = new MpegAudioFillerFrameCreator(getStreamType());
            break;

        default:
            break;
        }
    }

    if (m_fillerFrameCreator) {
        m_fillerFrameCreator->processIncomingFrame(frame);
    }
}

Frame *AudioFillerFrameInserter::generateFillerFrame()
{
    if (++m_repeatCount > getParams().m_repeatedFrameCount && m_fillerFrameCreator) {
        Frame *frame = m_fillerFrameCreator->create();
        if (frame) {
            return frame;
        }
    }

    return new Frame(m_lastAudioFrame);
}

//
// Suppose the ingress stream contains frames 1, 2, 3 and 4.
// They have frame durations of D1, D2, D3 and D4, respectively.
// First PTS is PTS1, so PTS2 == PTS1 + D1.
// In the following scenario, we assume that we need to insert 2 filler frames,
// with a duration of DFF1 and DFF2. The result should be a sequence having the
// following time stamps:
// frame 1: PTS1; m_delay == 0
// frame 2: PTS2 (== PTS1 + D1); m_delay == 0
// filler frame 1: PTS2 + D2 (== PTS3 but we don't have that one yet); m_delay == DFF1
// filler frame 2: PTS2 + D2 + DFF1 (== PTS3 + DFF1); m_delay == DFF1 + DFF2
// frame 3: PTS3 + DFF1 + DFF2 (== PTS2 + D2 + DFF1 + DFF2)
// frame 4: PTS4 + DFF1 + DFF2
//
// Now, audio and video behave differently, typically. Audio is a stream with a fixed sample rate and it is important
// that all audio frames are scheduled back-to-back. Luckily, and because of that, audio frames have a clear duration,
// so the Dx parameter can be determined relatively easily. Also when removing audio frames, it is important to keep
// the stream continuous so the frame duration must be precisely known.
//
// *** Insertion of filler frames for continuous (audio) streams:
// First call to getNextFrame(), m_delay == 0, checkSource() returns frame 1, which should have a valid PTS.
// processNewFrame() will compute D1.
// It computes a filler frame, FF, with a duration DFF.
// The PTS (PTS1) and duration (D1) of the ingress frame are stored in m_lastAudioFrame.
// The frame is displayed, its PTS remains unchanged, equal to PTS1.
//
// Second call to getNextFrame(), m_delay == 0, checkSource() returns frame 2.
// processNewFrame() will compute D2.
// Typically, no new filler frame is computed.
// The PTS (PTS2) and duration (D2) of the ingress frame are stored in m_lastAudioFrame.
// The frame is displayed, its PTS remains unchanged, equal to PTS2.
//
// Third call to getNextFrame(), m_delay == 0, checkSource() returns 0.
// The PTS and duration of m_lastAudioFrame (PTS2 and D2, respectively) are valid and we assume for now that we need a filler frame.
// The filler frame is generated by generateFillerFrame() and has a duration DFF.
// Its PTS is set to the PTS + duration of m_lastAudioFrame, PTS2 + D2, so it will be presented immediately after frame 2; m_delay is set to DFF.
//
// Fourth call to getNextFrame(), m_delay == DFF, checkSource() returns 0.
// The PTS and duration of m_lastAudioFrame (still PTS2 and D2, respectively) are valid and we assume that we need another filler frame.
// The filler frame is again generated by generateFillerFrame() and has a duration DFF.
// Its PTS is set to PTS2 + D2 + DFF, so it will be presented immediately after the previously inserted filler frame; m_delay is increased to 2* DFF.
//
// Fifth call to getNextFrame(), m_delay == 2 * DFF, checkSource() returns frame 3.
// processNewFrame() will compute D3.
// Typically, no new filler frame is computed.
// The PTS (PTS3) and duration (D3) of the ingress frame are stored in m_lastAudioFrame.
// The frame is displayed, its PTS is now set to PTS3 + 2 * DFF. This is correct. We assume for now that there is no recovery yet.
//
// *** When to insert filler frames and when to recover?
// Typically, a frame needs to be fully present before the PCR (program clock reference) reaches its PTS (decode time stamp).
// The minDelay parameter allows for some specific headroom (and latency) to the decoder.
// So a correctly timed frame should have a ePTS >= PCR + minDelay. Consequentially, a frame with ePTS < PCR + minDelay is too late.
// In our code, the egress PTS (ePTS) is equal to the ingress PTS (iPTS) + m_delay + getParams().m_delay, hence the addition of m_delay
// and getParams().m_delay in some of the equations.
//
// We need to insert a filler frame if the ePTS of the next frame otherwise *could* be too late. This is, when in the next iteration
// (at PCR + 10ms) it has an ePTS < (PCR + 10ms) + minDelay. Some jitter in the internal processing may increase the 10ms value.
// The ePTS of the next frame will be m_lastAudioFrame.m_pts + m_lastAudioFrame.m_duration + m_delay. So if this is less than
// PCR + 10ms + minDelay, we need to insert a filler frame.
//
// Recovery can be done if not only the current frame is on time but the next frame is as well. The problem is that we don't have the next
// frame yet so we don't know if it arrives on time. [We could save a frame but that would make this algorithm significantly more complex.]
// With audio, we know the PTS of the next frame, so we may assume that if the current frame is more than a frame time ahead of presentation,
// the next frame probably will be on time as well. But it may be 'just in time', risking another filler frame to be inserted soon.
// Regretfully, there is not much we can do about that except for increasing the delay.
//

Frame *AudioFillerFrameInserter::getNextFrame(TimeStamp pcr)
{
    Frame *frame = checkSource();
    if (frame) {
        if (!frame->m_pts.isValid()) {
            RPLAYER_LOG_WARNING("Cannot process frame with invalid PTS");
            return frame;
        }

        // Analyze the frame: create a matching filler frame and compute frame->m_duration if possible.
        processNewFrame(frame);

        // Save frame contents, but also the last ingress PTS and frame duration
        m_lastAudioFrame = *frame;

        // Transform to egress PTS
        frame->m_pts += m_delay + getParams().m_delay;

        if (frame->m_pts < pcr + getParams().m_minDelay) {
            // We have an underrun while a frame is present; this should normally never happen in running streams.
            // At start-up it may occur, though, because we may not be able to insert filler frames yet.
            TimeStamp lag = pcr + getParams().m_minDelay - frame->m_pts;
            m_delay += lag;
            frame->m_pts += lag;
            RPLAYER_LOG_INFO("Regular audio frame has underrun of %dms, adapting PTS, delay=%ums", static_cast<int32_t>(lag.getAsMilliseconds()), static_cast<uint32_t>(m_delay.getAsMilliseconds()));
        }

        // Check whether we're able to recover latency
        if (m_delay > TimeStamp::zero() && frame->m_duration.isValid()) {
            if (frame->m_pts >= pcr + getParams().m_minDelay + getParams().m_clockGranularityAndJitter + frame->m_duration) {
                if (m_delay >= frame->m_duration) {
                    m_delay -= frame->m_duration;
                    RPLAYER_LOG_INFO("Recovering latency by skipping a frame, length=%ums, delay=%ums", static_cast<uint32_t>(frame->m_duration.getAsMilliseconds()), static_cast<uint32_t>(m_delay.getAsMilliseconds()));
                    delete frame;
                    frame = 0;
                    return getNextFrame(pcr); // Retry. Nice way of saying: goto start;
                }
            }
        }

        // Inform the base class on any delay for the current frame.
        if (m_delay > TimeStamp::zero()) {
            UnderrunAlgorithmBase::notifyDelay(m_delay);
        }
    } else {
        // We have no frame (yet); check whether we need to (and can) create a filler frame.
        if (m_lastAudioFrame.m_pts.isValid() && m_lastAudioFrame.m_duration.isValid()) {
            TimeStamp nextPts = m_lastAudioFrame.m_pts + m_lastAudioFrame.m_duration + m_delay + getParams().m_delay; // The next frame will be inserted right at the end of the previous frame.
            if (nextPts < pcr + getParams().m_minDelay + getParams().m_clockGranularityAndJitter) {
                frame = generateFillerFrame();
                if (frame) {
                    assert(frame->m_data.size() > 0);
                    assert(frame->m_duration.isValid());
                    m_delay += frame->m_duration;
                    frame->m_pts = nextPts;
                    RPLAYER_LOG_INFO("Inserting filler frame, length=%ums, delay=%ums", static_cast<uint32_t>(frame->m_duration.getAsMilliseconds()), static_cast<uint32_t>(m_delay.getAsMilliseconds()));
                }
            }
        }
    }

    return frame;
}