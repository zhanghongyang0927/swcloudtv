///
/// \file PlaybackReport.h
///
/// \brief CloudTV Nano SDK Playback report.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "ReportBase.h"
#include "OptionalValue.h"

#include <utils/Histogram.h>

#include <map>
#include <string>

#include <inttypes.h>

namespace ctvc
{

class PlaybackReport : public ReportBase
{
public:
    enum PlaybackState
    {
        STARTING,
        PLAYING,
        STALLED,
        STOPPED
    };

    PlaybackReport();
    ~PlaybackReport();

    void reset();

    void add_stalled_duration_sample(const std::string &histogram_id, bool is_audio_not_video, int32_t stalled_duration_in_ms);

    OptionalValue<enum PlaybackState> m_playback_state;
    OptionalValue<uint32_t> m_stalled_duration_in_ms;
    OptionalValue<uint64_t> m_current_pts;
    OptionalValue<uint32_t> m_pcr_delay;
    OptionalValue<uint32_t> m_bandwidth;

    Histogram::BinDefinition m_bin_definition;
    std::map<std::string, std::pair<Histogram *, Histogram *> > m_stalled_duration_histograms;
};

}
