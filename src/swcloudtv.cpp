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
#include <sys/un.h>
#include <sys/socket.h>

#include "swapi.h"
#include "swcloudtv.h"
#include "swcloudtv_priv.h"
#define CONFIG_FILE "/system/etc/cloudtv.config"
#define ClOUDTV_SOCKET_FILE "/tmp/.cloudtv"

using namespace ctvc;

#define UNUSED(x) (void)(x)

volatile bool must_shutdown = false;
static int m_key_msg = 0;
static int m_key_socket = 0;

class SessionCallbacks : public Session::ISessionCallbacks
{
public:
    void state_update(Session::State state, ClientErrorCode reason)
    {
        m_state_observer.state_update(state, reason);

        if (state != Session::STATE_ERROR && state != Session::STATE_DISCONNECTED) {
            return;
        }

        CLOUDTV_LOG_DEBUG("reason:%d", reason);

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

static int init_recv_socket(char * path)
{
	int sockfd,len;
	struct sockaddr_un addr;
	sockfd=socket(AF_UNIX,SOCK_DGRAM,0);

	if(sockfd<0)
	{
		CLOUDTV_LOG_DEBUG("create unix socket failed!\n");
		return -1;
	} 
	sw_memset(&addr,sizeof(struct sockaddr_un), 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	sw_strlcpy(addr.sun_path, sizeof(addr.sun_path), path, sizeof(addr.sun_path));
	unlink(path);
	len = strlen(addr.sun_path) + sizeof(addr.sun_family);

	if(bind(sockfd,(struct sockaddr *)&addr,len)<0)
	{
		CLOUDTV_LOG_DEBUG("bind unix socket failed!\n");
		close(sockfd);
		return -1;
	}
	return sockfd;
}

static void handle_keys(Session &session)
{
	if(m_key_socket <= 0)
	{
		CLOUDTV_LOG_DEBUG("start reec key socket.\n");
		m_key_socket = init_recv_socket(ClOUDTV_SOCKET_FILE);
	}
	if(m_key_socket > 0)
	{
		while(!must_shutdown)
		{
			if (recvfrom(m_key_socket, &m_key_msg, sizeof(m_key_msg), 0, NULL, NULL) < 0)
				CLOUDTV_LOG_DEBUG("recv msg failed!\n");

			CLOUDTV_LOG_DEBUG("recv key is:%d.\n", m_key_msg);
            bool client_must_handle_key_code;
            session.get_input().send_keycode(m_key_msg, IInput::ACTION_DOWN_AND_UP, client_must_handle_key_code);
		}
	}
	else
		CLOUDTV_LOG_DEBUG("\n");
}

typedef struct sw_key_table
{
	int index;
	uint32_t phycode;
    X11KeyCode x11_code;
}cloudtv_key_map_t;

cloudtv_key_map_t m_key_map[] =
{
	{ 0,  0x7,   X11_0      },
	{ 1,  0x8,   X11_1      },
	{ 2,  0x9,   X11_2      },
	{ 3,  0xa,   X11_3      },
	{ 4,  0xb,   X11_4      },
	{ 5,  0xc,   X11_5      },
	{ 6,  0xd,   X11_6      },
	{ 7,  0xe,   X11_7      },
	{ 8,  0xf,   X11_8      },
	{ 9,  0x10,  X11_9      },
	{ 10, 0x15,  X11_LEFT   },
	{ 11, 0x16,  X11_RIGHT  },
	{ 12, 0x13,  X11_UP     },
	{ 13, 0x14,  X11_DOWN   },
	{ 14, 0x17,  X11_OK     },
	{ 15, 0x52,  X11_HOME   },
	{ 16, 0x4,   X11_BACK   },
};

static void init_keymap(X11KeyMap &keymap)
{
	int i = 0;
	for(i=0;i<sizeof(m_key_map)/sizeof(m_key_map[0]);i++)
    	keymap.add_mapping(m_key_map[i].phycode, m_key_map[i].x11_code);
}

static void setup_keymap()
{
    ClientContext &client_context(ClientContext::instance());
    init_keymap(client_context.get_keymap());
    client_context.set_base_store_path("/tmp");
}

static void print_version()
{
    CLOUDTV_LOG_DEBUG( "Core Version:%s\n", get_sdk_version().c_str());
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
		CLOUDTV_LOG_DEBUG( "config file isn't given!\n");
        json_config_file = CONFIG_FILE;
    }

    CLOUDTV_LOG_DEBUG( "Using the following config file:(%s)", json_config_file);

    // Load json file
    FILE *fp = fopen(json_config_file, "r");
    cJSON *json = NULL;

    if (fp != NULL) {
		CLOUDTV_LOG_DEBUG( "open config file success.\n");
        char buf[10000];
        memset(buf, 0, sizeof(buf));

        size_t read_size = fread(buf, 1, sizeof(buf), fp);
        fclose(fp);

        if (read_size == 0) {
			CLOUDTV_LOG_DEBUG( "No data in JSON file\n");
            return 1;
        }

        json = cJSON_Parse(buf);
        if (!json) {
            CLOUDTV_LOG_DEBUG( "Parse error in JSON file\n");
            return 1;
        }
    } else {
        if (is_file_given) {
            CTVC_LOG_ERROR("Can't open JSON config file:(%s)", json_config_file);
            return 1;
        } else {
            CLOUDTV_LOG_DEBUG("Can't open JSON config file:(%s)", json_config_file);
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

        CLOUDTV_LOG_DEBUG("base_store_path: %s", s);
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

void sw_cloudtv_send_key(int phycode)
{

}

int run_session(Session &session, SessionCallbacks &callbacks, const std::string &session_url, const std::string &app_url, unsigned int width, unsigned int height, const std::map<std::string, std::string> &optional_parameters)
{
    CLOUDTV_LOG_DEBUG( "Starting session\n");

    callbacks.m_state_observer.set_states_to_wait_for(Session::STATE_CONNECTING, Session::STATE_DISCONNECTED | Session::STATE_ERROR);
    session.get_control().initiate(session_url, app_url, width, height, optional_parameters);
    if (!callbacks.m_state_observer.wait_for_states()) {
        CLOUDTV_LOG_DEBUG( "Session initiate() failed.\n");
    }
	else
		CLOUDTV_LOG_DEBUG( "Session create success.\n");
	callbacks.state_update(session.STATE_CONNECTED, CLIENT_ERROR_CODE_OK);
	

    // Handle keys from the command line
    handle_keys(session);

	CLOUDTV_LOG_DEBUG( "begin control terminate.\n");

    session.get_control().terminate();

    CLOUDTV_LOG_DEBUG( "Session end\n");

    return 0;
}
static int m_recount = 0;
int sw_cloudtv_init()
{
	CLOUDTV_LOG_DEBUG( "\n");
    std::string json_config_file;
    bool must_reconnect = false;

    // Set some working defaults
    std::string session_url = "rfbtv://10.10.18.124:8095";
    std::string app_url = "webkit:http://www.youtube.com/tv";
    unsigned int screen_width = 1280;
    unsigned int screen_height = 720;
	print_version();
    CLOUDTV_LOG_DEBUG("<<<<<<<<<<<<<<<<Starting>>>>>>>>>>>>>>>>>>>>>");

	CLOUDTV_LOG_DEBUG( "setup_keymap.\n");
    setup_keymap();

    StreamPlayer stream_player;
    SessionCallbacks callbacks;
	CLOUDTV_LOG_DEBUG( "set callbacks.\n");
    Session session(ClientContext::instance(), &callbacks, 0);

    // Register default content loaders
    SimpleMediaPlayerFactory<HttpLoader> http_player_factory(stream_player);
    SimpleMediaPlayerFactory<UdpLoader> udp_player_factory(stream_player);
    session.register_media_player("http", http_player_factory);
    session.register_media_player("https", http_player_factory);
    session.register_media_player("udp", udp_player_factory);

    std::map<std::string, std::string> optional_parameters;
	
	CLOUDTV_LOG_DEBUG( "start client Configure.\n");

    // Configure the client
    if (client_configure(optional_parameters, stream_player, json_config_file.c_str(), session_url, app_url, screen_width, screen_height) > 0) {
		CLOUDTV_LOG_DEBUG( "get config json file failed.\n");
        return 1;
    }

    while (true) {
        must_shutdown = false;

		CLOUDTV_LOG_DEBUG( "session_url:%s, app_url:%s, width:%u, height:%u.\n", session_url.c_str(), app_url.c_str(), screen_width, screen_height);
        int ret = run_session(session, callbacks, session_url, app_url, screen_width, screen_height, optional_parameters);
		CLOUDTV_LOG_DEBUG( "ret session %d.\n",ret);
        if (ret) {
			CLOUDTV_LOG_DEBUG( "create session:%d.\n", ret);
            return ret;
        }
        if (!must_reconnect) {
			CLOUDTV_LOG_DEBUG( "No need Reconnect.\n");
            break;
        }
		else
		{
			if(m_recount < 20)
			{
				m_recount++;
				CLOUDTV_LOG_DEBUG( "start %d reconnect.\n", m_recount);
				continue;
			}
		}
    }

    CLOUDTV_LOG_DEBUG("Exiting the client\n");

    session.unregister_media_player("http");
    session.unregister_media_player("https");
    session.unregister_media_player("udp");

    return 0;
}
