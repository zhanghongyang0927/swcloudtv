///
/// \file Histogram.h
///
/// \brief CloudTV Nano SDK Histogram accumulator to be used in reporting.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <vector>

#include <inttypes.h>

namespace ctvc
{

//
// This class is intended to store histogram data delivered by the underrun mitigator and
// forward it to RFB-TV playback reports. These histograms are fixed 11-bin histograms with
// exponential bin size (defined in RFB-TV and by CTV-26999).
// This class is a little bit more flexible though, so it will be easy to either change
// the format later on or to add other histograms with a different definition.
// The underrun mitigator histograms measure underrun occurrences in 33 possible streaming
// domains: 2 for each of the 16 possible RAMS stream ID values (one for audio and one for
// video per stream ID) plus 1 to measure any underruns occurring in the middleware decoder
// (player) itself. We try to make efficient use of the storage space by only assigning
// storage for streams that are actually used (and events actually measured).
//
// To save duplication of information, the HistogramBinDefinition class keeps track of the
// bin size definition and the data itself is kept in a separate Histogram object.
//

class Histogram
{
public:
    class BinDefinition
    {
    public:
        BinDefinition();

        // Add bins to the definition
        void add_bins(int32_t first_bin_start, const uint32_t *bin_widths, uint32_t n_bins);

        // The number of bins in this histogram
        uint32_t get_n_bins() const;

        // Get the range value of a certain bin
        // The start value is included and the end value is excluded in the range
        void get_bin_range(uint32_t bin_index, int &start/*out*/, int &end/*out*/) const;

    private:
        friend class Histogram;

        std::vector<int32_t> m_bin_starts;

        // Accumulate a value into given histogram data
        void accumulate(int32_t value, Histogram &histogram) const;
    };

    Histogram(const BinDefinition &bin_definition);

    // Get the associated bin definition
    const BinDefinition &get_bin_definition() const;

    // Clear all accumulated histogram data
    void clear();

    // Accumulate a value into given histogram data
    void accumulate(int32_t value);

    // Get the accumulated histogram data
    uint32_t get_entry(uint32_t bin_index) const;

    // Get the total number of accumulated samples
    uint32_t get_n_samples() const;

private:
    const BinDefinition &m_bin_definition;

    std::vector<uint32_t> m_entries;
    uint32_t m_n_samples;
};

}
