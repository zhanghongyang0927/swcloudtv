///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "ReportManager.h"

using namespace ctvc;

ReportManager::ReportManager(ReportBase &managed_report, IReportTransmitter &report_transmitter) :
    m_managed_report(managed_report),
    m_report_transmitter(report_transmitter),
    m_is_triggered_enabled(false),
    m_interval_in_ms(0)
{
}

ReportManager::~ReportManager()
{
}

void ReportManager::enable_triggered_reports()
{
    bool do_trigger = !m_is_triggered_enabled;

    m_is_triggered_enabled = true;

    // Send a report if triggered sending has just been enabled
    if (do_trigger) {
        generate_report();
    }
}

void ReportManager::enable_periodic_reports(uint32_t interval_in_ms)
{
    // Just set the interval. Periodic transmission will adapt automatically
    // if timer_tick() is called frequently.
    m_interval_in_ms = interval_in_ms;
}

void ReportManager::disable_reports()
{
    m_is_triggered_enabled = false;
    m_interval_in_ms = 0;
}

bool ReportManager::is_enabled() const
{
    return m_is_triggered_enabled || m_interval_in_ms > 0;
}

void ReportManager::generate_report()
{
    // Take every report sent into account for timing periodic reports
    m_last_triggered_time = TimeStamp::now();

    // Send the report
    m_report_transmitter.request_transmission(m_managed_report);
}

void ReportManager::report_updated()
{
    // Send a report if triggered sending is enabled
    if (m_is_triggered_enabled) {
        generate_report();
    }
}

void ReportManager::timer_tick()
{
    // Send a report if periodic report generation is enabled...
    if (m_interval_in_ms > 0) {
        // ...and it's time to generate a new report.
        if ((TimeStamp::now() - m_last_triggered_time).get_as_milliseconds() >= m_interval_in_ms) {
            generate_report();
        }
    }
}
