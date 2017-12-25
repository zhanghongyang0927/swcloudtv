///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "Application.h"

#include <core/IControl.h>
#include <core/IInput.h>
#include <stream/SimpleMediaPlayer.h>
#include <stream/HttpLoader.h>
#include <porting_layer/ClientContext.h>
#include <porting_layer/Keyboard.h>

#include <stdlib.h>
#include <unistd.h>

using namespace ctvc;

Application::Application()
{
}

Application::~Application()
{
}

void Application::run(const std::string &server, const std::string &app_url, const std::string &forward_url)
{
    Session session(ClientContext::instance(), &m_state_observer, 0);

    session.register_drm_system(m_clear_key_drm_system);

    static SimpleMediaPlayerFactory<HttpLoader> http_media_player_factory(m_stream_player);
    session.register_media_player("http", http_media_player_factory);
    session.register_media_player("https", http_media_player_factory);

    std::map<std::string, std::string> optional_parameters;

    m_stream_player.set_forward_url(forward_url);

    m_state_observer.set_states_to_wait_for(Session::STATE_CONNECTING, Session::STATE_DISCONNECTED | Session::STATE_ERROR);
    session.get_control().initiate(server, app_url, 1280, 720, optional_parameters);
    if (!m_state_observer.wait_for_states()) {
        CTVC_LOG_ERROR("Session initiate() failed");
    }

    while (true) {
        if (session.get_state() != Session::STATE_CONNECTING && session.get_state() != Session::STATE_CONNECTED) {
            break; // session closed or error: break loop
        }
        // handle key presses (the simple way)
        int key = Keyboard::get_key();
        if (key == 'q' || key == EOF) {
            CTVC_LOG_INFO("client terminates session");
            session.get_control().terminate();
            break;
        }
        if (key) {
            bool client_must_handle_key_code;
            session.get_input().send_keycode(key, IInput::ACTION_DOWN_AND_UP, client_must_handle_key_code);
            if (client_must_handle_key_code) {
                CTVC_LOG_INFO("client must handle the key");
            }
        }
    }

    CTVC_LOG_INFO("session closed");

    session.unregister_drm_system(m_clear_key_drm_system);

    session.unregister_media_player("http");
    session.unregister_media_player("https");
}

Application::StreamPlayer::StreamPlayer()
{
}

Application::StreamPlayer::~StreamPlayer()
{
}

void Application::StreamPlayer::set_forward_url(const std::string &forward_url)
{
    m_forward_url = forward_url;
}

ResultCode Application::StreamPlayer::start()
{
    if (!m_forward_url.empty()) {
        return m_forwarder.open(m_forward_url.c_str());
    }

    return ResultCode::SUCCESS;
}

void Application::StreamPlayer::stop()
{
    m_forwarder.close();
}

void Application::StreamPlayer::stream_data(const uint8_t *data, uint32_t length)
{
    m_forwarder.stream_data(data, length);
}

void Application::StreamPlayer::stream_error(ResultCode error_code)
{
    m_forwarder.stream_error(error_code);
}
