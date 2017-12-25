///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace ctvc {

/// \brief Generic platform-independent semaphore interface
///
class Semaphore
{
public:
    /// \brief Semaphores are counting semaphore objects used for synchronization between threads.
    /// They are constructed with a count of 0.
    Semaphore();

    ~Semaphore()
    {
        delete &m_impl;
    }

    /// \brief Post to the semaphore; the semaphore's count is incremented by 1. If any thread is
    /// blocked on wait(), one of them is released now.
    void post()
    {
        m_impl.post();
    }

    /// \brief Wait for the semaphore to have a count greater than 0. If so, the semaphore's count
    /// is decremented and the call returns. If not, the method will wait indefinitely until some
    /// other thread calls \a post().
    void wait()
    {
        m_impl.wait();
    }

    /// \brief Wait for the semaphore to have a count greater than 0 using a timeout. If the count
    /// is greater than 0 when the call is made, the semaphore's count is decremented and the call
    /// returns immediately with the return value true. If not, the method will wait for at most
    /// \a timeout_in_ms milliseconds for \a post() to be called by another thread. If \a post()
    /// is called before the timeout expires, \a wait() returns true. If \a post() was not called
    /// within \a timeout_in_ms milliseconds, \a wait() returns false.
    /// \param[in] timeout_in_ms The maximum time to wait for another thread to call \a post() before
    ///                          returning false.
    /// \retval true if successful; the semaphore count will be decremented by one.
    /// \retval false if a timeout occurred; the semaphore count will not have changed and still be 0.
    bool wait(uint32_t timeout_in_ms)
    {
        return m_impl.wait(timeout_in_ms);
    }

    /// \brief Check whether the semaphore has a count greater than 0. If so, the semaphore's count
    /// is decremented and the call returns immediately with the return value true. If not, the method
    /// returns immediately with the return value false.
    /// \retval true if successful; the semaphore count will be decremented by one.
    /// \retval false if unsuccessful; the semaphore count will not have changed and still be 0.
    bool trywait()
    {
        return m_impl.trywait();
    }

    /// \brief Interface for the implementation of Semaphore.
    ///
    /// Semaphore uses an object that implements this interface to expose its
    /// functionality. There is a one-to-one mapping between ISemaphore methods and
    /// Semaphore's, i.e. they have the same syntax and semantics \see Semaphore
    struct ISemaphore
    {
        /// \{
        ISemaphore() {}
        virtual ~ISemaphore() {}

        virtual void post() = 0;
        virtual void wait() = 0;
        virtual bool wait(uint32_t timeout_in_ms) = 0;
        virtual bool trywait() = 0;
        /// \}
    };

private:
    Semaphore(const Semaphore &);
    Semaphore &operator=(const Semaphore &);

    ISemaphore &m_impl;
};

} // namespace
