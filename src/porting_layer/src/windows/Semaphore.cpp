///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <windows.h>
#define NO_MIN_MAX

#include <porting_layer/Semaphore.h>
#include <porting_layer/Log.h>

#include <limits.h>

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
    HANDLE m_semaphore;
};

Semaphore::Semaphore() :
    m_impl(*new SemaphoreImpl())
{
}

SemaphoreImpl::SemaphoreImpl() :
    m_semaphore(0)
{
    m_semaphore = CreateSemaphore(NULL, 0L, INT_MAX, NULL);
    if (m_semaphore == NULL) {
        CTVC_LOG_ERROR("CreateSemaphore error: %ld", GetLastError());
    }
}

SemaphoreImpl::~SemaphoreImpl()
{
    CloseHandle(m_semaphore);
}

void SemaphoreImpl::post()
{
    if (!ReleaseSemaphore(m_semaphore, 1, NULL)) {
        CTVC_LOG_ERROR("ReleaseSemaphore error: %ld", GetLastError());
    }
}

void SemaphoreImpl::wait()
{
    WaitForSingleObject(m_semaphore, INFINITE);
}

bool SemaphoreImpl::wait(uint32_t timeout_in_ms)
{
    return WaitForSingleObject(m_semaphore, timeout_in_ms) == WAIT_OBJECT_0;
}

bool SemaphoreImpl::trywait()
{
    return WaitForSingleObject(m_semaphore, 0L) == WAIT_OBJECT_0;
}
