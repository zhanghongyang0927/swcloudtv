///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "socketserver.h"

#include <map>
#include <string>

class DialApplication;

typedef std::map<std::string, DialApplication*> ApplicationList;

class DialServer : public SocketServer
{
public:
    DialServer(uint16_t port, const char *proxy_dest = 0);
    void register_application(DialApplication *app);

private:
    virtual bool open_socket();
    virtual void run();

    static void *client_thread(void *arg);
    bool proxy_message(char *buffer, int client_socket);
    bool handle_message(char *buffer, int client_socket);
    ApplicationList m_applications;
    std::string m_proxy_dest;
};
