///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/Mutex.h>
#include <porting_layer/AutoLock.h>

namespace ctvc {

/// \brief Generic interface for 'atomic' variables.
///
/// This template class helps in creating variables that need to be thread safe.
///
/// \note Only operators that are actually used in the SDK are implemented.
template<typename T> struct Atomic
{
    Atomic()
    {
    }

    /// \brief Construct an atomic value.
    Atomic(const T &value) :
        m_value(value)
    {
    }

    ~Atomic()
    {
    }

    /// \brief Assign new \a value atomically.
    T operator=(const T &value)
    {
        AutoLock l(m_mutex);
        m_value = value;
        return m_value;
    }

    /// \brief Get the value atomically.
    operator T() const
    {
        AutoLock l(m_mutex);
        return m_value;
    }

    /// \brief Increment the value atomically.
    T operator++()
    {
        AutoLock l(m_mutex);
        return ++m_value;
    }

private:
    Atomic(const Atomic &rhs);
    Atomic &operator=(const Atomic &value);

    mutable Mutex m_mutex;
    T m_value;
};

} // namespace
