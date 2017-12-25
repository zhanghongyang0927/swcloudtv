///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Condition.h>
#include <porting_layer/Log.h>

#include <windows.h>

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
    bool wait_without_lock(uint32_t);

private:
    HANDLE           m_mutex;
    HANDLE           m_event;
    uint32_t         m_waiters_count;
    uint32_t         m_lock_count;
    CRITICAL_SECTION m_waiters_count_lock;
    CRITICAL_SECTION m_mutex_count_lock;
};

Condition::Condition() :
    m_impl(*new ConditionImpl())
{
}

ConditionImpl::ConditionImpl() :
    m_mutex(NULL),
    m_event(NULL),
    m_waiters_count(0),
    m_lock_count(0)
{
    m_mutex = CreateMutex(NULL, false, NULL);
    if (m_mutex == NULL) {
        CTVC_LOG_ERROR("Failed to create mutex");
        return;
    }

    m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_event == NULL) {
        CTVC_LOG_ERROR("Failed to create event");
        return;
    }

    InitializeCriticalSection(&m_waiters_count_lock);
    InitializeCriticalSection(&m_mutex_count_lock);
}

ConditionImpl::~ConditionImpl()
{
    if (m_mutex != NULL) {
        CloseHandle(m_mutex);
    }
    if (m_event) {
        CloseHandle(m_event);
    }
}

void ConditionImpl::lock()
{
    if (WaitForSingleObject(m_mutex, INFINITE) != WAIT_OBJECT_0) {
        CTVC_LOG_ERROR("Failed to lock mutex");
    } else {
        EnterCriticalSection(&m_mutex_count_lock);
        m_lock_count++;
        LeaveCriticalSection(&m_mutex_count_lock);
    }
}

void ConditionImpl::unlock()
{
    EnterCriticalSection(&m_mutex_count_lock);
    m_lock_count--;
    LeaveCriticalSection(&m_mutex_count_lock);

    if (ReleaseMutex(m_mutex) == 0) {  // In Windows-CE, a zero indicates failure.
        CTVC_LOG_ERROR("Failed to unlock mutex");
    }
}

bool ConditionImpl::trylock()
{
    return (WaitForSingleObject(m_mutex, 0) == WAIT_OBJECT_0);
}

void ConditionImpl::notify()
{
    EnterCriticalSection(&m_waiters_count_lock);
    bool have_waiters = m_waiters_count > 0;
    LeaveCriticalSection(&m_waiters_count_lock);

    if (have_waiters) {
        SetEvent(m_event);
    }
}

void ConditionImpl::wait_without_lock()
{
    EnterCriticalSection(&m_waiters_count_lock);
    m_waiters_count++;
    LeaveCriticalSection(&m_waiters_count_lock);

    EnterCriticalSection(&m_mutex_count_lock);
    uint32_t lock_count = m_lock_count;

    // It's ok to release the mutex here and now since Win32
    // manual-reset events maintain state when used with
    // SetEvent. (This avoids the "lost wakeup" bug...)
    while (m_lock_count) {
        unlock();
    }
    LeaveCriticalSection(&m_mutex_count_lock);

    // Wait for either event to become signaled due to 'notify()' being called.
    // Because 'm_event' is auto-reset, we don't have to call ResetEvent().
    if (WaitForSingleObject(m_event, INFINITE) != WAIT_OBJECT_0) {
        CTVC_LOG_ERROR("Failed to wait for event");
    }

    EnterCriticalSection(&m_waiters_count_lock);
    m_waiters_count--;
    LeaveCriticalSection (&m_waiters_count_lock);

    // Restore the original lock count
    for (uint32_t i = 0; i < lock_count; i++) {
        lock();
    }
}

bool ConditionImpl::wait_without_lock(uint32_t timeout_in_ms)
{
    EnterCriticalSection(&m_waiters_count_lock);
    m_waiters_count++;
    LeaveCriticalSection(&m_waiters_count_lock);

    EnterCriticalSection(&m_mutex_count_lock);
    uint32_t lock_count = m_lock_count;
    // It's ok to release the mutex here and now since Win32
    // manual-reset events maintain state when used with
    // SetEvent. (This avoids the "lost wakeup" bug...)
    while (m_lock_count) {
        unlock();
    }
    LeaveCriticalSection(&m_mutex_count_lock);

    bool result = false;

    // Wait for either event to become signaled due to 'notify()' being called.
    // Because 'm_event' is auto-reset, we don't have to call ResetEvent().
    DWORD res = WaitForSingleObject(m_event, timeout_in_ms);
    if (res == WAIT_OBJECT_0) {
        result = true;
    } else if (res != WAIT_TIMEOUT) {
        CTVC_LOG_ERROR("Failed to wait for event");
    }

    EnterCriticalSection(&m_waiters_count_lock);
    m_waiters_count--;
    LeaveCriticalSection (&m_waiters_count_lock);

    // Restore the original lock count
    for (uint32_t i = 0; i < lock_count; i++) {
        lock();
    }

    return result;
}
