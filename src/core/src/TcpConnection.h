///
/// \file TcpConnection.h
///
/// \brief Class that manages a TCP client-side connection, including a receive thread.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <stream/IStream.h>

#include <porting_layer/Thread.h>
#include <porting_layer/Mutex.h>

#include <string>

namespace ctvc {

class TcpSocket;

class TcpConnection : public Thread::IRunnable
{
public:
    static const ResultCode CONNECTION_NOT_OPEN;

    TcpConnection(const std::string &thread_name);
    ~TcpConnection();

    // Open a connection to given host and port (possibly using SSL) and create a
    // receive thread that sends its output to the given IStream object.
    // To prevent data copies, the IStream semantics are different that usual: the
    // data pointer in stream_data() is allocated by the TcpConnection and is expected
    // to be delete[]d by the receiving object. This way, handling of the data can be
    // deferred without having to copy the data.
    ResultCode open(const std::string &host, int port, bool ssl_flag, IStream &data_out);

    // Close the connection and stop the receive thread.
    // The IStream object passed to open() will receive a call to stream_error() with
    // ResultCode::SUCCESS in order to notify the regular close.
    ResultCode close();

    // Send data to the socket
    ResultCode send_data(const uint8_t *data, uint32_t length);

private:
    void close_socket_and_stream();

    // Implementation of Thread::IRunnable
    bool run();

    TcpSocket *m_socket;
    Thread m_thread;
    mutable Mutex m_mutex;
    IStream *m_stream_out;
    bool m_do_connect;
    std::string m_host;
    int m_port;
};

} // namespace
