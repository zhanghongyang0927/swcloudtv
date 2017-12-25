///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TimeStamp.h>

#include <vector>
#include <inttypes.h>

namespace rplayer {

// This class stores a single audio or video frame and its PTS.
class Frame
{
public:
    Frame()
    {
    }

    Frame(const TimeStamp &pts, const TimeStamp &dts) :
        m_pts(pts),
        m_dts(dts)
    {
    }

    std::vector<uint8_t> m_data;
    TimeStamp m_pts;
    TimeStamp m_dts;
    TimeStamp m_duration;
};

}
