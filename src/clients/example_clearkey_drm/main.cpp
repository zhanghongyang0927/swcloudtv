///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "Application.h"

#include <porting_layer/ClientContext.h>
#include <porting_layer/X11KeyMap.h>
#include <porting_layer/Keyboard.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

using namespace ctvc;

const char *DEFAULT_SERVER_URL = "rfbtv://127.0.0.1:8095";
const char *DEFAULT_APP_URL = "webkit:http://youtube.com/tv";
const char *DEFAULT_FORWARD_URL = "udp://127.0.0.1:12345";
const char *DEFAULT_BASE_STORE_PATH = "/tmp";

void usage(const char *name)
{
    fprintf(stderr, "Usage: %s [options]\n", name);
    fprintf(stderr, "\nAvailable options:\n");
    fprintf(stderr, " -h                      Print this help.\n");
    fprintf(stderr, " -s <server URL>         Connect to the specified RFB-TV server.                default: '%s'\n", DEFAULT_SERVER_URL);
    fprintf(stderr, " -a <app URL>            Start the specified app on the server.                 default: '%s'\n", DEFAULT_APP_URL);
    fprintf(stderr, " -b <base store path>    Path to datastore files (i.e. cookie file).            default: '%s'\n", DEFAULT_BASE_STORE_PATH);
    fprintf(stderr, " -f <forward URL>        Forward the received stream to the specified address.  default: '%s'\n", DEFAULT_FORWARD_URL);
    fprintf(stderr, "\nExample: %s -s rfbtv://localhost -a webkit:http://activevideo.com -f udp://127.0.0.1:9999\n", name);
}

void init_keymap(X11KeyMap &keymap)
{
    keymap.add_mapping(Keyboard::ENTER_KEY, X11_OK);
    keymap.add_mapping(Keyboard::DEL_KEY, X11_BACK);
    keymap.add_mapping(Keyboard::UP_KEY, X11_UP);
    keymap.add_mapping(Keyboard::DOWN_KEY, X11_DOWN);
    keymap.add_mapping(Keyboard::RIGHT_KEY, X11_RIGHT);
    keymap.add_mapping(Keyboard::LEFT_KEY, X11_LEFT);
}

void setup_client_context(const std::string &base_store_path)
{
    ClientContext &client_context(ClientContext::instance());

    client_context.set_manufacturer("MyCompany");
    client_context.set_device_type("STB1234");
    client_context.set_unique_id("01:02:03:04:05:06");

    client_context.set_base_store_path(base_store_path.c_str());

    init_keymap(client_context.get_keymap());
}

int main(int argc, char *argv[])
{
    Application app;

    // Connect to server-simulator on localhost by default
    std::string server(DEFAULT_SERVER_URL);
    std::string app_url(DEFAULT_APP_URL);
    std::string base_store_path(DEFAULT_BASE_STORE_PATH);
    std::string forward_url(DEFAULT_FORWARD_URL);

    int opt;
    while ((opt = getopt(argc, argv, "hs:a:b:f:")) != -1) {
        switch (opt) {
        case 's':
            server = optarg;
            break;
        case 'a':
            app_url = optarg;
            break;
        case 'b':
            base_store_path = optarg;
            break;
        case 'f':
            forward_url = optarg;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        default: /* '?' */
            usage(argv[0]);
            return 1;
        }
    }

    if (optind != argc) {
        // Print usage and exit if we have any non-option arguments
        usage(argv[0]);
        return 1;
    }

    setup_client_context(base_store_path);

    app.run(server, app_url, forward_url);

    return 0;
}
