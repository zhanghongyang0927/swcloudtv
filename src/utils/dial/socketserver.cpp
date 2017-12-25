///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "socketserver.h"

#include <sys/socket.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>


SocketServer::SocketServer(uint16_t port) : m_port(port), m_socket(-1)
{
}

SocketServer::~SocketServer()
{
    stop();
}

void SocketServer::close_socket()
{
    if (m_socket != -1) {
        close(m_socket);
        m_socket = -1;
    }
}

bool SocketServer::start()
{
    if (m_socket != -1) {
        fprintf(stderr, "server already active\n");
        return false;
    }

    if (!open_socket()) {
        fprintf(stderr, "failed to open socket\n");
        return false;
    }

    if (pthread_create(&m_thread_id, NULL, &server_thread, this) != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(errno));
        close_socket();
        return false;
    }

    pthread_detach(m_thread_id); // We don't join, so automatically free allocated resources.
    printf("server thread with id %d created\n", (int)m_thread_id);

    return true;
}

void SocketServer::stop()
{
    if (m_socket != -1) {
        // Signal the thread, so that any blocking calls are interrupted
        struct sigaction new_action;
        new_action.sa_handler = termination_handler;
        sigemptyset(&new_action.sa_mask);
        new_action.sa_flags = 0;
        sigaction(SIGINT, &new_action, NULL);
        pthread_kill(m_thread_id, SIGINT);
        pthread_join(m_thread_id, NULL);
    }
}

void SocketServer::termination_handler(int) /* static */
{
}

void *SocketServer::server_thread(void *arg) /* static */
{
    SocketServer *server = reinterpret_cast<SocketServer*>(arg);
    server->run();
    server->close_socket();
    return NULL;
}
