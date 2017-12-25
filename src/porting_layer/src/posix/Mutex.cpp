///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Mutex.h>
#include <porting_layer/Log.h>

#include <pthread.h>

using namespace ctvc;

class MutexImpl : public Mutex::IMutex
{
public:
    MutexImpl();
    ~MutexImpl();

    void lock();
    void unlock();
    bool trylock();

private:
    pthread_mutex_t m_mutex;
};

Mutex::Mutex() :
    m_impl(*new MutexImpl())
{
}

MutexImpl::MutexImpl()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&m_mutex, &attr) != 0) {
        CTVC_LOG_ERROR("Failed to create mutex");
    }
    pthread_mutexattr_destroy(&attr);
}

MutexImpl::~MutexImpl()
{
    if (pthread_mutex_destroy(&m_mutex) != 0) {
        CTVC_LOG_ERROR("Failed to destroy mutex");
    }
}

void MutexImpl::lock()
{
    if (pthread_mutex_lock(&m_mutex) != 0) {
        CTVC_LOG_ERROR("Failed to lock mutex");
    }
}

void MutexImpl::unlock()
{
    if (pthread_mutex_unlock(&m_mutex) != 0) {
        CTVC_LOG_ERROR("Failed to unlock mutex");
    }
}

bool MutexImpl::trylock()
{
    return pthread_mutex_trylock(&m_mutex) == 0;
}
