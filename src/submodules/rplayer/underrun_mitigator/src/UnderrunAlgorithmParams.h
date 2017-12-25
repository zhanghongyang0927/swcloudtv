///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TimeStamp.h>

#include <inttypes.h>

namespace rplayer {

// This struct stores the underrun mitigator parameter settings.
struct UnderrunAlgorithmParams
{
public:
    UnderrunAlgorithmParams() :
        m_clockGranularityAndJitter(TimeStamp::zero()),
        m_minFrameDistance(TimeStamp::zero()),
        m_minDelay(TimeStamp::zero()),
        m_defaultFillerFrameDuration(TimeStamp::zero()),
        m_delay(TimeStamp::zero()),
        m_repeatedFrameCount(0)
    {
    }

    TimeStamp m_clockGranularityAndJitter;
    TimeStamp m_minFrameDistance;
    TimeStamp m_minDelay;
    TimeStamp m_defaultFillerFrameDuration;
    TimeStamp m_delay;
    unsigned m_repeatedFrameCount;
};

}
