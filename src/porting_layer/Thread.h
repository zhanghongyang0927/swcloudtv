///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "ResultCode.h"

#include <string>

#include <inttypes.h>

namespace ctvc {

/// \brief Generic thread interface
///
/// The implementation is done as part of the porting layer. All implementation
/// is forwarded to the private Impl class.  Coding this inline should speed-up
/// code and reduce code size.
class Thread
{
public:
    static const ResultCode THREAD_ALREADY_STARTED;              ///< The thread was already started
    static const ResultCode CANNOT_CREATE_THREAD;                ///< The thread could not be created
    static const ResultCode CANNOT_SET_THREAD_PRIORITY;          ///< The thread priority could not be set
    static const ResultCode FAILED_WAITING_FOR_THREAD_TO_FINISH; ///< The thread could not be joined

    /// \brief Priority levels at which a thread can run.
    enum Priority
    {
        PRIO_LOW, ///< Priority below normal. Such a thread should only run if no other threads can run at that moment.
        PRIO_NORMAL, ///< Normal priority, at which threads run that would otherwise not have their priority set. Typically the same prority as that of the main() thread.
        PRIO_HIGH, ///< Priority above normal. Such a thread should run if possible (i.e. if not blocked), unless a higher priority thread can run.
        PRIO_HIGHEST ///< Highest priority. Such a thread should run at all times if it can (i.e. if not blocked).
    };

    /// \brief Interface of the object that Thread will execute in parallel
    struct IRunnable
    {
        virtual ~IRunnable() {}

        /// \brief This function will run in its own thread
        ///
        /// \retval false It will be run again (looped)
        /// \retval true It will stop and the thread will exit
        virtual bool run() = 0;
    };

    /// \brief Constructor.
    ///
    /// \param[in] thread_name Name of the current thread.
    Thread(const std::string &thread_name);

    ~Thread()
    {
        delete &m_impl;
    }

    /// \brief Sleep for the given number of milliseconds.
    ///
    /// \param[in] time_in_milliseconds Number of milliseconds to sleep.
    static void sleep(uint32_t time_in_milliseconds);

    /// \brief Execute the function in a separate thread.
    ///
    /// \param[in] runnable Object with run() function to be executed in a separated thread.
    /// \param[in] priority Priority at which the thread is expected to run.
    /// \retval ResultCode::SUCCESS When the thread has started successfully.
    /// \retval CANNOT_CREATE_THREAD If the thread couldn't be started, e.g. lack of resources.
    /// \retval THREAD_ALREADY_STARTED If there is already an IRunnable being executed.
    /// \retval CANNOT_SET_THREAD_PRIORITY If the requested thread priority cannot be set.
    ResultCode start(IRunnable &runnable, Priority priority)
    {
        return m_impl.start(runnable, priority);
    }

    /// \brief Stop the current running thread (if any)
    void stop()
    {
        m_impl.stop();
    }

    /// \brief Wait for the thread until it stops
    ///
    /// stop() shall be called previously
    /// \retval ResultCode::SUCCESS If the thread has successfully finished
    /// \retval FAILED_WAITING_FOR_THREAD_TO_FINISH When an error condition ocurred, e.g. no thread was started.
    ResultCode wait_until_stopped()
    {
        return m_impl.wait_until_stopped();
    }

    /// \brief Check if Thread is currently executing an IRunnable object
    ///
    /// \retval True if the thread is running.
    /// \retval False if the thread is not running.
    bool is_running()
    {
        return m_impl.is_running();
    }

    /// \brief Check if Thread must stop
    ///
    /// \retval True if the thread is running and has been signaled to stop.
    /// \retval False if the thread is not running or not signaled to stop.
    bool must_stop()
    {
        return m_impl.must_stop();
    }

    /// \brief Stop the thread and wait until it finishes
    ///
    /// \see stop() and wait_until_stopped()
    /// \return \see wait_until_stopped()
    ResultCode stop_and_wait_until_stopped()
    {
        return m_impl.stop_and_wait_until_stopped();
    }

    /// \brief Get a reference to the thread name.
    ///
    /// \return Reference to the thread name.
    const std::string &get_name() const
    {
        return m_impl.get_name();
    }

    /// \brief Get a pointer to the current thread.
    ///
    /// \return Pointer to current thread or NULL if if not on an explicit thread, e.g. the main thread.
    static Thread *self();

    /// \brief Interface for the implementation of Thread
    ///
    /// Thread uses an object that implements this interface to expose the threading functions.
    /// There is a one-to-one mapping between IThread methods and Thread's, i.e. they have
    /// the same syntax and semantics \see Thread
    /// N.B. the only exception is the static method Tread::self() because by definition static
    /// methods cannot be part of an interface. Furthermore, the implementation is hidden by default.
    struct IThread
    {
        /// \{
        IThread() {}
        virtual ~IThread() {}

        virtual ResultCode start(IRunnable &runnable, Priority priority) = 0;
        virtual void stop() = 0;
        virtual ResultCode wait_until_stopped() = 0;
        virtual bool is_running() = 0;
        virtual bool must_stop() = 0;
        virtual ResultCode stop_and_wait_until_stopped() = 0;
        virtual const std::string &get_name() const = 0;
        /// \}
    };

private:
    Thread(const Thread &);
    Thread &operator=(const Thread &);

    IThread &m_impl;
};

} // namespace
