///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Mutex.h>
#include <porting_layer/Log.h>

#include <windows.h>

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
    HANDLE m_mutex;
};

Mutex::Mutex() :
    m_impl(*new MutexImpl())
{
}

MutexImpl::MutexImpl()
{
    m_mutex = CreateMutex(NULL, false, NULL);
    if (m_mutex == NULL) {
        CTVC_LOG_ERROR("Failed to create mutex");
    }
}

MutexImpl::~MutexImpl()
{
    CloseHandle(m_mutex);
}

void MutexImpl::lock()
{
    if (WaitForSingleObject(m_mutex, INFINITE) != WAIT_OBJECT_0) {
        CTVC_LOG_ERROR("Failed to lock mutex");
    }
}

void MutexImpl::unlock()
{
    if (ReleaseMutex(m_mutex) == 0) {  // In Windows-CE, Zero indicates failure.
        CTVC_LOG_ERROR("Failed to unlock mutex");
    }
}

bool MutexImpl::trylock()
{
    return (WaitForSingleObject(m_mutex, 0) == WAIT_OBJECT_0);
}
