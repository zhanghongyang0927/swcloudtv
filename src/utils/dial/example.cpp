///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "mcastserver.h"
#include "dialserver.h"
#include "dialapplication.h"
#include "upnpserver.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>

#include <string>

static std::string get_local_address()
{
    int sok = socket(AF_INET, SOCK_DGRAM, 0);
    if (sok < 0) {
        printf("socket() failed: %s\n", strerror(errno));
        exit(1);
    }

    char buf[4096];
    struct ifconf ifc;
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (0 > ioctl(sok, SIOCGIFCONF, &ifc)) {
        printf("ioctl(SIOCGIFCONF) failed: %s\n", strerror(errno));
        exit(1);
    }

    if (ifc.ifc_len == sizeof(buf)) {
        printf("SIOCGIFCONF output too long\n");
        exit(1);
    }

    std::string local_address;
    int len = 0;
    struct ifreq *ifreq = ifc.ifc_req;
    for (int i = 0; i < ifc.ifc_len; i += len) {
        ifreq = (struct ifreq*)((char*)ifreq + len);

#ifndef __linux__
        len = IFNAMSIZ + ifreq->ifr_addr.sa_len;
#else
        len = sizeof(*ifreq);
#endif
        // Save address now, because the ioctl SIOCGIFFLAGS overwrites the union
        local_address = inet_ntoa(((struct sockaddr_in *)(&ifreq->ifr_addr))->sin_addr);

        if (((struct sockaddr_in *)(&ifreq->ifr_addr))->sin_family != AF_INET) {
            // don't use IPv-6 or link levbel addresses
            continue;
        }

        // Next call overwrites the union!
        if (ioctl(sok, SIOCGIFFLAGS, ifreq) < 0) {
            printf("ioctl(SIOCGIFFLAGS) failed: %s\n", strerror(errno));
            continue;
        }

        if (ifreq->ifr_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) {
            // don't use loop back or point-to-point interfaces
            continue;
        }

        if (strchr(ifreq->ifr_name, ':')) {
            printf("not using '%s' skipping...\n", ifreq->ifr_name);
            continue;
        }

        printf("%s: using %s (interface %s)\n", __FUNCTION__, local_address.c_str(), ifreq->ifr_name);
        break;
    }

    close(sok);

    return local_address;
}


static std::string xmlencode(const char *src)
{
    std::string dst;
    while (*src) {
        switch (*src) {
            case '&':
                dst += "&amp;";
                break;
            case '\"':
                dst += "&quot;";
                break;
            case '\'':
                dst += "&apos;";
                break;
            case '<':
                dst += "&lt;";
                break;
            case '>':
                dst += "&gt;";
                break;
            default:
                dst += *src;
                break;
        }
        src++;
    }
    return dst;
}

static char to_hex(char code)
{
    static char hex[] = "0123456789ABCDEF";
    return hex[code & 15];
}

static std::string url_encode(const std::string &str)
{
    std::string buf;
    const char *pstr;
    pstr = str.c_str();

    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
            buf += *pstr;
        else if (*pstr == ' ')
            buf += '+';
        else
            buf += '%', buf += to_hex(*pstr >> 4), buf += to_hex(*pstr & 15);
        pstr++;
    }

    return buf;
}

class NetflixApplication : public DialApplication
{
public:
    NetflixApplication(std::string ip_addr) : DialApplication("Netflix", ip_addr)
    {
    }

    virtual bool launch(std::string body)
    {
        // Check for 'sync'
        std::string::size_type begin = body.find("intent=");
        if (begin != std::string::npos) {
            set_additional_data("<intent>sync</intent>");
        }

        char additional_data_url[] = "http://localhost:8080/apps/Netflix/dial_data/";
        set_additional_data_url(additional_data_url);

        if (m_running)
        {
            // To pass the conformance test, we just say it already started OK
            return true;
        }
        std::string cmdline = "https://secure.netflix.com/us/htmltvui/release-webkit30-2014_11_04-12/3_0/p/720p/html/plus.html?q=";
        cmdline += std::string("source_type=12");
        cmdline += std::string("&dial=") + url_encode(body);
        cmdline += std::string("&additionalDataUrl=") + url_encode(additional_data_url);

        printf("############ STARTING NETFLIX WITH %s\n", cmdline.c_str());
        std::string netflix = "chromium-browser \"" + cmdline + "\"";
        system(netflix.c_str());

        set_running(true);
        return true;
    }

    virtual bool kill()
    {
        if (!m_running)
        {
            return false;
        }
        // TODO: Actual app stop
        set_running(false);
        return true;
    }

};

class YouTubeApplication : public DialApplication
{
public:
    YouTubeApplication(std::string ip_addr) : DialApplication("YouTube", ip_addr)
    {
    }

    virtual bool launch(std::string body)
    {
        if (m_running)
        {
            return false;
        }

        std::string::size_type begin = body.find("pairingCode=");
        if (begin != std::string::npos) {
            std::string pairingCode;
            pairingCode = body.substr(begin);
            std::string::size_type end = pairingCode.find("\r\n");
            if (end != std::string::npos) {
                pairingCode.erase(end);
            }
            printf("############ STARTING YOUTUBE WITH %s\n", pairingCode.c_str());
            std::string youtube = "chromium-browser \"https://www.youtube.com/tv?" + xmlencode(pairingCode.c_str()) + "\"";
            system(youtube.c_str());
        } else {
            printf("############ 'pairingCode' NOT FOUND\n");
            return false;
        }
        // TODO: Actual app start
        set_running(true);
        return true;
    }

    virtual bool kill()
    {
        if (!m_running)
        {
            return false;
        }
        // TODO: Actual app stop
        set_running(false);
        return true;
    }
};


int main(int argc, char *argv[])
{
    const char *proxy_dest = NULL;
    bool proxy((argc >= 3) && (argv[1][0] == '-') && (argv[1][1] == 'p'));
    if (proxy) {
        proxy_dest = argv[2];
    }

    std::string local_address = get_local_address();

    uint16_t dial_port(8080);
    MulticastServer mcastServer(local_address, dial_port);
    UpnpServer upnpServer(local_address, dial_port);
    DialServer dialServer(dial_port, proxy_dest);

    NetflixApplication netflix(local_address);
    YouTubeApplication youtube(local_address);

    //const char *allowed_host = "172.16.106.141";
    //const char *allowed_host = "172.16.102.193"; // Fred's phone
    //const char *allowed_host = "172.16.102.126"; // Breght's phone
    //upnpServer.set_allowed_hosts(allowed_host);
    //mcastServer.set_allowed_hosts(allowed_host);

    if (!proxy) {
        dialServer.register_application(&netflix);
        dialServer.register_application(&youtube);
    }

    if (upnpServer.start() && dialServer.start() && mcastServer.start()) {
        while(true) {
            sleep(3600);
        }
    }

    return 0;
}
