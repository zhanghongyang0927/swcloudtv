///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Thread.h>
#include <porting_layer/Mutex.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/Atomic.h>
#include <porting_layer/Log.h>

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "swcloudtv_priv.h"

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
    static pthread_key_t m_key;
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
    pthread_t m_thread_id;
    Mutex m_mutex;
    Atomic<Thread::IRunnable *> m_runnable;
    Atomic<bool> m_is_running;
    Atomic<bool> m_must_stop;
    Atomic<Thread *> m_thread;
    const std::string m_name;

    static void *thread_func(void *arg);
    void *thread_func();
    static ThreadLocalStorage m_tls;
};

Thread::Thread(const std::string &name) :
    m_impl(*new ThreadImpl(*this, name))
{
}

Thread *Thread::self()
{
    return ThreadImpl::self();
}

ThreadImpl::ThreadImpl(Thread &thread, const std::string &name) :
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
        //CLOUDTV_LOG_DEBUG("Thread '%s' already started", m_name.c_str());
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
        //CLOUDTV_LOG_DEBUG("m_must_stop of '%s' is unexpectedly set. Please call the software repairman.", m_name.c_str());
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    sched_param param;
    int policy = SCHED_OTHER;
    param.sched_priority = 0;
    switch (priority) {
    case Thread::PRIO_LOW:
        param.sched_priority = (sched_get_priority_min(policy) * 4 + sched_get_priority_max(policy) * 1) / 5;
		//CLOUDTV_LOG_DEBUG( "priority=%d...\n", param.sched_priority);
        break;
    case Thread::PRIO_NORMAL:
      //  policy = SCHED_FIFO;
        param.sched_priority = (sched_get_priority_min(policy) * 3 + sched_get_priority_max(policy) * 2) / 5;
		//CLOUDTV_LOG_DEBUG( "priority=%d, policy=%d...\n", param.sched_priority, policy);
        break;
    case Thread::PRIO_HIGH:
    //    policy = SCHED_FIFO;
        param.sched_priority = (sched_get_priority_min(policy) * 2 + sched_get_priority_max(policy) * 3) / 5;
		//CLOUDTV_LOG_DEBUG( "priority=%d, policy=%d...\n", param.sched_priority, policy);
        break;
    case Thread::PRIO_HIGHEST:
   //     policy = SCHED_FIFO;
        param.sched_priority = (sched_get_priority_min(policy) * 1 + sched_get_priority_max(policy) * 4) / 5;
		//CLOUDTV_LOG_DEBUG( "priority=%d, policy=%d...\n", param.sched_priority, policy);
        break;
    }
#if 0
	if(policy != SCHED_OTHER)
	{
		if (pthread_attr_setschedpolicy(&attr, policy) != 0 ||
				pthread_attr_setschedparam(&attr, &param) != 0) {
			//CLOUDTV_LOG_DEBUG("Can't set thread priority.\n");
			pthread_attr_destroy(&attr);
			m_runnable = 0;
			return Thread::CANNOT_SET_THREAD_PRIORITY;
		}
    	//CLOUDTV_LOG_DEBUG("Thread prio=%d, policy=%d.\n", param.sched_priority, policy);
	}
#endif

    int res = pthread_create(&m_thread_id, &attr, thread_func, this);
    pthread_attr_destroy(&attr);

    if (res != 0) {
        //CLOUDTV_LOG_DEBUG("pthread_create() of '%s' failed", m_name.c_str());
        m_runnable = 0;
        return Thread::CANNOT_CREATE_THREAD;
    }

    m_is_running = true;
	//CLOUDTV_LOG_DEBUG( "create thrd successfully...\n");

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
        //CLOUDTV_LOG_DEBUG("Thread '%s' not started or already stopped...", m_name.c_str());
        m_must_stop = false;
        return ResultCode::SUCCESS;
    }

    assert(!pthread_equal(m_thread_id, pthread_self()));

    // Wait for the thread to be stopped.
    // The mutex keeps locked in the mean time, preventing further calls to start() or wait_until_stopped() to proceed.
    // However, the thread itself is not blocked since it can't get access to the mutex.
    if (pthread_join(m_thread_id, NULL) != 0) {
        //CLOUDTV_LOG_DEBUG("pthread_join() of '%s' failed", m_name.c_str());
        return Thread::FAILED_WAITING_FOR_THREAD_TO_FINISH;
    }

    m_is_running = false;
    m_must_stop = false;
    m_runnable = 0;

    //CLOUDTV_LOG_DEBUG("pthread_join() of '%s' succeeded", m_name.c_str());

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

void *ThreadImpl::thread_func(void *arg)
{
    return reinterpret_cast<ThreadImpl *>(arg)->thread_func();
}

void *ThreadImpl::thread_func()
{
    // m_thread is atomic
    if (!m_tls.set(m_thread)) {
        //CLOUDTV_LOG_DEBUG("failed to set thread '%s' in local storage", m_name.c_str());
        return 0;  // Fatal error, no point in continuing.
    }

    // m_runnable is atomic
    Thread::IRunnable *runnable = m_runnable;

    while (true) {
        // Run the thread func; we always run the thread func at least once.
        if (runnable->run()) {
            //CLOUDTV_LOG_DEBUG("m_runnable->run() of '%s' stops", m_name.c_str());
            break;
        }

        // m_must_stop is atomic
        if (m_must_stop) {
            break;
        }
    }

    //CLOUDTV_LOG_DEBUG("Thread '%s' stops", m_name.c_str());

    return 0;
}

pthread_key_t ThreadLocalStorage::m_key = 0;

ThreadLocalStorage::ThreadLocalStorage()
{
    pthread_key_create(&m_key, tls_destructor);
}

ThreadLocalStorage::~ThreadLocalStorage()
{
    pthread_key_delete(m_key);
}

void ThreadLocalStorage::tls_destructor(void *)
{
    pthread_setspecific(m_key, NULL);
}

bool ThreadLocalStorage::set(void *data)
{
    return pthread_setspecific(m_key, data) == 0;
}

void *ThreadLocalStorage::get()
{
    return pthread_getspecific(m_key);
}
