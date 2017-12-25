///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "socketserver.h"

#include <string>
#include <map>

class MulticastServer : public SocketServer
{
public:
    MulticastServer(std::string ip_addr, int dial_port);
    void set_allowed_hosts(const char *ip);

private:
    virtual bool open_socket();
    virtual void run();

    bool create_socket();
    bool bind_socket();
    bool join_multicast(const char *ip);

    void handle_message(char *msg, struct sockaddr_in *src_addr);

    bool is_dial_ssdp_discover_message(const char *msg) const;
    bool is_dial_multiscreen_ssdp_message(const char *msg) const;
    bool is_dial_netflix_ssdp_message(const char *msg) const;

    void send_multiscreen_reply(struct sockaddr_in *src_addr);
    void send_netflix_reply(struct sockaddr_in *src_addr);

    std::string m_ip_addr;
    int m_dial_port;
    std::map<std::string,bool> m_allowed_hosts;
};
