///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "socketserver.h"

#include <string>
#include <map>

class UpnpServer : public SocketServer
{
    struct ClientData
    {
        int socket;
        UpnpServer *server;
    };

public:
    UpnpServer(std::string dial_host, uint16_t dial_port);
    void set_allowed_hosts(const char *ip);

private:
    virtual bool open_socket();
    virtual void run();

    static void *client_thread(void *arg);
    bool handle_message(char *buffer, int client_socket);

    std::string m_ip_addr;
    std::map<std::string,bool> m_allowed_hosts;

    uint16_t m_dial_port;
};
