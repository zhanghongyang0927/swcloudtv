///
/// \file TimerEngine.cpp
///
/// \brief CloudTV Nano SDK client timer.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <utils/TimerEngine.h>

#include <porting_layer/AutoLock.h>
#include <porting_layer/Log.h>

#include <algorithm>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

using namespace ctvc;

const ResultCode TimerEngine::NOT_STARTED("The timer engine has not been successfully started");
const ResultCode TimerEngine::ALREADY_STARTED("The timer engine already has been started");
const ResultCode TimerEngine::TIMER_ALREADY_REGISTERED("The requested timer has already been registered");
const ResultCode TimerEngine::TIMER_NOT_REGISTERED("The requested timer is not registered");
const ResultCode TimerEngine::ILLEGAL_TIMEOUT("The requested timeout_in_ms has an illegal value");

TimerEngine::TimerEngine(const std::string &thread_name) :
    m_thread(thread_name)
{
}

TimerEngine::~TimerEngine()
{
    stop();
}

ResultCode TimerEngine::start(Thread::Priority priority)
{
    AutoLock lck(m_condition);

    if (m_thread.is_running()) {
        return ALREADY_STARTED;
    }

    return m_thread.start(*this, priority);
}

void TimerEngine::stop()
{
    {
        AutoLock lck(m_condition);

        // Cancel all present timers.
        while (!m_timers.empty()) {
            TimerEntry *p = &m_timers.back();

            // Signal the timer that it is canceled.
            p->m_timer->timer_canceled();

            // And signal its removal.
            p->m_timer->timer_done();

            // Remove the given timer entry itself.
            m_timers.pop_back();
        }

        m_thread.stop();
    }

    m_condition.notify();

    m_thread.wait_until_stopped(); // Cannot have the mutex here because it's locked inside the loop as well.
}

bool TimerEngine::run()
{
    const TimeStamp now = TimeStamp::now();

    // Fire any timers that need firing.
    fire_timers(now);

    AutoLock lck(m_condition);

    // Compute the next time the first timer should be fired.
    // From the current expiration time (still standing or rescheduled) we keep the smallest for earliest firing time.
    // We're sorted, so therefore we only have to check the first timer.
    uint32_t wait_time = ~0U;
    if (!m_timers.empty()) {
        // We round up to make sure we wait at least 1 ms, even if the time difference is really small (a few us).
        // Even though we have already fired all expired timers we may still find one that is expired now; this can happen
        // if an expiring timer was added just after fire_timers() was called.
        int64_t diff = (m_timers[0].m_expiration_time - now).get_as_milliseconds();
        if (diff <= 0) {
            diff = 1;
        }
        if (diff < wait_time) {
            wait_time = diff;
        }
    }

    if (!m_thread.must_stop()) {
        // We wait for the specified time.
        // We don't need to check the result code because if we exit early, we are triggered because of a reschedule.
        m_condition.wait_without_lock(wait_time);
    }

    return false; // Continue the thread
}

ResultCode TimerEngine::start_timer(ITimer &timer, uint32_t timeout_in_ms, Mode mode)
{
    if (timeout_in_ms < 1) {
        return ILLEGAL_TIMEOUT;
    }

    AutoLock lck(m_condition);

    if (!m_thread.is_running()) {
        return NOT_STARTED;
    }

    if (std::find(m_timers.begin(), m_timers.end(), &timer) != m_timers.end()) {
        return TIMER_ALREADY_REGISTERED;
    }

    // Put the timer in our list.
    m_timers.push_back(TimerEntry(timer, TimeStamp::now().add_milliseconds(timeout_in_ms), timeout_in_ms, mode));

    // And sort the list
    sort_timers();

    // Trigger the loop so the proper time to wait will be computed.
    m_condition.notify();

    return ResultCode::SUCCESS;
}

ResultCode TimerEngine::cancel_timer(ITimer &timer)
{
    AutoLock lck(m_condition);

    if (!m_thread.is_running()) {
        return NOT_STARTED;
    }

    std::vector<TimerEntry>::iterator it = std::find(m_timers.begin(), m_timers.end(), &timer);
    if (it == m_timers.end()) {
        return TIMER_NOT_REGISTERED;
    }

    // Signal the timer that it is canceled.
    it->m_timer->timer_canceled();

    // Signal its removal.
    it->m_timer->timer_done();

    // And remove the entry from our list.
    m_timers.erase(it);

    return ResultCode::SUCCESS;
}

void TimerEngine::fire_timers(const TimeStamp &now)
{
    std::vector<ITimer *> expired_timers;
    std::vector<ITimer *> removed_timers;

    {
        // The mutex is not yet locked here, so we need to lock it when accessing our data
        AutoLock lck(m_condition);

        // Check all timers that are expired; this is a sorted list, so we can stop at the first non-expired timer.
        bool has_list_changed = false;
        for (size_t i = 0; i < m_timers.size(); i++) {
            TimerEntry &t(m_timers[i]);

            if (now >= t.m_expiration_time) {
                // The timer expired, so we must signal this.
                expired_timers.push_back(t.m_timer);

                // And check what to do next.
                switch (t.m_mode) {
                case ONE_SHOT:
                    // We must remove any firing one-shot timer.
                    // Make sure their removal is signaled.
                    removed_timers.push_back(t.m_timer);

                    // Remove the given timer entry.
                    m_timers.erase(m_timers.begin() + i);

                    i--; // Compensate for i++ at end-of-loop
                    continue; // No further processing for this timer since it was just removed. No resorting needs to be done either.

                case PERIODIC:
                    // Re-schedule a periodic timer.
                    t.m_expiration_time.add_milliseconds(t.m_timeout_in_ms); // Adding time to 'now' would cause time creep.
                    if (now >= t.m_expiration_time) {
                        // Safeguard: we wait at least 1 ms
                        t.m_expiration_time = now;
                        t.m_expiration_time.add_milliseconds(1);
                    }
                    has_list_changed = true; // We'll need to resort the list.
                    break;
                }
            } else {
                // The timer didn't expire so we can quit checking.
                break;
            }
        }

        // And re-sort the list if necessary.
        if (has_list_changed && m_timers.size() > 1) {
            sort_timers();
        }
    }

    // Signal all expired timers while not holding our mutex.
    // (Prevents deadlocks in case the called object just happens to try to access us at the same time.)
    for (size_t i = 0; i < expired_timers.size(); i++) {
        expired_timers[i]->timer_expired();
    }

    // Signal all removed timers while not holding our mutex.
    // (Prevents deadlocks in case the called object just happens to try to access us at the same time.)
    for (size_t i = 0; i < removed_timers.size(); i++) {
        removed_timers[i]->timer_done();
    }
}

int TimerEngine::compare_func(const void *_a, const void *_b)
{
    const TimerEntry &a(*reinterpret_cast<const TimerEntry *>(_a));
    const TimerEntry &b(*reinterpret_cast<const TimerEntry *>(_b));

    return a.m_expiration_time < b.m_expiration_time ? -1 : a.m_expiration_time > b.m_expiration_time ? 1 : 0;
}

void TimerEngine::sort_timers()
{
    qsort(&m_timers[0], m_timers.size(), sizeof(m_timers[0]), &compare_func);
}
