///
/// \file Histogram.cpp
///
/// \brief CloudTV Nano SDK Histogram accumulator implementation.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <utils/Histogram.h>

#include <assert.h>

using namespace ctvc;

Histogram::Histogram(const BinDefinition &bin_definition) :
    m_bin_definition(bin_definition),
    m_n_samples(0)
{
    clear();
}

const Histogram::BinDefinition &Histogram::get_bin_definition() const
{
    return m_bin_definition;
}

void Histogram::clear()
{
    m_n_samples = 0;
    m_entries.clear();
    m_entries.resize(m_bin_definition.get_n_bins());
}

void Histogram::accumulate(int32_t value)
{
    m_n_samples++;
    m_bin_definition.accumulate(value, *this);
}

uint32_t Histogram::get_entry(uint32_t bin_index) const
{
    assert(bin_index < m_entries.size());

    return m_entries[bin_index];
}

uint32_t Histogram::get_n_samples() const
{
    return m_n_samples;
}

Histogram::BinDefinition::BinDefinition()
{
}

void Histogram::BinDefinition::add_bins(int32_t first_bin_start, const uint32_t *bin_widths, uint32_t n_bins)
{
    m_bin_starts.push_back(first_bin_start);

    int32_t bin_start = first_bin_start;
    for (uint32_t i = 0; i < n_bins; i++) {
        bin_start += bin_widths[i];
        m_bin_starts.push_back(bin_start);
    }
}

uint32_t Histogram::BinDefinition::get_n_bins() const
{
    return m_bin_starts.size() - 1;
}

void Histogram::BinDefinition::get_bin_range(uint32_t bin_index, int &start/*out*/, int &end/*out*/) const
{
    assert(bin_index < get_n_bins());

    start = m_bin_starts[bin_index];
    end = m_bin_starts[bin_index + 1];
}

void Histogram::BinDefinition::accumulate(int32_t value, Histogram &histogram) const
{
    assert(m_bin_starts.size() > 0);
    assert(histogram.m_entries.size() == m_bin_starts.size() - 1);

    unsigned first = 0;
    unsigned last = m_bin_starts.size() - 1;
    if (value < m_bin_starts[first] || value >= m_bin_starts[last]) {
        return;
    }
    while (last > first + 1) {
        unsigned mid = (first + last) / 2;
        if (value >= m_bin_starts[mid]) {
            first = mid;
        } else {
            last = mid;
        }
    }
    histogram.m_entries[first]++;
}
