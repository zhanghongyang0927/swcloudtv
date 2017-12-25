///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Condition.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/Log.h>

#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

using namespace ctvc;

class ConditionImpl : public Condition::ICondition
{
public:
    ConditionImpl();
    ~ConditionImpl();

    void lock();
    void unlock();
    bool trylock();
    void notify();
    void wait_without_lock();
    bool wait_without_lock(uint32_t timeout_in_ms);

private:
    pthread_mutex_t m_mutex;
    pthread_cond_t  m_cond;
    int m_lock_count;
};

Condition::Condition() :
    m_impl(*new ConditionImpl())
{
}

ConditionImpl::ConditionImpl() :
    m_lock_count(0)
{
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
   // pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
    if (pthread_cond_init(&m_cond, &cattr) != 0) {
        CTVC_LOG_ERROR("Failed to create condition variable");
    } else {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (pthread_mutex_init(&m_mutex, &attr) != 0) {
            CTVC_LOG_ERROR("Failed to create mutex");
        }
        pthread_mutexattr_destroy(&attr);
    }
    pthread_condattr_destroy(&cattr);
}

ConditionImpl::~ConditionImpl()
{
    if (pthread_mutex_destroy(&m_mutex) != 0) {
        CTVC_LOG_ERROR("Failed to destroy mutex");
    }
    if (pthread_cond_destroy(&m_cond) != 0) {
        CTVC_LOG_ERROR("Failed to destroy condition variable");
    }
}

void ConditionImpl::lock()
{
    if (pthread_mutex_lock(&m_mutex) != 0) {
        CTVC_LOG_ERROR("Failed to lock mutex");
    }
    m_lock_count++;
}

void ConditionImpl::unlock()
{
    assert(m_lock_count > 0);
    m_lock_count--;
    if (pthread_mutex_unlock(&m_mutex) != 0) {
        CTVC_LOG_ERROR("Failed to unlock mutex");
    }
}

bool ConditionImpl::trylock()
{
    bool is_success = pthread_mutex_trylock(&m_mutex) == 0;
    if (is_success) {
        m_lock_count++;
    }
    return is_success;
}

void ConditionImpl::notify()
{
    if (pthread_cond_signal(&m_cond) != 0) {
        CTVC_LOG_ERROR("Failed to signal condition");
    }
}

void ConditionImpl::wait_without_lock()
{
    // It's required that the mutex is locked here, so lock it in case it isn't
    AutoLock lck(*this);

    // However, pthread_cond_wait() requires the mutex to be locked exactly once...
    int lock_count = m_lock_count;
    while (m_lock_count > 1) {
        unlock();
    }

    if (pthread_cond_wait(&m_cond, &m_mutex) != 0) {
        CTVC_LOG_ERROR("Failed to wait for condition");
    }

    // Restore the original lock count
    while (m_lock_count < lock_count) {
        lock();
    }
}

bool ConditionImpl::wait_without_lock(uint32_t timeout_in_ms)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) {
        CTVC_LOG_ERROR("Can't obtain time");
    }

    uint64_t nsec = 1000000000ULL * t.tv_sec + t.tv_nsec + timeout_in_ms * 1000000ULL;
    t.tv_sec = nsec / 1000000000ULL;
    t.tv_nsec = nsec % 1000000000ULL;

    // It's required that the mutex is locked here, so lock it in case it isn't
    AutoLock lck(*this);

    // However, pthread_cond_wait() requires the mutex to be locked exactly once...
    int lock_count = m_lock_count;
    while (m_lock_count > 1) {
        unlock();
    }

    int ret = pthread_cond_timedwait(&m_cond, &m_mutex, &t);
    if (ret != 0 && ret != ETIMEDOUT) {
        CTVC_LOG_ERROR("Failed to wait for condition");
    }

    // Restore the original lock count
    while (m_lock_count < lock_count) {
        lock();
    }

    return ret == 0;
}
