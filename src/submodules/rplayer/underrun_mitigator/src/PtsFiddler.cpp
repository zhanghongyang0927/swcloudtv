///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "PtsFiddler.h"
#include "UnderrunAlgorithmParams.h"
#include "Frame.h"

#include <rplayer/utils/Logger.h>

using namespace rplayer;

PtsFiddler::PtsFiddler(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback) :
    UnderrunAlgorithmBase(source, params, callback)
{
}

PtsFiddler::~PtsFiddler()
{
}

void PtsFiddler::clear()
{
    UnderrunAlgorithmBase::clear();
    m_lastDts.invalidate();
}

Frame *PtsFiddler::getNextFrame(TimeStamp pcr)
{
    Frame *frame = checkSource();
    if (frame) {
        TimeStamp dts = frame->m_dts.isValid() ? frame->m_dts : frame->m_pts;
        dts += getParams().m_delay;
        TimeStamp original = dts;
        if (dts < pcr + getParams().m_minDelay) {
            dts = pcr + getParams().m_minDelay;
        }
        if (m_lastDts.isValid() && dts < m_lastDts + getParams().m_minFrameDistance) {
            dts = m_lastDts + getParams().m_minFrameDistance;
        }
        if (dts != original) {
            TimeStamp diff = dts - original;
            RPLAYER_LOG_INFO("Adjusting DTS from %d to %d (%+6d), PCR=%d, size=%5d, PTS/PCR diff=%d", (int)original.getAs90kHzTicks(), (int)dts.getAs90kHzTicks(), (int)diff.getAs90kHzTicks(), (int)pcr.getAs90kHzTicks(), (int)frame->m_data.size(), (int)(dts - pcr).getAs90kHzTicks());
            // Keep track of the delay measured while compensating underruns.
            UnderrunAlgorithmBase::notifyDelay(diff);
        }
        if (frame->m_dts.isValid()) {
            frame->m_pts += dts - frame->m_dts;
            frame->m_dts = dts;
        } else {
            frame->m_pts = dts;
        }
        m_lastDts = dts;
    }
    return frame;
}
