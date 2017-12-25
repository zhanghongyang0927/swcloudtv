///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "dialserver.h"
#include "dialapplication.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>

static const int MAX_PENDING_LISTENS = 10;  // Allow 10 connect requests in backlog

static std::string get_header(const std::string &buffer, const std::string &name)
{
    std::string header;

    std::string::size_type begin = buffer.find(name);
    if (begin != std::string::npos) {
        begin += name.size();
        std::string::size_type end = buffer.find("\r\n", begin);
        if (end != std::string::npos) {
            header = buffer.substr(begin, end - begin);
        }
    }

    return header;
}

static std::string get_origin(const std::string &buffer)
{
    return get_header(buffer, "Origin: ");
}

DialServer::DialServer(uint16_t port, const char *proxy_dest) : SocketServer(port)
{
    if (proxy_dest) m_proxy_dest = proxy_dest;
}

bool DialServer::open_socket()
{
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == -1) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    int r = bind(m_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    if (r != 0) {
        fprintf(stderr, "Failed to bind socket to port %u: %s\n", m_port, strerror(errno));
        return false;
    }

    if (listen(m_socket, MAX_PENDING_LISTENS) != 0) {
        fprintf(stderr, "Could not listen on socket: %s\n", strerror(errno));
        return false;
    }

    return true;
}

struct client_data
{
    int m_socket;
    DialServer *m_this;
};

void DialServer::run()
{
    while(true) {
        struct sockaddr_in src_addr;
        socklen_t addrlen = sizeof(src_addr);
        int client_socket = accept(m_socket, reinterpret_cast<struct sockaddr *>(&src_addr), &addrlen);
        if (client_socket < 0) {
            fprintf(stderr, "Could not accept connection: %s\n", strerror(errno));
            break;
        } else {
            printf("received request from '%s'\n", inet_ntoa(src_addr.sin_addr));
            pthread_t id;

            client_data *ctx = new client_data;
            ctx->m_socket = client_socket;
            ctx->m_this = this;
            if (pthread_create(&id, 0, client_thread, (void *)ctx) != 0) {
                fprintf(stderr, "failed to create client thread: %s\n", strerror(errno));
                delete ctx;
            } else {
                pthread_detach(id); // We don't join, so automatically free allocated resources.
                printf("dial server thread with id %d created\n", (int)id);
            }
        }
    }
}

static const uint32_t MAX_POSTDATA_LEN = 4096;

void *DialServer::client_thread(void *arg)
{
    client_data *ctx = (client_data *) arg;
    char buffer[MAX_POSTDATA_LEN + 1024];
    int32_t nbytes;
    int client_socket = ctx->m_socket;

    while ((nbytes = read(client_socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[nbytes] = '\0';
        if (ctx->m_this->m_proxy_dest.length() > 0) {
            printf("@@@@@@ DialServer received %d bytes: '%s'\nPass it to remote (proxy)\n", nbytes, buffer);
            if (!ctx->m_this->proxy_message(buffer, client_socket)) {
                break;
            }
        } else {
            printf("@@@@@@ DialServer received %d bytes: '%s'\n", nbytes, buffer);
            if (!ctx->m_this->handle_message(buffer, client_socket)) {
                break;
            }
        }
    }

    close(client_socket);
    delete ctx;
    return NULL;
}

// Quick and dirty mechanism to pass DIAL messages to a remote (Set-Top Box)
bool DialServer::proxy_message(char *buffer, int client_socket)
{
    int proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (proxy_socket == -1) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return false;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, m_proxy_dest.c_str(), &addr.sin_addr.s_addr);

    if (connect(proxy_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to connect to '%s' : %s\n", m_proxy_dest.c_str(), strerror(errno));
        return false;
    }

    if (send(proxy_socket, buffer, strlen(buffer), 0) < 0) {
        fprintf(stderr, "Failed to proxy data to '%s' : %s\n", m_proxy_dest.c_str(), strerror(errno));
        close(proxy_socket);
        return false;
    }

    // Get reply from proxy (quick and dirty, assume we get the complete message at once).
    int32_t nbytes = read(proxy_socket, buffer, MAX_POSTDATA_LEN);
    if (nbytes > 0) {
        std::string reply(buffer, nbytes);
        printf("Received %d bytes in reply:\n%s====== end of reply ======\n", nbytes, reply.c_str());
        send(client_socket, buffer, nbytes, 0);
    } else {
        fprintf(stderr, "Failed to receive reply from '%s' : %s\n", m_proxy_dest.c_str(), strerror(errno));
    }

    // Say goodbye to remote
    close(proxy_socket);

    return false;
}

static const char service_data[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
    "<service xmlns=\"urn:dial-multiscreen-org:schemas:dial\" dialVer=\"1.7\">\r\n"
    "  <name>%s</name>\r\n"
    "  <options allowStop=\"true\"/>\r\n"
    "  <state>%s</state>\r\n"
    "  %s"
    "  <additionalData>%s</additionalData>\r\n"
    "</service>\r\n";

bool DialServer::handle_message(char *buffer, int client_socket)
{
    char response[2048];

    char *body = strstr(buffer, "\r\n\r\n");
    if (body == NULL) {
        strcpy(response, "400 Bad Request");
        printf("@@@ SEND RESPONSE: %s\n", response);
        write(client_socket, response, strlen(response));
        return false; // close connection
    }
    body += 4; // Skip CR/LF pair

    uint32_t content_length = 0;
    uint32_t body_length = strlen(body);
    const char *content_length_header = strstr(buffer, "Content-Length:");
    if (content_length_header != NULL) {
        if (sscanf(content_length_header, "Content-Length: %u", &content_length) == 1) {

            if (content_length > MAX_POSTDATA_LEN) {
                strcpy(response, "HTTP/1.1 413 Request Entity Too Large\r\n");
                printf("@@@ SEND RESPONSE: %s\n", response);
                write(client_socket, response, strlen(response));
                return false; // close connection
            }

            const char *continuation = strstr(buffer, "Expect: 100-continue");
            if (continuation != NULL) {
                strcpy(response, "HTTP/1.1 100 Continue\r\n");
                printf("@@@ SEND RESPONSE: %s\n", response);
                write(client_socket, response, strlen(response));
            }

            if (content_length != body_length) {
                printf("Getting body of length %u\n", content_length);
                if (read(client_socket, &body[body_length], content_length - body_length) <= 0) {
                    return false;
                }
            }
        }
    }

    if ( (strstr(buffer, "POST /mdx/") != NULL) ||
         (strstr(buffer, "POST /pairingrequest ") != NULL) ) {
        const char *reply = "HTTP/1.1 200 OK\r\n"
                            "Access-Control-Allow-Origin: *\r\n"
                            "Content-Type: application/json; charset=\"utf-8\"\r\n"
                            "Connection: close\r\n"
                            "Content-Length: 9\r\n"
                            "\r\n"
                            "status=ok";
        write(client_socket, reply, strlen(reply));
        return false; // close connection
    }

    ApplicationList::iterator it;
    for (it = m_applications.begin(); it != m_applications.end(); it++) {
        DialApplication *app = (*it).second;
        std::string match1 = "/apps/" + app->get_name() + " ";
        std::string match2 = "/apps/" + app->get_name() + "/run ";

        if ((strstr(buffer, match1.c_str()) != NULL) || (strstr(buffer, match2.c_str()) != NULL)) {

            std::string origin_header = get_origin(buffer);

            if (memcmp(buffer, "GET", 3) == 0) {
                printf("SENDING %s status\n", app->get_name().c_str());

                char content[1024];
                sprintf(content, service_data, app->get_name().c_str(),
                        app->get_status(),
                        app->is_running() ? "<link rel=\"run\" href=\"run\"/>\r\n" : "",
                        app->additional_data().c_str());

                sprintf(response, "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/xml; charset=utf-8\r\n"
                                  "Content-Length: %u\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "%s", static_cast<uint32_t>(strlen(content)), content);

            } else if (memcmp(buffer, "POST", 4) == 0) {
                printf("STARTING %s\n", app->get_name().c_str());
                std::string loc = "http://" + app->ip_addr() + ":8080/apps/" + app->get_name() + "/run";
                if (app->launch(body)) {
                    sprintf(response, "HTTP/1.1 201 Created\r\n"
                                      "Access-Control-Allow-Origin: %s\r\n"
                                      "Content-Type: text/plain; charset=\"utf-8\"\r\n"
                                      "Content-Length: %u\r\n"
                                      "Connection: close\r\n"
                                      "Location: %s\r\n"
                                      "\r\n"
                                      "%s", origin_header.c_str(), static_cast<uint32_t>(loc.size()), loc.c_str(), loc.c_str());
                } else {
                    printf("%s already running\n", app->get_name().c_str());
                    strcpy(response, "HTTP/1.1 503 Service Unavailable\r\n");
                }

            } else if (memcmp(buffer, "DELETE", 6) == 0) {
                if (!app->kill()) {
                    printf("%s not running\n", app->get_name().c_str());
                    strcpy(response, "HTTP/1.1 404 Not Found\r\n");
                } else {
                    printf("%s stopped\n", app->get_name().c_str());
                    sprintf(response, "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/plain; charset=\"utf-8\"\r\n"
                                      "Content-Length: 0\r\n"
                                      "Connection: close\r\n"
                                      "\r\n");
                }
            } else {
                printf("UNKNOWN COMMAND: %s\n", buffer);
                strcpy(response, "HTTP/1.1 404 Not Found\r\n");
            }

            break;  // Found the app, break loop
        }
    }

    if (it == m_applications.end()) {
        printf("UNKNOWN APP: %s\n", buffer);
        strcpy(response, "HTTP/1.1 404 Not Found\r\n");
    }

    printf("@@@ SEND RESPONSE: %s\n", response);
    write(client_socket, response, strlen(response));

    return false; // close connection
}

void DialServer::register_application(DialApplication *app)
{
    m_applications[app->get_name()] = app;
}
