/// \file pc_reference.cpp
/// \brief PC client implementation using the CloudTV Nano SDK.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#define _CRT_SECURE_NO_WARNINGS

#include <core/IControl.h>
#include <core/IInput.h>
#include <core/Session.h>
#include <core/SessionStateObserver.h>
#include <core/Version.h>
#include <porting_layer/ClientContext.h>
#include <porting_layer/X11KeyMap.h>
#include <porting_layer/Keyboard.h>
#include <porting_layer/Log.h>
#include <utils/cJSON.h>
#include <utils/utils.h>
#include <stream/StreamForwarder.h>
#include <stream/IStreamPlayer.h>
#include <stream/SimpleMediaPlayer.h>
#include <stream/HttpLoader.h>
#include <stream/UdpLoader.h>

#include <string>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

using namespace ctvc;

#define UNUSED(x) (void)(x)

volatile bool must_shutdown = false;

class SessionCallbacks : public Session::ISessionCallbacks
{
public:
    void state_update(Session::State state, ClientErrorCode reason)
    {
        m_state_observer.state_update(state, reason);

        if (state != Session::STATE_ERROR && state != Session::STATE_DISCONNECTED) {
            return;
        }

        CTVC_LOG_DEBUG("reason:%d", reason);

        must_shutdown = true;
    }

    SessionStateObserver m_state_observer;
};

class StreamPlayer : public IStreamPlayer
{
public:
    ResultCode open(const std::string &url)
    {
        return m_stream_forwarder.open(url);
    }
    // Implements IStream
    void stream_data(const uint8_t *data, uint32_t length)
    {
        m_stream_forwarder.stream_data(data, length);
    }
    void stream_error(ResultCode result)
    {
        m_stream_forwarder.stream_error(result);
    }
    // Implements IStreamPLayer
    ResultCode start()
    {
        return ResultCode::SUCCESS;
    }
    void stop()
    {
    }

    StreamForwarder m_stream_forwarder;
};

static void handle_keys(Session &session)
{
    while (!must_shutdown) {
        int c = Keyboard::get_key();
        if (c == 0) {
            continue;
        }

        switch (c) {
        case 'q':
            return;
            break;
        case 0x13: // Ctrl+s
            printf("Suspend\n");
            session.get_control().suspend();
            break;
        case 0x12: // Ctrl+r
            printf("Resume\n");
            session.get_control().resume();
            break;
        default:
            printf("Sending %08X\n", c);
            bool client_must_handle_key_code;
            session.get_input().send_keycode(c, IInput::ACTION_DOWN_AND_UP, client_must_handle_key_code);
            if (client_must_handle_key_code) {
                CTVC_LOG_DEBUG("Client must handle the key");
            }
        }
    }
}

static void setup_keymap()
{
    X11KeyMap::KeyMap map[] = {
        { '0', X11_0 },
        { '1', X11_1 },
        { '2', X11_2 },
        { '3', X11_3 },
        { '4', X11_4 },
        { '5', X11_5 },
        { '6', X11_6 },
        { '7', X11_7 },
        { '8', X11_8 },
        { '9', X11_9 },
        { '0', X11_0 },
        { '#', X11_HASH },
        { '*', X11_ASTERISK },

        { Keyboard::ENTER_KEY, X11_OK },
        { '\n', X11_OK },

        { Keyboard::BACKSPACE_KEY, X11_BACK },
        { Keyboard::DEL_KEY, X11_BACK },

        { Keyboard::UP_KEY, X11_UP },
        { Keyboard::DOWN_KEY, X11_DOWN },
        { Keyboard::RIGHT_KEY, X11_RIGHT },
        { Keyboard::LEFT_KEY, X11_LEFT },

        { '%', X11_OEMA },
        { '^', X11_OEMB },
        { '&', X11_OEMC },
        { '(', X11_OEMD },

        { 'U', X11_PAGE_UP },
        { '!', X11_PAGE_DOWN },

        { '@', X11_VOL_DOWN },
        { 'V', X11_VOL_UP },

        { 'T', X11_CHANNEL_UP },
        { 'Y', X11_CHANNEL_DOWN },

        { 'I', X11_RED },
        { 'O', X11_GREEN },
        { 'P', X11_YELLOW },
        { 'S', X11_BLUE },

        { 'h', X11_HOME },
        { 'p', X11_PLAY },
        { 's', X11_STOP },
        { 'w', X11_PAUSE },
        { 'f', X11_FF },
        { 'r', X11_RW },
        { 'e', X11_SKIP },
        { '$', X11_REPLAY },
        { 'y', X11_PLAYPAUSE },
        { 'n', X11_NEXT },
        { 'm', X11_PREV },
        { 'v', X11_END },
        { 'l', X11_LIST },
        { 'z', X11_LAST },
        { 'g', X11_SETUP },
        { 'i', X11_EXIT },
        { 'o', X11_MENU },
        { 'j', X11_NETTV },
        { 'k', X11_TOP_MENU },
        { 'x', X11_PPV },
        { 'c', X11_DVR },
        { 'W', X11_LIVE },
        { 'E', X11_MEDIA },
        { 'R', X11_SETTINGS },
        { 'F', X11_INFO },
        { 'G', X11_HELP },
        { 'H', X11_RECORD },
        { 'J', X11_GUIDE },
        { 'K', X11_FAVORITES },
        { 'L', X11_DAY_UP },
        { 'Z', X11_DAY_DOWN },
        { 'X', X11_MUTE },
    };

    ClientContext::instance().get_keymap().add_mapping(map, sizeof(map) / sizeof(map[0]));
}

static void print_version()
{
    fprintf(stderr, "Core Version:%s\n", get_sdk_version().c_str());
}

static const char *read_json_string(cJSON *obj, const char *name)
{
    if (!obj) {
        return 0;
    }
    cJSON *item = cJSON_GetObjectItem(obj, name);
    if (!item) {
        return 0;
    }
    if (item->type != cJSON_String) {
        CTVC_LOG_WARNING("Non-string object %s in json file", name);
        return 0;
    }
    return item->valuestring;
}

static int client_configure(std::map<std::string, std::string> &optional_parameters/*out*/, StreamPlayer &stream_player, const char *json_config_file, std::string &session_url/*out*/, std::string &app_url/*out*/, unsigned int &width/*out*/, unsigned int &height/*out*/)
{
    bool is_file_given = (json_config_file && json_config_file[0] != '\0');

    if (!is_file_given) {
        json_config_file = "./config.json";
    }

    CTVC_LOG_DEBUG("Using the following config file:(%s)", json_config_file);

    // Load json file
    FILE *fp = fopen(json_config_file, "r");
    cJSON *json = NULL;

    if (fp != NULL) {
        char buf[10000];
        memset(buf, 0, sizeof(buf));

        size_t read_size = fread(buf, 1, sizeof(buf), fp);
        fclose(fp);

        if (read_size == 0) {
            CTVC_LOG_ERROR("No data in JSON file");
            return 1;
        }

        json = cJSON_Parse(buf);
        if (!json) {
            CTVC_LOG_ERROR("Parse error in JSON file");
            return 1;
        }
    } else {
        if (is_file_given) {
            CTVC_LOG_ERROR("Can't open JSON config file:(%s)", json_config_file);
            return 1;
        } else {
            CTVC_LOG_DEBUG("Can't open JSON config file:(%s)", json_config_file);
            return 0;
        }
    }

    const char *s = read_json_string(json, "base_store_path");
    if (s) {
#ifndef __WIN32__
        mkdir(s, 0755);
#else
        mkdir(s);
#endif

        ctvc::ClientContext::instance().set_base_store_path(s);

        CTVC_LOG_DEBUG("base_store_path: %s", s);
    }

    cJSON *rfbtv_obj = cJSON_GetObjectItem(json, "rfbtv");
    if (!rfbtv_obj) {
        CTVC_LOG_ERROR("No rfbtv element in json file");
        return 1;
    }

    s = read_json_string(rfbtv_obj, "resolution");
    if (s) {
        if (sscanf(s, "%ux%u", &width, &height) != 2) {
            CTVC_LOG_WARNING("Illegal rfbtv resolution in json file:%s", s);
        }
    } else {
        CTVC_LOG_WARNING("Missing rfbtv resolution in json file");
    }

    s = read_json_string(rfbtv_obj, "client_manufacturer");
    if (s) {
        ClientContext::instance().set_manufacturer(s);
    }

    s = read_json_string(rfbtv_obj, "client_model");
    if (s) {
        ClientContext::instance().set_device_type(s);
    }

    s = read_json_string(json, "mac_address");
    if (s) {
        ClientContext::instance().set_unique_id(s);
    } else {
        CTVC_LOG_WARNING("Missing mac_address in json file");
    }

    s = read_json_string(rfbtv_obj, "ca_path");
    if (s) {
        ctvc::ClientContext::instance().set_ca_path(s);
    }

    s = read_json_string(rfbtv_obj, "ca_client_path");
    if (s) {
        ctvc::ClientContext::instance().set_ca_client_path(s);
    }

    s = read_json_string(rfbtv_obj, "private_key_path");
    if (s) {
        ctvc::ClientContext::instance().set_private_key_path(s);
    }

    s = read_json_string(json, "session_manager_url");
    if (s) {
        session_url = s;
    } else {
        CTVC_LOG_WARNING("Missing session_manager_url in json file");
    }

    s = read_json_string(rfbtv_obj, "app_url");
    if (s) {
        app_url = s;
    }

    s = read_json_string(rfbtv_obj, "stream_forward_url");
    if (s) {
        ResultCode ret = stream_player.open(s);
        if (ret.is_error()) {
            CTVC_LOG_ERROR("Stream open fails: (%s)", ret.get_description());
        }
    } else {
        CTVC_LOG_WARNING("Missing stream_forward_url in json file");
    }

    cJSON *params_obj = cJSON_GetObjectItem(rfbtv_obj, "setup_params");
    if (params_obj) {
        int n_items = cJSON_GetArraySize(params_obj);
        for (int i = 0; i < n_items; i++) {
            cJSON *item = cJSON_GetArrayItem(params_obj, i);
            if (item) {
                if (item->type == cJSON_String) {
                    optional_parameters[item->string] = item->valuestring;
                } else {
                    CTVC_LOG_WARNING("Non-string object %s in json file", item->string);
                }
            }
        }
    } else {
        CTVC_LOG_WARNING("Missing setup_params in json file");
    }

    cJSON_Delete(json);

    return 0;
}

static void print_usage(const char *name)
{
    printf("usage: %s [<options>]\n", name);
    printf(" -f config_file : Read given JSON config file (default: ./config.json)\n");
    printf(" -r             : Reconnect after a session finished\n");
    printf(" -h             : Print this help\n");
    printf(" -v             : Print CloudTV Nano SDK version\n");
}


int run_session(Session &session, SessionCallbacks &callbacks, const std::string &session_url, const std::string &app_url, unsigned int width, unsigned int height, const std::map<std::string, std::string> &optional_parameters)
{
    CTVC_LOG_DEBUG("Starting session");

    callbacks.m_state_observer.set_states_to_wait_for(Session::STATE_CONNECTING, Session::STATE_DISCONNECTED | Session::STATE_ERROR);
    session.get_control().initiate(session_url, app_url, width, height, optional_parameters);
    if (!callbacks.m_state_observer.wait_for_states()) {
        CTVC_LOG_ERROR("Session initiate() failed");
    }

    // Handle keys from the command line
    handle_keys(session);

    session.get_control().terminate();

    CTVC_LOG_DEBUG("Session end");

    return 0;
}

int main(int argc, char *argv[])
{
    int arg_count = 1;
    std::string json_config_file;
    bool must_reconnect = false;

    // Set some working defaults
    std::string session_url = "rfbtv://127.0.0.1:8095";
    std::string app_url = "webkit:http://www.youtube.com/tv";
    unsigned int screen_width = 1280;
    unsigned int screen_height = 720;

    while (arg_count < argc) {
        if (strcmp("-v", argv[arg_count]) == 0) {
            print_version();
            return 0;
        } else if (strcmp("-h", argv[arg_count]) == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp("-r", argv[arg_count]) == 0) {
            must_reconnect = true;
        } else if (strcmp("-f", argv[arg_count]) == 0) {
            arg_count++;
            if (arg_count < argc) {
                json_config_file = argv[arg_count];
            } else {
                CTVC_LOG_ERROR("-f is missing the file name");
                return 1;
            }
        } else {
            CTVC_LOG_ERROR("Illegal argument:(%s)", argv[arg_count]);
            print_usage(argv[0]);
            return 0;
        }
        arg_count++;
    }

    CTVC_LOG_DEBUG("<<<<<<<<<<<<<<<<Starting>>>>>>>>>>>>>>>>>>>>>");

    setup_keymap();

    StreamPlayer stream_player;
    SessionCallbacks callbacks;
    Session session(ClientContext::instance(), &callbacks, 0);

    // Register default content loaders
    SimpleMediaPlayerFactory<HttpLoader> http_player_factory(stream_player);
    SimpleMediaPlayerFactory<UdpLoader> udp_player_factory(stream_player);
    session.register_media_player("http", http_player_factory);
    session.register_media_player("https", http_player_factory);
    session.register_media_player("udp", udp_player_factory);

    std::map<std::string, std::string> optional_parameters;

    // Configure the client
    if (client_configure(optional_parameters, stream_player, json_config_file.c_str(), session_url, app_url, screen_width, screen_height) > 0) {
        return 1;
    }

    while (true) {
        must_shutdown = false;

        int ret = run_session(session, callbacks, session_url, app_url, screen_width, screen_height, optional_parameters);
        if (ret) {
            return ret;
        }
        if (!must_reconnect) {
            break;
        }
    }

    CTVC_LOG_DEBUG("Exiting the client");

    session.unregister_media_player("http");
    session.unregister_media_player("https");
    session.unregister_media_player("udp");

    return 0;
}
