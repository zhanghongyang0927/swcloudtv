///
/// \file ReportManager.h
///
/// \brief CloudTV Nano SDK client report manager.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/ResultCode.h>
#include <porting_layer/TimeStamp.h>

#include <inttypes.h>

namespace ctvc
{

//
// Usage of this class:
// The RFB-TV session manager owns a ReportManager and a Report object for each
// type of report it manages. For instance:
//   ...
//   PlaybackReport m_playback_report;
//   ReportManager m_playback_report_manager;
//   ...
//
// The constructor then initializes
//   ...
//   m_playback_report_manager(m_playback_report, <report transmitter object>),
//   ...
// in which the report transmitter object typically is the session manager itself.
//
// The PlaybackReport derives from ReportBase, so the ReportTransmitter can transmit a generic
// report over RFB-TV, at least in theory.
// Upon receiving the call to request_transmission(), some report fields may be updated by
// querying other objects first before transmitting it. This update should be instantaneous.
//
// The session manager can set the report modes of the ReportManager directly upon receiving
// a ServerCommand message.
//
// Report updates can enter the session manager asynchronously as well. These events should
// update the report and subsequently call the ReportManager's report_updated() method, which
// in turn may trigger a request_transmission().
//

class ReportBase;

struct IReportTransmitter
{
    // The ReportManager requests to transmit a report.
    // The report may acquire some statistics first to fill itself,
    // or transmit itself in its current state - whatever is applicable
    // to the report.
    virtual ResultCode request_transmission(ReportBase &) = 0;
};

class ReportManager
{
public:
    ReportManager(ReportBase &managed_report, IReportTransmitter &report_transmitter);
    ~ReportManager();

    // Report generation control interface

    // Enable triggered reporting. A report will be generated each time a report is updated.
    // May be combined with periodic reports.
    void enable_triggered_reports();

    // Enable/disable interval reporting. A report will regularly be generated based
    // on given interval. An interval of 0 disables periodic reporting.
    // May be combined with triggered reports.
    void enable_periodic_reports(uint32_t interval_in_ms);

    // Disable reporting (triggered and interval reporting)
    // (Because triggered reporting is never disabled without also disabling periodic
    // reporting, there is no distinct API to disable triggered reporting only.)
    void disable_reports();

    // Generate a report *now* irrespective of the enabled reporting modes.
    // Relates to the 'oneshot' command in RFB-TV.
    // May generate a report directly from this thread or deferred from another thread.
    void generate_report();

    // Report update interface

    // Signals the update of a report. May trigger generation of a report if enabled.
    void report_updated();

    // Signals a timer tick.
    // May trigger generation of a report if periodic report generation is enabled.
    // Should be called regularly and frequent enough if periodic report generation is enabled.
    void timer_tick();

    // Indicates if latency reporting is enabled.
    bool is_enabled() const;

private:
    ReportBase &m_managed_report;
    IReportTransmitter &m_report_transmitter;

    bool m_is_triggered_enabled;
    uint32_t m_interval_in_ms;
    TimeStamp m_last_triggered_time;
};

} // namespace
