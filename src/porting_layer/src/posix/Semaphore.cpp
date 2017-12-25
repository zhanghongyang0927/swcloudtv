///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Semaphore.h>
#include <porting_layer/Log.h>

#include <errno.h>
#include <semaphore.h>
#include <time.h>

using namespace ctvc;

class SemaphoreImpl : public Semaphore::ISemaphore
{
public:
    SemaphoreImpl();
    ~SemaphoreImpl();

    void post();
    void wait();
    bool wait(uint32_t timeout_in_ms);
    bool trywait();

private:
    sem_t m_semaphore;
};

Semaphore::Semaphore() :
    m_impl(*new SemaphoreImpl())
{
}

SemaphoreImpl::SemaphoreImpl()
{
    if (sem_init(&m_semaphore, 0, 0) != 0) {
        CTVC_LOG_ERROR("Failed to initialize semaphore");
    }
}

SemaphoreImpl::~SemaphoreImpl()
{
    if (sem_destroy(&m_semaphore) != 0) {
        CTVC_LOG_ERROR("Failed to destroy semaphore");
    }
}

void SemaphoreImpl::post()
{
    if (sem_post(&m_semaphore) != 0) {
        CTVC_LOG_ERROR("Failed to post semaphore");
    }
}

void SemaphoreImpl::wait()
{
    for (;;) {
        if (sem_wait(&m_semaphore) != 0) {
            if (errno == EINTR) {
                continue;
            }
            CTVC_LOG_ERROR("Failed to wait for semaphore");
        }
        break;
    }
}

bool SemaphoreImpl::wait(uint32_t timeout_in_ms)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        return false;
    }

    ts.tv_nsec += (timeout_in_ms % 1000) * 1000000UL;
    ts.tv_sec += timeout_in_ms / 1000 + ts.tv_nsec / 1000000000UL;
    ts.tv_nsec %= 1000000000UL;

    for (;;) {
        if (sem_timedwait(&m_semaphore, &ts) == 0) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        break;
    }

    if (errno != ETIMEDOUT) {
        CTVC_LOG_ERROR("Failed to wait for semaphore");
    }

    return false;
}

bool SemaphoreImpl::trywait()
{
    return sem_trywait(&m_semaphore) == 0;
}
