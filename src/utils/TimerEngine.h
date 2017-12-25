///
/// \file TimerEngine.h
///
/// \brief CloudTV Nano SDK client timer.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/Thread.h>
#include <porting_layer/Condition.h>
#include <porting_layer/TimeStamp.h>
#include <porting_layer/ResultCode.h>

#include <vector>

#include <inttypes.h>

namespace ctvc {

//
// This class provides timers for one shot or periodic timing purposes.
//

class TimerEngine : public Thread::IRunnable
{
public:
    static const ResultCode NOT_STARTED; //!< The timer engine has not been successfully started.
    static const ResultCode ALREADY_STARTED; //!< The timer engine already has been started.
    static const ResultCode TIMER_ALREADY_REGISTERED; //!< The requested timer has already been registered. (See \a start_timer().)
    static const ResultCode TIMER_NOT_REGISTERED; //!< The requested timer is not registered. (See \a cancel_timer().)
    static const ResultCode ILLEGAL_TIMEOUT; //!< The requested timeout_in_ms has an illegal value. (See \a start_timer().)

    struct ITimer
    {
        virtual ~ITimer()
        {
        }

        /// \brief Called when the timeout expires. This can be called repetitively
        ///        for periodic timers or one time for one-shot timers.
        virtual void timer_expired() = 0;

        /// \brief Called when the timer is canceled.
        virtual void timer_canceled() = 0;

        /// \brief Called when the timer has finished. Neither timer_expired() nor
        ///        timer_canceled() will be called after timer_done() is called.
        ///        The ITimer object may be invalid after timer_done() returns.
        /// \note This call can be used to delete a dynamically allocated ITimer
        ///       object after use.
        virtual void timer_done()
        {
        }
    };

    TimerEngine(const std::string &thread_name);
    ~TimerEngine();

    /// \brief Start the timer engine.
    /// \param[in] priority Priority of the timer thread.
    /// \return ResultCode::SUCCESS if successful, an error code otherwise
    ResultCode start(Thread::Priority priority);

    /// \brief Stop the timer engine and cancel all running timers.
    void stop();

    enum Mode {
        ONE_SHOT, PERIODIC
    };

    /// \brief Start the given timer for the given number of milliseconds.
    /// \param[in] timer ITimer object that is called after the timeout expires
    ///            or when the timer is canceled.
    ///            The timer object must remain valid for the entire lifetime
    ///            of the timer, which is until \a timer_done() is called. This
    ///            is called upon canceling a timer or after a one-shot timer
    ///            expires.
    /// \param[in] timeout_in_ms Number of milliseconds to wait.
    /// \param[in] mode Mode of operation, either ONE_SHOT or PERIODIC.
    /// \return ResultCode::SUCCESS if successful, an error code otherwise.
    ResultCode start_timer(ITimer &timer, uint32_t timeout_in_ms, Mode mode);

    /// \brief Cancel the timer for the given ITimer object. If the timer is
    ///        still registered, timer_canceled() will be called as a result.
    /// \return ResultCode::SUCCESS if successful, an error code otherwise.
    ResultCode cancel_timer(ITimer &timer);

private:
    Thread m_thread;
    Condition m_condition;

    struct TimerEntry
    {
        TimerEntry(ITimer &timer, const TimeStamp &expiration_time, uint32_t timeout_in_ms, Mode mode) :
            m_timer(&timer),
            m_expiration_time(expiration_time),
            m_timeout_in_ms(timeout_in_ms),
            m_mode(mode)
        {
        }

        bool operator==(const ITimer *t) const
        {
            return m_timer == t;
        }

        ITimer *m_timer;
        TimeStamp m_expiration_time;
        uint32_t m_timeout_in_ms;
        Mode m_mode;
    };

    static int compare_func(const void *, const void *);
    void sort_timers();

    // We use a vector and simple linear search. That should be fine for
    // all cases because we expect only a very limited number of timers
    // to be registered at any single point in time.
    std::vector<TimerEntry> m_timers;

    void fire_timers(const TimeStamp &now);

    // Implements Thread::IRunnable
    bool run();
};

// Use this for regular members or otherwise objects that are guaranteed to be valid for the lifetime of the timer functions.
// Typically used as member of a class that also has a TimerEngine object.
template<class C> class BoundTimerEngineTimer : public TimerEngine::ITimer
{
public:
    BoundTimerEngineTimer(C &object, void (C::*expired_function)(), void (C::*canceled_function)()) :
        m_object(object),
        m_expired_function(expired_function),
        m_canceled_function(canceled_function)
    {
    }

    void timer_expired()
    {
        if (m_expired_function) {
            (m_object.*m_expired_function)();
        }
    }

    void timer_canceled()
    {
        if (m_canceled_function) {
            (m_object.*m_canceled_function)();
        }
    }

private:
    C &m_object;
    void (C::*m_expired_function)();
    void (C::*m_canceled_function)();
};

} // namespace
