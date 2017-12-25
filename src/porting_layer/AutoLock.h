///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "Mutex.h"

namespace ctvc {

/// \brief Convenience class to ease scoped Mutex lock and unlock.
///
/// This template class uses the RAII principle.
class AutoLock
{
public:
    explicit AutoLock(IMutex &mutex) :
        m_mutex(mutex)
    {
        m_mutex.lock();
    }

    ~AutoLock()
    {
        m_mutex.unlock();
    }

private:
    AutoLock(const AutoLock &);
    AutoLock &operator=(const AutoLock &);

    IMutex &m_mutex;
};

} // namespace
