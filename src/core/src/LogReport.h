///
/// \file LogReport.h
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

#include <porting_layer/Log.h>

#include <string>

#include <inttypes.h>

namespace ctvc
{

class LogReport : public ReportBase
{
public:
    LogReport();

    //
    // Configuration
    //

    // Set minimum level. Logs lower than this level won't be added.
    void set_min_level(LogMessageType log_level);

    //
    // Data management
    //

    // Clear all data and state.
    void reset();

    // Add a log message of a certain level.
    void add_log(LogMessageType log_level, const std::string &text);

    //
    // Data access.
    //

    // Get the maximum level of the log messages accumulated up to now.
    LogMessageType get_max_level() const;

    // Get the log text accumulated up to now.
    const std::string &get_text() const;

private:
    LogMessageType m_min_level;
    LogMessageType m_current_max_level;
    std::string m_current_text;
};

}
