///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "TcpConnection.h"

#include <porting_layer/Log.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/Socket.h>

#include <assert.h>

using namespace ctvc;

const ResultCode TcpConnection::CONNECTION_NOT_OPEN("Trying to send data while the connection is not open");

TcpConnection::TcpConnection(const std::string &thread_name) :
    m_socket(0),
    m_thread(thread_name),
    m_stream_out(0),
    m_do_connect(false),
    m_port(-1)
{
}

TcpConnection::~TcpConnection()
{
    close();
}

ResultCode TcpConnection::open(const std::string &host, int port, bool ssl_flag, IStream &data_out)
{
    CTVC_LOG_DEBUG("host:%s, port:%d, ssl:%d", host.c_str(), port, ssl_flag);

    AutoLock lck(m_mutex);

    // TcpConnection should not be reopened without an intermediate close().
    assert(!m_socket);
    assert(!m_stream_out);

    m_do_connect = true;
    m_host = host;
    m_port = port;
    m_stream_out = &data_out;

    m_socket = ssl_flag ? new SslSocket : new TcpSocket;

    ResultCode ret = m_socket->set_no_delay(true);
    if (ret.is_error()) {
        CTVC_LOG_ERROR("m_socket->set_no_delay() failed");
        close_socket_and_stream();
        return ret;
    }

    ret = m_thread.start(*this, Thread::PRIO_NORMAL);
    if (ret.is_error()) {
        CTVC_LOG_ERROR("m_thread.start() failed");
        close_socket_and_stream();
        return ret;
    }

    return ResultCode::SUCCESS;
}

ResultCode TcpConnection::close()
{
    CTVC_LOG_DEBUG("");

    ResultCode ret = m_thread.stop_and_wait_until_stopped();

    {
        AutoLock lck(m_mutex);
        close_socket_and_stream();
    }

    CTVC_LOG_DEBUG("Done");

    return ret;
}

ResultCode TcpConnection::send_data(const uint8_t *data, uint32_t length)
{
    AutoLock lck(m_mutex);

    if (m_socket) {
        return m_socket->send(data, length);
    } else {
        return CONNECTION_NOT_OPEN;
    }
}

void TcpConnection::close_socket_and_stream()
{
    // Our mutex is locked here
    CTVC_LOG_DEBUG("");

    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = 0;
        m_stream_out = 0;
    }
}

bool TcpConnection::run()
{
    CTVC_LOG_DEBUG("");

    TcpSocket *socket = 0;
    IStream *stream_out = 0;
    bool do_connect = false;
    std::string host;
    int port = -1;

    {
        AutoLock lck(m_mutex);

        socket = m_socket;
        stream_out = m_stream_out;
        do_connect = m_do_connect;

        if (do_connect) {
            m_do_connect = false;

            host = m_host;
            port = m_port;
        }
    }

    assert(socket);
    assert(stream_out);

    if (do_connect) {
        assert(port != -1);
        ResultCode ret = socket->connect(host.c_str(), port);
        if (ret.is_error()) {
            if (ret != Socket::THREAD_SHUTDOWN) {
                CTVC_LOG_ERROR("m_socket->connect(%s,%d) failed", host.c_str(), port);
            } else {
                CTVC_LOG_DEBUG("m_socket->connect(%s,%d) interrupted", host.c_str(), port);
            }

            stream_out->stream_error(ret);

            return true; // Exit thread
        }

        CTVC_LOG_DEBUG("m_socket->connect(%s,%d) successful", host.c_str(), port);
    }

    const unsigned int BUFSIZE = 4096;
    uint8_t *buf = new uint8_t[BUFSIZE];
    uint32_t bytes_received = 0;

    ResultCode ret = socket->receive(buf, BUFSIZE, bytes_received);

    if (ret.is_ok() && bytes_received > 0) {
        CTVC_LOG_DEBUG("Got %d bytes of data", bytes_received);

        // Ownership of 'buf' is passed downstream.
        stream_out->stream_data(buf, bytes_received);
    } else {
        delete[] buf;

        if (ret.is_ok()) { // Connection closed
            assert(bytes_received == 0);
            CTVC_LOG_WARNING("Connection closed by peer");
        } else if (ret == Socket::THREAD_SHUTDOWN) {
            CTVC_LOG_INFO("Connection to be closed by us");
        } else {
            CTVC_LOG_ERROR("Receive failed, ret:%s", ret.get_description());
        }

        stream_out->stream_error(ret);

        return true; // Exit thread
    }

    return false; // Continue looping
}
