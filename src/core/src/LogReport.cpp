///
/// \file LogReport.cpp
///
/// \brief CloudTV Nano SDK Log report implementation.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "LogReport.h"

using namespace ctvc;

static const LogMessageType DEFAULT_MIN_LEVEL = LOG_WARNING;
static const LogMessageType GLOBAL_MIN_LEVEL = LOG_DEBUG;
static const uint32_t MAX_LOG_SIZE = 65535; // In bytes; RFB-TV strings cannot be any longer.

LogReport::LogReport() :
    m_min_level(DEFAULT_MIN_LEVEL),
    m_current_max_level(GLOBAL_MIN_LEVEL)
{
}

void LogReport::set_min_level(LogMessageType log_level)
{
    m_min_level = log_level;
    if (m_current_text.empty()) {
        m_current_max_level = log_level;
    }
}

void LogReport::reset()
{
    m_current_max_level = m_min_level;
    m_current_text = "";
}

void LogReport::add_log(LogMessageType level, const std::string &text)
{
    if (level > m_min_level) { // Enum values are opposite from levels...
        return;
    }

    if (m_current_text.empty() || level < m_current_max_level) {
        m_current_max_level = level;
    }

    if (m_current_text.size() + text.size() > MAX_LOG_SIZE) {
        m_current_text = m_current_text.substr(m_current_text.size() + text.size() - MAX_LOG_SIZE) + text;
    } else {
        m_current_text += text;
    }
}

LogMessageType LogReport::get_max_level() const
{
    return m_current_max_level;
}

const std::string &LogReport::get_text() const
{
    return m_current_text;
}
