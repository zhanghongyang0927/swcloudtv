///
/// \file LatencyReport.h
///
/// \brief CloudTV Nano SDK Latency report.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "ReportBase.h"
#include "OptionalValue.h"

#include <vector>
#include <string>

#include <inttypes.h>

namespace ctvc
{

class LatencyReport : public ReportBase
{
public:
    enum Subtype
    {
        SUBTYPE_SESSION_START_TO_STREAM,
        SUBTYPE_SESSION_START_TO_FIRSTPAINT,
        SUBTYPE_SESSION_START_TO_COMPLETE,
        SUBTYPE_KEY_TO_DISPLAY,
        SUBTYPE_SESSION_START_BEGIN,
        SUBTYPE_SESSION_START_STREAM,
        SUBTYPE_SESSION_START_FIRSTPAINT_DISPLAY,
        SUBTYPE_SESSION_START_COMPLETE_DISPLAY,
        SUBTYPE_KEY_SENT,
        SUBTYPE_KEY_DISPLAY
    };

    LatencyReport();

    //
    // Configuration
    //

    // Supported measurement modes, or-able flags.
    static const int MEASUREMENT_MODE_DURATION = (1 << 0);
    static const int MEASUREMENT_MODE_EVENT = (1 << 1);

    void set_measurement_mode(int mode);

    //
    // Data management
    //

    // Clear all data and state.
    void reset();

    // Add a latency measurement entry.
    void add_entry(Subtype sub_type, const std::string &label, uint64_t data);

    //
    // Data access
    //

    uint32_t get_n_entries() const;

    Subtype get_subtype(uint32_t index) const;

    const std::string &get_label(uint32_t index) const;

    uint64_t get_data(uint32_t index) const;

private:
    int m_measurement_mode;
    std::vector<Subtype> m_subtypes;
    std::vector<std::string> m_labels;
    std::vector<uint64_t> m_data;
};

}
