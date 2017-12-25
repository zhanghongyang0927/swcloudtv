///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

#include "Mutex.h"

namespace ctvc {

/// \brief Generic condition variable interface
class Condition : public IMutex
{
public:
    Condition();
    ~Condition()
    {
        delete &m_impl;
    }

    /// \brief Lock operation to protect a critical region.
    ///
    /// This method will wait until the mutex can be acquired. The mutex is
    /// recursive, thus if the same thread locks twice the same mutex, it won't
    /// block the execution.
    void lock()
    {
        m_impl.lock();
    }

    /// \brief Unlock operation to indicate the end of a critical region
    void unlock()
    {
        m_impl.unlock();
    }

    /// \brief Try to lock the mutex
    ///
    /// This method never blocks. If the mutex cannot be acquired, it will return false.
    /// \retval true If the mutex has been acquired.
    /// \retval false If the mutex could not be acquired.
    bool trylock()
    {
        return m_impl.trylock();
    }

    /// \brief Notify a thread that is waiting (to be notified).
    ///
    /// \see wait()
    void notify()
    {
        m_impl.notify();
    }

    /// \brief Wait until notified.
    ///
    /// The corresponding mutex will be atomically released when the wait starts. As soon
    /// as the calling thread gets notified, the mutex ownership will be gained (again
    /// atomically).
    /// \see notify()
    void wait_without_lock()
    {
        m_impl.wait_without_lock();
    }

    /// \brief Wait until notified while using a timeout.
    ///
    /// If locked, the corresponding mutex will be atomically released when the wait starts.
    /// As soon as the calling thread gets notified, the mutex ownership will be gained (again
    /// atomically). If not notified, the method will wait for at most \a timeout_in_ms
    /// milliseconds for \a notify() to be called by another thread. If \a notify()
    /// is called before the timeout expires, \a wait_without_lock() returns true.
    /// If \a notify() was not called within \a timeout_in_ms milliseconds,
    /// \a wait_without_lock() returns false.
    /// In both cases the Mutex will be locked again, before returning from this call unless
    /// it was not locked upon enty.
    /// \param[in] timeout_in_ms The maximum time to wait for another thread to call \a notify()
    ///                          before  returning false.
    /// \retval true if successful.
    /// \retval false if a timeout occurred.
    /// \see notify()
    bool wait_without_lock(uint32_t timeout_in_ms)
    {
        return m_impl.wait_without_lock(timeout_in_ms);
    }

    /// \brief Interface for the implementation of Condition
    ///
    /// Condition uses an object that implements this interface to expose its
    /// functionality. There is a one-to-one mapping between ICondition methods and
    /// Condition's, i.e. they have the same syntax and semantics \see Condition
    struct ICondition : public IMutex
    {
        /// \{
        ICondition() {}
        virtual ~ICondition() {}

        virtual void notify() = 0;
        virtual void wait_without_lock() = 0;
        virtual bool wait_without_lock(uint32_t) = 0;
        /// \}
    };

private:
    Condition(const Condition &);
    Condition &operator=(const Condition &);

    ICondition &m_impl;
};

} // namespace
