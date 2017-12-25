///
/// \file LatencyReport.cpp
///
/// \brief CloudTV Nano SDK Latency report implementation.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "LatencyReport.h"

#include <assert.h>

using namespace ctvc;

LatencyReport::LatencyReport() :
    m_measurement_mode(0)
{
}

void LatencyReport::set_measurement_mode(int mode)
{
    m_measurement_mode = mode;
}

void LatencyReport::reset()
{
    m_subtypes.clear();
    m_labels.clear();
    m_data.clear();
}

void LatencyReport::add_entry(Subtype sub_type, const std::string &label, uint64_t data)
{
    m_subtypes.push_back(sub_type);
    m_labels.push_back(label);
    m_data.push_back(data);
}

uint32_t LatencyReport::get_n_entries() const
{
    uint32_t n = m_subtypes.size();

    assert(n == m_labels.size());
    assert(n == m_data.size());

    return n;
}

LatencyReport::Subtype LatencyReport::get_subtype(uint32_t index) const
{
    assert(index < m_subtypes.size());

    return m_subtypes[index];
}

const std::string &LatencyReport::get_label(uint32_t index) const
{
    assert(index < m_labels.size());

    return m_labels[index];
}

uint64_t LatencyReport::get_data(uint32_t index) const
{
    assert(index < m_data.size());

    return m_data[index];
}
