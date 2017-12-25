///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "mcastserver.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

MulticastServer::MulticastServer(std::string ip_addr, int dial_port) :
    SocketServer(1900),
    m_ip_addr(ip_addr),
    m_dial_port(dial_port)
{
}

void MulticastServer::set_allowed_hosts(const char *ip)
{
    m_allowed_hosts[std::string(ip)] = true;
}

bool MulticastServer::open_socket()
{
    return create_socket() && bind_socket() && join_multicast("239.255.255.250");
}

bool MulticastServer::create_socket()
{
    assert(m_socket == -1);

    m_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == -1) {
        fprintf(stderr, "failed to create a UDP socket: %s\n", strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    return true;
}

bool MulticastServer::bind_socket()
{
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // bind socket to any interface
    server_address.sin_port = htons(m_port);

    if (bind(m_socket, reinterpret_cast<struct sockaddr *>(&server_address), sizeof(server_address)) < 0) {
        fprintf(stderr, "bind to port 1900 failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}

bool MulticastServer::join_multicast(const char *ip)
{
    struct ip_mreq mcast_address;
    memset(&mcast_address, 0, sizeof(mcast_address));
    mcast_address.imr_multiaddr.s_addr = inet_addr(ip);
    mcast_address.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const void *>(&mcast_address), sizeof(mcast_address)) != 0) {
        fprintf(stderr, "failed to join multicast group: %s\n", strerror(errno));
        return false;
    }
    return true;
}

void MulticastServer::run()
{
    char buffer[2048];

    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    ssize_t nbytes;

    while ((nbytes = recvfrom(m_socket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<struct sockaddr *>(&src_addr), &addrlen)) > 0) {
        buffer[nbytes] = '\0';
        handle_message(buffer, &src_addr);
    }

    printf("DialServer::run() ended errno:%d => '%s'\n", errno, strerror(errno));
}

void MulticastServer::handle_message(char *msg, struct sockaddr_in *src_addr)
{
    const char *remote_host = inet_ntoa(src_addr->sin_addr);

    if ((m_allowed_hosts.size() == 0) || (m_allowed_hosts.find(remote_host) != m_allowed_hosts.end())) {
        if (is_dial_ssdp_discover_message(msg)) {
            printf("@@@@@@@ received DIAL service discovery query from '%s'\n", remote_host);
            if (is_dial_multiscreen_ssdp_message(msg)) {
                printf("Send multiscreen SSDP reply\n");
                send_multiscreen_reply(src_addr);
            } else if (is_dial_netflix_ssdp_message(msg)) {
                printf("Send Netflix SSDP reply\n");
                send_netflix_reply(src_addr);
            } else {
                //printf("@@@ UNKNOWN REQUEST: %s\n", msg);
            }
        } else {
            //printf("@@@ UNKNOWN REQUEST: %s\n", msg);
        }
    }
}

bool MulticastServer::is_dial_ssdp_discover_message(const char *msg) const
{
    return ((strstr(msg, "M-SEARCH * HTTP/1.1") != NULL) &&
            (strstr(msg, "MAN: \"ssdp:discover\"") != NULL));
}

bool MulticastServer::is_dial_multiscreen_ssdp_message(const char *msg) const
{
    return (strstr(msg, "ST: urn:dial-multiscreen-org:service:dial:1") != NULL);
}

bool MulticastServer::is_dial_netflix_ssdp_message(const char *msg) const
{
    return (strstr(msg, "ST: urn:mdx-netflix-com:service:target:1") != NULL);
}

void MulticastServer::send_multiscreen_reply(struct sockaddr_in *src_addr)
{
    const char ssdp_reply[] =
        "HTTP/1.1 200 OK\r\n"
        "LOCATION: http://%s:52235/dd.xml\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "EXT:\r\n"
        "BOOTID.UPNP.ORG: 1\r\n"
        "SERVER: Linux/2.6 UPnP/1.0 quick_ssdp/1.0\r\n"
        "ST: urn:dial-multiscreen-org:service:dial:1\r\n"
        "USN: uuid:21e76d22-3fc9-49ce-aeeb-f938de6033e9::urn:dial-multiscreen-org:service:dial:1\r\n"
        "\r\n";

    char buffer[sizeof(ssdp_reply) + m_ip_addr.size() + 1];
    snprintf(buffer, sizeof(buffer), ssdp_reply, m_ip_addr.c_str());

    socklen_t addrlen = sizeof(*src_addr);
    sendto(m_socket, buffer, strlen(buffer), 0, reinterpret_cast<struct sockaddr *>(src_addr), addrlen);
}

void MulticastServer::send_netflix_reply(struct sockaddr_in *src_addr)
{
    const char ssdp_reply[] =
        "HTTP/1.1 200 OK\r\n"
        "LOCATION: http://%s:%d\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "EXT:\r\n"
        "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
        "01-NLS: 349f6662-1dd2-11b2-bc7e-e0ecf5f88667\r\n"
        "SERVER: Linux/2.6 UPnP/1.0 quick_ssdp/1.0\r\n"
        "X-User-Agent: NRDP MDX\r\n"
        "X-Friendly-Name: CloudTV-Nano-Client\r\n"
        "X-Accepts-Registration: 3\r\n"
        "X-MSL: 1\r\n"
        "X-MDX-Caps: mdx,http\r\n"
        "X-MDX-ID: 11e7df68e93363adaafb331ceb1f8624\r\n"
        "X-MDX-Registered: 1\r\n"
        "ST: urn:mdx-netflix-com:service:target:1\r\n"
        "USN: uuid:CTVC-NANO_PREMIUM-41234124124::urn:mdx-netflix-com:service:target:1\r\n"
        "\r\n";

    char buffer[sizeof(ssdp_reply) + 16 + m_ip_addr.size() + 1];
    snprintf(buffer, sizeof(buffer), ssdp_reply, m_ip_addr.c_str(), m_dial_port);

    socklen_t addrlen = sizeof(*src_addr);
    sendto(m_socket, buffer, strlen(buffer), 0, reinterpret_cast<struct sockaddr *>(src_addr), addrlen);
}
