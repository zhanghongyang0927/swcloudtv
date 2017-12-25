///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <windows.h>

#define NO_MIN_MAX

#include <porting_layer/Thread.h>
#include <porting_layer/Mutex.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/Atomic.h>
#include <porting_layer/Log.h>

#include <string.h>

using namespace ctvc;

const ResultCode Thread::THREAD_ALREADY_STARTED("The thread has already been started");
const ResultCode Thread::CANNOT_CREATE_THREAD("Unable to create thread");
const ResultCode Thread::CANNOT_SET_THREAD_PRIORITY("Unable to set the thread priority");
const ResultCode Thread::FAILED_WAITING_FOR_THREAD_TO_FINISH("Failed waiting for thread to finish");

// Abstraction of Thread Local Storage AKA 'TLS'
class ThreadLocalStorage
{
public:
    ThreadLocalStorage();
    ~ThreadLocalStorage();
    bool set(void *);  // Returns 'true' if value was successfully set in thread local storage
    void *get();
private:
    static DWORD m_key;
    static void tls_destructor(void *);
};

class ThreadImpl : public Thread::IThread
{
public:
    ThreadImpl(Thread &thread, const std::string &name);
    virtual ~ThreadImpl();

    ResultCode start(Thread::IRunnable &runnable, Thread::Priority priority);
    void stop();
    ResultCode wait_until_stopped();
    bool is_running();
    bool must_stop();
    ResultCode stop_and_wait_until_stopped();
    const std::string &get_name() const;
    static Thread *self();

protected:
    HANDLE m_thread_handle;
    Mutex m_mutex;
    Atomic<Thread::IRunnable *> m_runnable;
    Atomic<bool> m_is_running;
    Atomic<bool> m_must_stop;
    Atomic<Thread *> m_thread;
    const std::string m_name;

    static DWORD thread_func(void *arg);
    DWORD thread_func();
    static ThreadLocalStorage m_tls;
};

void Thread::sleep(uint32_t time_in_milliseconds)
{
    Sleep(time_in_milliseconds);
}

Thread::Thread(const std::string &name) :
    m_impl(*new ThreadImpl(*this, name))
{
}

Thread *Thread::self()
{
    return ThreadImpl::self();
}

ThreadImpl::ThreadImpl(Thread &thread, const std::string &name) :
    m_thread_handle(NULL),
    m_runnable(0),
    m_is_running(false),
    m_must_stop(false),
    m_thread(&thread),
    m_name(name)
{
}

ThreadImpl::~ThreadImpl()
{
    stop_and_wait_until_stopped();

    if (m_thread_handle != NULL) {
        CloseHandle(m_thread_handle);
        m_thread_handle = NULL;
    }
}

const std::string &ThreadImpl::get_name() const
{
    return m_name;
}

ThreadLocalStorage ThreadImpl::m_tls;

Thread *ThreadImpl::self()
{
    return static_cast<Thread*>(m_tls.get());
}

ResultCode ThreadImpl::start(Thread::IRunnable &runnable, Thread::Priority priority)
{
    AutoLock lck(m_mutex);

    if (m_is_running) {
        CTVC_LOG_ERROR("Thread '%s' already started", m_name.c_str());
        return Thread::THREAD_ALREADY_STARTED;
    }

    m_runnable = &runnable;

    // Don't reset m_must_stop.
    // Advantage of resetting is that calls to stop() before start() will have no effect, but
    // the disadvantage is that a start() happening between a stop() and a wait_until_stopped()
    // will lead to a deadlock. (A race condition that might happen if start() and stop() are
    // called from different threads in a non-deterministic way.)
    // Normally a stop() will always be followed by a wait_until_stopped() so m_must_stop being
    // true here would be an error condition anyway.
    // Therefore we issue an error message instead - we should fix the root cause if this ever barks.
    // m_must_stop = false;
    if (m_must_stop) {
        CTVC_LOG_ERROR("m_must_stop of '%s' is unexpectedly set. Please call the software repairman.", m_name.c_str());
    }

    DWORD thread_id;
    m_thread_handle = CreateThread(NULL, 0, thread_func, (void*)this, 0, &thread_id);
    if (m_thread_handle == NULL) {
        CTVC_LOG_ERROR("Unable to create thread '%s'", m_name.c_str());
        m_runnable = 0;
        return Thread::CANNOT_CREATE_THREAD;
    }

    m_is_running = true;

    int winCePriority = THREAD_PRIORITY_NORMAL;
    switch (priority) {
    case PRIO_LOW:
        winCePriority = THREAD_PRIORITY_BELOW_NORMAL
        break;
    case PRIO_NORMAL:
        winCePriority = THREAD_PRIORITY_NORMAL
        break;
    case PRIO_HIGH:
        winCePriority = THREAD_PRIORITY_ABOVE_NORMAL
        break;
    case PRIO_HIGHEST:
        winCePriority = THREAD_PRIORITY_HIGHEST
        break;
    }
    if (!SetThreadPriority(m_thread_handle, winCePriority)) {
        CTVC_LOG_ERROR("Can't set thread priority");
        stop_and_wait_until_stopped();
        return CANNOT_SET_THREAD_PRIORITY;
    }

    return ResultCode::SUCCESS;
}

void ThreadImpl::stop()
{
    // m_must_stop is atomic
    m_must_stop = true;
}

ResultCode ThreadImpl::wait_until_stopped()
{
    // If multiple threads wait for a stop, let them wait for one another
    AutoLock lck(m_mutex);

    if (!m_is_running) {
        CTVC_LOG_DEBUG("Thread '%s' not started or already stopped...", m_name.c_str());
        m_must_stop = false;
        return ResultCode::SUCCESS;
    }

    if (m_thread_handle != NULL) {
        if (WaitForSingleObject(m_thread_handle, INFINITE) != WAIT_OBJECT_0) {
            CTVC_LOG_ERROR("WaitForSingleObject() of '%s' failed", m_name.c_str());
            return Thread::FAILED_WAITING_FOR_THREAD_TO_FINISH;
        }
    }

    m_is_running = false;
    m_must_stop = false;
    m_runnable = 0;

    CTVC_LOG_DEBUG("WaitForSingleObject() of '%s' succeeded", m_name.c_str());

    return ResultCode::SUCCESS;
}

bool ThreadImpl::is_running()
{
    // m_is_running is atomic
    return m_is_running;
}

bool ThreadImpl::must_stop()
{
    // m_must_stop is atomic
    return m_must_stop;
}

ResultCode ThreadImpl::stop_and_wait_until_stopped()
{
    // Make the combination atomic so no start() or other bodily thread method can be called in between
    AutoLock lck(m_mutex);
    stop();
    return wait_until_stopped();
}

DWORD ThreadImpl::thread_func(void *arg)
{
    return reinterpret_cast<ThreadImpl *>(arg)->thread_func();
}

DWORD ThreadImpl::thread_func()
{
    // m_thread is atomic
    if (!m_tls.set(m_thread)) {
        CTVC_LOG_ERROR("failed to set thread '%s' in local storage", m_name.c_str());
        return 0;  // Fatal error, no point in continuing.
    }

    // m_runnable is atomic
    Thread::IRunnable *runnable = m_runnable;

    while (true) {
        // Run the thread func; we always run the thread func at least once.
        if (runnable->run()) {
            CTVC_LOG_INFO("m_runnable->run() of '%s' stops", m_name.c_str());
            break;
        }

        // m_must_stop is atomic
        if (m_must_stop) {
            break;
        }
    }

    CTVC_LOG_INFO("Thread '%s' stops", m_name.c_str());

    return 0;
}

DWORD ThreadLocalStorage::m_key = 0;

ThreadLocalStorage::ThreadLocalStorage()
{
    m_key = TlsAlloc();
}

ThreadLocalStorage::~ThreadLocalStorage()
{
    TlsFree(m_key);
}

void ThreadLocalStorage::tls_destructor(void *)
{
    TlsSetValue(m_key, NULL);
}

bool ThreadLocalStorage::set(void *data)
{
    return TlsSetValue(m_key, data) != FALSE;
}

void *ThreadLocalStorage::get()
{
    return TlsGetValue(m_key);
}
