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
void Application::run(const std::string &server, const std::string &app_url)
{
    Session session(ClientContext::instance(), this, this);

    // DefaultContentLoader has to be started with the required number of
    // threads. Passing "0" as argument will disable any threading, i.e. the
    // content will be download in a synchronously
    m_default_content_loader.start(5);

    // Registering the content loader
    session.register_content_loader(&m_default_content_loader);

    std::map<std::string, std::string> optional_parameters;
    optional_parameters["lan"] = "eth10";

    m_state_observer.set_states_to_wait_for(Session::STATE_CONNECTING, Session::STATE_DISCONNECTED | Session::STATE_ERROR);
    session.get_control().initiate(server, app_url, 1280, 720, optional_parameters);
    if (!m_state_observer.wait_for_states()) {
        CTVC_LOG_ERROR("Session initiate() failed");
        return;
    }

    printf("we have a session!\n");

    while (session.get_state() == Session::STATE_CONNECTED || session.get_state() == Session::STATE_CONNECTING) {
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

    // Remove the IContentLoader from Session, so it won't be used
    session.register_content_loader(0);

    // Stopping all threads that were created in DefaultContentLoader::start().
    // This call blocks until all threads a stopped.
    m_default_content_loader.stop();
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
    }
}

void Application::overlay_blit_image(const PictureParameters &picture_params)
{
    printf("TODO: overlay_blit_image(x=%u, y=%u, w=%u, h=%u, alpha=%u, data size=%lu)\n",
            picture_params.x,
            picture_params.y,
            picture_params.w,
            picture_params.h,
            picture_params.alpha,
            static_cast<unsigned long int>(picture_params.m_data.size()));
}

void Application::overlay_clear()
{
    printf("TODO: overlay_clear()\n");
}

void Application::overlay_flip()
{
    printf("TODO: overlay_flip()\n");
}
