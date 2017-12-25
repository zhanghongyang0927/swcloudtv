///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <pthread.h>
#include <stdint.h>

class SocketServer
{
public:
    SocketServer(uint16_t port);
    ~SocketServer();

    bool start();
    void stop();

protected:
    virtual bool open_socket() = 0;
    virtual void close_socket();
    virtual void run() = 0;

    uint16_t m_port;
    int m_socket;
    pthread_t m_thread_id;

    static void *server_thread(void *);
    static void termination_handler(int);
};
