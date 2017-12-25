///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Semaphore.h>
#include <porting_layer/Log.h>
#include <porting_layer/TimeStamp.h>
#include <porting_layer/Thread.h>
#include <porting_layer/Mutex.h>
#include <porting_layer/AutoLock.h>

#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <fcntl.h> // For O_* constants

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
    sem_t *m_semaphore;
};

Semaphore::Semaphore() :
    m_impl(*new SemaphoreImpl())
{
}

SemaphoreImpl::SemaphoreImpl() :
    m_semaphore(0)
{
    static Mutex mutex;

    AutoLock lck(mutex);

    const char *name = "MySemaphore";

    while ((m_semaphore = sem_open(name, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
        switch (errno) {
        case EEXIST:
        case ENOSPC:
        case ENFILE:
            CTVC_LOG_WARNING("Failed to create semaphore, retrying...");
            // Temporary failure
            continue;
        default:
            // Actual failure
            CTVC_LOG_ERROR("Failed to create semaphore");
            break;
        }
    }
    if (sem_unlink(name) < 0) {
        CTVC_LOG_ERROR("Failed to unlink semaphore");
    }
}

SemaphoreImpl::~SemaphoreImpl()
{
    if (sem_close(m_semaphore) < 0) {
        CTVC_LOG_ERROR("Failed to destroy semaphore");
    }
}

void SemaphoreImpl::post()
{
    if (sem_post(m_semaphore) < 0) {
        CTVC_LOG_ERROR("Failed to post semaphore");
    }
}

void SemaphoreImpl::wait()
{
    for (;;) {
        if (sem_wait(m_semaphore) < 0) {
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
    TimeStamp t = TimeStamp::now().add_milliseconds(timeout_in_ms);

    while (true) {
        if (sem_trywait(m_semaphore) == 0) {
            return true;
        }
        if (TimeStamp::now() >= t) {
            return false;
        }
        // This is a bit of a clumsy implementation but should largely work
        Thread::sleep(10);
    }
}

bool SemaphoreImpl::trywait()
{
    return sem_trywait(m_semaphore) == 0;
}
