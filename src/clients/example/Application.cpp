///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "Application.h"
#include "MediaChunkAllocator.h"

#include <core/IControl.h>
#include <core/IInput.h>
#include <stream/SimpleMediaPlayer.h>
#include <stream/HttpLoader.h>
#include <porting_layer/ClientContext.h>
#include <porting_layer/Keyboard.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace ctvc;

Application::Application()
{
}

Application::~Application()
{
}

///! \brief Just an example implementation.
void Application::run(const std::string &server, const std::string &app_url, const std::string &forward_url)
{
    Session session(ClientContext::instance(), this, &m_overlay_callbacks);

    MediaChunkAllocator allocator;
    session.register_media_chunk_allocator(&allocator);

    static SimpleMediaPlayerFactory<HttpLoader> http_media_player_factory(m_stream_player);
    session.register_media_player("http", http_media_player_factory);
    session.register_media_player("https", http_media_player_factory);

    session.register_protocol_extension(m_my_protocol_extension);

    std::map<std::string, std::string> optional_parameters;
    optional_parameters["lan"] = "eth10";

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
            printf("client terminates session\n");
            session.get_control().terminate();
            break;
        }
        if (key) {
            bool client_must_handle_key_code;
            session.get_input().send_keycode(key, IInput::ACTION_DOWN_AND_UP, client_must_handle_key_code);
            if (client_must_handle_key_code) {
                printf("client must handle the key\n");
            }
        }
    }

    printf("session closed\n");

    session.unregister_media_player("http");
    session.unregister_media_player("https");

    session.register_media_chunk_allocator(0);
}

void Application::state_update(Session::State state, ClientErrorCode reason)
{
    m_state_observer.state_update(state, reason);

    if (state != Session::STATE_ERROR && state != Session::STATE_DISCONNECTED) {
        return;
    }

    if (reason != CLIENT_ERROR_CODE_OK_AND_DO_NOT_RETUNE) {
        printf("TODO: Retune back to original program\n");
    }

    if (state == Session::STATE_ERROR) {
        printf("#####################################################################\n");
        printf("TODO: show message in on-screen dialog to end-user, code:%d\n", reason);
        printf("      PRESS OK TO CONTINUE\n");
        printf("#####################################################################\n");
        char c;
        if (read(0, &c, sizeof(c))) {
        }
    }
}

void Application::OverlayCallbacks::overlay_blit_image(const PictureParameters &/*picture_params*/)
{
    printf("TODO: OverlayCallbacks::overlay_blit_image()\n");
}

void Application::OverlayCallbacks::overlay_clear()
{
    printf("TODO: OverlayCallbacks::overlay_clear()\n");
}

void Application::OverlayCallbacks::overlay_flip()
{
    printf("TODO: OverlayCallbacks::overlay_flip()\n");
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
    printf("TODO: StreamPlayer::start()\n");

    if (!m_forward_url.empty()) {
        return m_forwarder.open(m_forward_url.c_str());
    }

    return ResultCode::SUCCESS;
}

void Application::StreamPlayer::stop()
{
    printf("TODO: StreamPlayer::stop()\n");
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
