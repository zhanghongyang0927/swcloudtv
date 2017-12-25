///
/// \file PlaybackReport.cpp
///
/// \brief CloudTV Nano SDK Playback report implementation.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "PlaybackReport.h"

using namespace ctvc;

PlaybackReport::PlaybackReport()
{
    // These are the definitions according to CTV-26999;
    const int first_bin_start = 1;
    const uint32_t bin_widths[] = {
        19, 20, 39, 78, 156, 313, 625, 1250, 2500, 5000, 2147473646
    };
    m_bin_definition.add_bins(first_bin_start, bin_widths, sizeof(bin_widths) / sizeof(bin_widths[0]));
}

PlaybackReport::~PlaybackReport()
{
    reset(); // Clean up
}

void PlaybackReport::reset()
{
    m_playback_state.reset();
    m_stalled_duration_in_ms.reset();
    m_current_pts.reset();
    m_pcr_delay.reset();
    m_bandwidth.reset();

    // Remove all present histograms.
    for (std::map<std::string, std::pair<Histogram *, Histogram *> >::iterator i = m_stalled_duration_histograms.begin(); i != m_stalled_duration_histograms.end(); ++i) {
        delete i->second.first;
        delete i->second.second;
    }
    m_stalled_duration_histograms.clear();
}

void PlaybackReport::add_stalled_duration_sample(const std::string &histogram_id, bool is_audio_not_video, int32_t stalled_duration_in_ms)
{
    // Find appropriate histogram and add if needed.
    std::pair<Histogram *, Histogram *> &pair(m_stalled_duration_histograms[histogram_id]);
    Histogram *&p(is_audio_not_video ? pair.first : pair.second);
    if (!p) {
        p = new Histogram(m_bin_definition);
    }

    // Accumulate this sample into the appropriate histogram.
    p->accumulate(stalled_duration_in_ms);
}
