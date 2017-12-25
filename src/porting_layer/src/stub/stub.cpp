#include <porting_layer/Socket.h>
#include <porting_layer/Thread.h>
#include <porting_layer/Mutex.h>
#include <porting_layer/Condition.h>
#include <porting_layer/Semaphore.h>
#include <porting_layer/TimeStamp.h>
#include <porting_layer/FileSystem.h>
#include <porting_layer/Keyboard.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <typeinfo>

using namespace ctvc;

///\brief
/// These classes and variables MUST be defined in the porting layer in order to make the application
/// run correctly. This assumes that the standard C library is available and implemented correctly for
/// the target platform. If not, then these function must also be implemented by the porting layer.
///
///\todo Create a suitable porting layer implementation for your platform.
///

const ResultCode Socket::HOST_NOT_FOUND("The given host is not found by the DNS");
const ResultCode Socket::CONNECTION_REFUSED("TCP connection failed to open due to the connection being refused");
const ResultCode Socket::CONNECT_TIMEOUT("TCP connection failed to open because remote server did not respond in time");
const ResultCode Socket::THREAD_SHUTDOWN("A blocking call was interrupted because the calling thread is shut down");

const char ctvc::FILE_SEPARATOR = '/';

class SocketImpl : public Socket::ISocket
{
public:
    SocketImpl()
    {
    }

    virtual ~SocketImpl()
    {
    }

    virtual void open()
    {
    }

    virtual void close()
    {
    }

    virtual ResultCode connect(const char */*host*/, int /*port*/)
    {
        return ResultCode::SUCCESS;
    }

    virtual ResultCode bind(const char */*host*/, int /*port*/)
    {
        return ResultCode::SUCCESS;
    }

    virtual ResultCode send(const uint8_t */*data*/, uint32_t /*length*/)
    {
        return ResultCode::SUCCESS;
    }

    virtual ResultCode receive(uint8_t */*data*/, uint32_t /*size*/, uint32_t &length/*out*/)
    {
        length = 0;
        return ResultCode::SUCCESS;
    }

    virtual ResultCode set_receive_buffer_size(uint32_t /*size*/)
    {
        return ResultCode::SUCCESS;
    }

    virtual ResultCode set_reuse_address(bool /*on*/)
    {
        return ResultCode::SUCCESS;
    }

    virtual ResultCode set_non_blocking(bool /*on*/)
    {
        return ResultCode::SUCCESS;
    }
};

UdpSocket::UdpSocket() :
    Socket(*new SocketImpl)
{
}

TcpSocket::TcpSocket() :
    Socket(*new SocketImpl)
{
}

TcpSocket::TcpSocket(ISocket &impl) :
    Socket(impl)
{
}

ResultCode TcpSocket::listen(uint32_t /*backlog*/)
{
    return ResultCode::SUCCESS;
}

TcpSocket *TcpSocket::accept()
{
    return 0;
}

ResultCode TcpSocket::set_no_delay(bool /*on*/)
{
    return ResultCode::SUCCESS;
}

SslSocket::SslSocket() :
    TcpSocket(*new SocketImpl())
{
}

class ThreadImpl : public Thread::IThread
{
public:
    ThreadImpl()
    {
    }

    virtual ~ThreadImpl()
    {
    }

    ResultCode start(Thread::IRunnable &/*runnable*/, Thread::Priority /*priority*/)
    {
        return ResultCode::SUCCESS;
    }

    void stop()
    {
    }

    ResultCode wait_until_stopped()
    {
        return ResultCode::SUCCESS;
    }

    void set_stopped()
    {
    }

    bool is_running()
    {
        return false;
    }

    bool must_stop()
    {
        return false;
    }

    ResultCode stop_and_wait_until_stopped()
    {
        return ResultCode::SUCCESS;
    }
};

void Thread::sleep(uint32_t /*time_in_milliseconds*/)
{
}

Thread::Thread() :
    m_impl(*new ThreadImpl)
{
}

class MutexImpl : public Mutex::IMutex
{
public:
    MutexImpl()
    {
    }
    ~MutexImpl()
    {
    }

    void lock()
    {
    }
    void unlock()
    {
    }
    bool trylock()
    {
        return true;
    }
};

Mutex::Mutex() :
    m_impl(*new MutexImpl())
{
}

class ConditionImpl : public Condition::ICondition
{
public:
    ConditionImpl()
    {
    }
    ~ConditionImpl()
    {
    }

    void lock()
    {
    }
    void unlock()
    {
    }
    bool trylock()
    {
        return true;
    }
    void notify()
    {
    }
    void wait_without_lock()
    {
    }
    bool wait_without_lock(uint32_t)
    {
        return false;
    }
};

Condition::Condition() :
    m_impl(*new ConditionImpl())
{
}

class SemaphoreImpl : public Semaphore::ISemaphore
{
public:
    SemaphoreImpl()
    {
    }

    ~SemaphoreImpl()
    {
    }

    void post()
    {
    }

    void wait()
    {
    }

    bool wait(uint32_t /*timeout_in_ms*/)
    {
        return false;
    }

    bool trywait()
    {
        return false;
    }
};

Semaphore::Semaphore() :
    m_impl(*new SemaphoreImpl())
{
}

int Keyboard::get_key()
{
    return 0;
}
