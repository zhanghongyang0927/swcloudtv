///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "upnpserver.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const char ddxml[] = ""
    "<?xml version=\"1.0\"?>"
    "<root"
    "  xmlns=\"urn:schemas-upnp-org:device-1-0\""
    "  xmlns:r=\"urn:restful-tv-org:schemas:upnp-dd\">"
    "  <specVersion>"
    "  <major>1</major>"
    "  <minor>0</minor>"
    "  </specVersion>"
    "  <device>"
    "  <deviceType>urn:schemas-upnp-org:device:tvdevice:1</deviceType>"
    "  <friendlyName>ActiveVideo CloudTV Client</friendlyName>"
    "  <manufacturer>ActiveVideo</manufacturer>"
    "  <modelName>Nano 4.0</modelName>"
    "  <UDN>uuid:21e76d22-3fc9-49ce-aeeb-f938de6033ea</UDN>"
    "  </device>"
    "</root>";

UpnpServer::UpnpServer(std::string dial_host, uint16_t dial_port) :
    SocketServer(52235), m_ip_addr(dial_host), m_dial_port(dial_port)
{
}

void UpnpServer::set_allowed_hosts(const char *ip)
{
    m_allowed_hosts[std::string(ip)] = true;
}

bool UpnpServer::open_socket()
{
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == -1) {
        fprintf(stderr, "Failed to create socket: %s", strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);   // TODO: make this dynamically allocate a free socket (0)
    addr.sin_addr.s_addr = INADDR_ANY;
    int r = bind(m_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    if (r != 0) {
        fprintf(stderr, "Failed to bind socket to port %u: %s\n", m_port, strerror(errno));
        return false;
    }

    if (listen(m_socket, 5) != 0) {
        fprintf(stderr, "Could not listen on socket: %s\n", strerror(errno));
        return false;
    }

    return true;
}

void UpnpServer::run()
{
    while(true) {
        struct sockaddr_in src_addr;
        socklen_t addrlen = sizeof(src_addr);
        int client_socket = accept(m_socket, reinterpret_cast<struct sockaddr *>(&src_addr), &addrlen);
        if (client_socket < 0) {
            fprintf(stderr, "Could not accept connection: %s\n", strerror(errno));
            break;
        } else {
            pthread_t id;
            const char *remote_host = inet_ntoa(src_addr.sin_addr);
            if ((m_allowed_hosts.size() == 0) || (m_allowed_hosts.find(remote_host) != m_allowed_hosts.end())) {
                printf("UpnpServer: new connection from %s\n", remote_host);
                ClientData *client = new ClientData;
                client->socket = client_socket;
                client->server = this;
                if (pthread_create(&id, 0, client_thread, (void *)client) != 0) {
                    fprintf(stderr, "UpnpServer: failed to create client thread: %s\n", strerror(errno));
                    delete client;
                } else {
                    pthread_detach(id); // We don't join, so automatically free allocated resources.
                    printf("UpnpServer: thread with id %d created\n", (int)id);
                }
            } else {
                //printf("host '%s' not in list, dropping connection\n", remote_host);
                close(client_socket);
            }
        }
    }
}

void *UpnpServer::client_thread(void *arg)
{
    ClientData *client = reinterpret_cast<ClientData*>(arg);
    char buffer[1024];
    ssize_t nbytes;

    while ((nbytes = read(client->socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[nbytes] = '\0';
        if (!client->server->handle_message(buffer, client->socket)) {
            break;
        }
    }

    printf("UpnpServer: thread with socket %d has completed\n", client->socket);

    close(client->socket);
    delete client;
    return NULL;
}

bool UpnpServer::handle_message(char *buffer, int client_socket)
{
    char header[2048];

    printf("UpnpServer::handle_message: %s\n", buffer);

    if (strstr(buffer, "GET /dd.xml HTTP/1.1") != NULL) {
        sprintf(header, "HTTP/1.1 200 OK\r\n"
                        "Host: %s:%u\r\n"
                        "Content-Type: text/xml; charset=utf-8\r\n"
                        "Content-Length: %u\r\n"
                        "Connection: close\r\n"
                        "Application-URL: http://%s:8080/apps/\r\n"
                        "\r\n"
                        "%s", m_ip_addr.c_str(), m_dial_port, static_cast<uint32_t>(strlen(ddxml)), m_ip_addr.c_str(), ddxml);
    } else {
        printf("UpnpServer::handle_message: unknown request\n");
        strcpy(header, "HTTP/1.1 404 Not Found\r\n");
    }

    write(client_socket, header, strlen(header));

    return false; // close connection
}
