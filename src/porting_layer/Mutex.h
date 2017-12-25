///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace ctvc {

/// \brief Abstract interface for the implementation of Mutex.
///
/// \see Mutex and Condition
struct IMutex
{
    /// \{
    IMutex() {}
    virtual ~IMutex() {}

    virtual void lock() = 0;
    virtual void unlock() = 0;
    virtual bool trylock() = 0;
    /// \}
};

/// \brief Generic mutex interface.
///
/// Mutex uses an object that implements this interface to expose its
/// functionality. There is a one-to-one mapping between IMutex methods and
/// Mutex's, i.e. they have the same syntax and semantics \see Mutex
class Mutex : public IMutex
{
public:
    Mutex();
    ~Mutex()
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

private:
    Mutex(const Mutex &);
    Mutex &operator=(const Mutex &);

    IMutex &m_impl;
};

} // namespace
