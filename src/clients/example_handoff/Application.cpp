///
/// \brief This example client shows how an RFB-TV session handoff handler
/// could be implemented.
///
/// This client does not implement the client-side overlay callbacks. For an
/// example of that, please refer to the other example client.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "Application.h"

#include <core/IControl.h>
#include <core/IInput.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/Thread.h>
#include <porting_layer/ClientContext.h>
#include <porting_layer/Keyboard.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace ctvc;

Application::Application() :
    m_current_remote_session(0),
    m_stream_player(),
    m_http_media_player_factory(m_stream_player),
    m_session_cleanup(0),
    m_session_cleanup_thread(0)
{
}

Application::~Application()
{
    if (m_session_cleanup_thread) {
        m_session_cleanup_thread->stop_and_wait_until_stopped();
        delete m_session_cleanup_thread;
    }
    if (m_session_cleanup) {
        delete m_session_cleanup;
    }
}

Application::RemoteSession::RemoteSession(Application &application, IOverlayCallbacks *overlay_callbacks) :
    m_application(application),
    m_session(new Session(ClientContext::instance(), this, overlay_callbacks)),
    m_in_handoff(false)
{
}

Application::RemoteSession::~RemoteSession()
{
    delete m_session;
}

Session *Application::RemoteSession::session() const
{
    return m_session;
}

void Application::RemoteSession::set_handoff_in_progress(bool handoff)
{
    m_in_handoff = handoff;
}

void Application::RemoteSession::state_update(Session::State state, ClientErrorCode error_code)
{
    m_state_observer.state_update(state, error_code);

    if (state != Session::STATE_ERROR && state != Session::STATE_DISCONNECTED) {
        return;
    }

    if (state == Session::STATE_ERROR && !m_in_handoff) {
        // Propagate to application
        m_application.session_error(error_code);
    }
}

bool Application::RemoteSession::initiate(const std::string &server, const std::string &app_url)
{
    std::map<std::string, std::string> optional_parameters;
    optional_parameters["lan"] = "eth10";

    CTVC_LOG_INFO("Handoff the session to server '%s'", server.c_str());
    m_state_observer.set_states_to_wait_for(Session::STATE_CONNECTED, Session::STATE_DISCONNECTED | Session::STATE_ERROR);
    session()->get_control().initiate(server, app_url, 1280, 720, optional_parameters);
    if (!m_state_observer.wait_for_states()) {
        CTVC_LOG_ERROR("Session initiate() failed");
        return false;
    }
    return true;
}

Application::RemoteSession *Application::create_remote_session()
{
    Application::RemoteSession *remote_session = new Application::RemoteSession(*this, 0);
    remote_session->session()->register_media_player("http", m_http_media_player_factory);
    remote_session->session()->register_media_player("https", m_http_media_player_factory);

    // We register for 'rfbtv' and 'rfbtvs' schemes, but in a similar way you could register for 'vod' handoffs.
    // The difference is in the handling of suspend/resume (and not having to create a completely new session).
    remote_session->session()->register_handoff_handler("rfbtv", *this);
    remote_session->session()->register_handoff_handler("rfbtvs", *this);

    // A typical client would also support another handoff scheme
    remote_session->session()->register_handoff_handler("http", *this);

    return remote_session;
}

void Application::set_current_remote_session(RemoteSession *remote_session)
{
    AutoLock lck(m_session_mutex);
    if (m_current_remote_session) {
        m_current_remote_session->session()->unregister_media_player("http");
        m_current_remote_session->session()->unregister_media_player("https");
        // No need to call 'unregister_handoff_handler()', because we can safely assume there won't be a new request.
        CTVC_LOG_INFO("scheduling previous current session for deletion");
        delete_later(m_current_remote_session);
    }
    m_current_remote_session = remote_session;
}

Session *Application::current_session() const
{
    AutoLock lck(m_session_mutex);
    return m_current_remote_session->session();
}

IHandoffHandler::HandoffResult Application::handoff_request(const std::string &scheme, const std::string &uri, bool /*resume_session_when_done*/)
{
    if ((scheme.compare("rfbtv") == 0) || (scheme.compare("rfbtvs") == 0)) {
        return handoff_request_rfbtv(scheme, uri);
    }

    return IHandoffHandler::HANDOFF_SUCCESS; // Simulate that handoff to other URI schemes went just fine (for component test)
}

IHandoffHandler::HandoffResult Application::handoff_request_rfbtv(const std::string &scheme, const std::string &uri)
{
    std::string server = uri;
    std::string app_url = "";
    std::string::size_type pos = uri.find("?url=");
    if (pos != std::string::npos) {
        server = uri.substr(0, pos);
        app_url = uri.substr(pos + 5); // skip '?url='
    }

    if (server.size() == 0) {
        CTVC_LOG_WARNING("Unable to handle handoff request due to invalid parameters");
        return IHandoffHandler::HANDOFF_ERRONEOUS_REQUEST;
    }

    Application::RemoteSession *new_remote_session = create_remote_session();
    if (!new_remote_session) {
        CTVC_LOG_WARNING("Unable to handle handoff request because a new session object could not be created");
        return IHandoffHandler::HANDOFF_UNSPECIFIED_ERROR;
    }

    // If the setup fails for the new handoff session, we get a callback but
    // we don't want to present the 'session closed' error to the end-user.
    new_remote_session->set_handoff_in_progress(true);

    std::string server_uri = scheme + ":" + server;
    CTVC_LOG_INFO("Handoff the session to server '%s'", server_uri.c_str());
    if (!new_remote_session->initiate(server_uri, app_url)) {
        delete new_remote_session;
        return IHandoffHandler::HANDOFF_UNSPECIFIED_ERROR;
    }

    CTVC_LOG_INFO("Successfully initiated a session handoff to server '%s'", server_uri.c_str());

    // Returning 'HANDOFF_SUCCESS' will terminate the current (originating) session, but
    // we obviously don't want to present the 'session closed' error to the end-user.
    m_current_remote_session->set_handoff_in_progress(true);

    // Switch to the new session
    set_current_remote_session(new_remote_session);
    // From now on we do want to get notified.
    new_remote_session->set_handoff_in_progress(false);

    return IHandoffHandler::HANDOFF_SUCCESS;
}

void Application::session_error(ctvc::ClientErrorCode error_code)
{
    printf("#####################################################################\n");
    printf("TODO: show message in on-screen dialog to end-user, code:%d\n", error_code);
    printf("      PRESS OK TO CONTINUE\n");
    printf("#####################################################################\n");
}

///! \brief Just an example implementation.
void Application::run(const std::string &server, const std::string &app_url)
{
    RemoteSession *remote_session = create_remote_session();
    set_current_remote_session(remote_session);

    remote_session->initiate(server, app_url);

    while (true) {
        if (current_session()->get_state() != Session::STATE_CONNECTING && current_session()->get_state() != Session::STATE_CONNECTED) {
            break;
        }
        // handle key presses (the simple way)
        int key = Keyboard::get_key();
        if (key == 'q' || key == EOF) {
            CTVC_LOG_INFO("client terminates session");
            current_session()->get_control().terminate();
            break;  // Break the loop and immediately try to do cleanup. Should not break/crash client nor SDK
        }
        if (key) {
            bool client_must_handle_key_code;
            current_session()->get_input().send_keycode(key, IInput::ACTION_DOWN_AND_UP, client_must_handle_key_code);
            if (client_must_handle_key_code) {
                CTVC_LOG_INFO("client must handle the key");
            }
        }
    }

    CTVC_LOG_INFO("session closed (state:%d)", current_session()->get_state());

    // Cleanup
    current_session()->unregister_media_player("http");
    current_session()->unregister_media_player("https");

    delete m_current_remote_session;
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
    CTVC_LOG_INFO("TODO: StreamPlayer::start()");

    if (!m_forward_url.empty()) {
        return m_forwarder.open(m_forward_url.c_str());
    }

    return ResultCode::SUCCESS;
}

void Application::StreamPlayer::stop()
{
    CTVC_LOG_INFO("TODO: StreamPlayer::stop()");
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

// After a successful handoff to a new Session object instance, the old session has to stay
// alive long enough to tear the session down but at some point it must be cleaned up to
// prevent a memory leak.
void Application::delete_later(RemoteSession *session)
{
    if (m_session_cleanup_thread) {
        m_session_cleanup_thread->stop_and_wait_until_stopped();
        delete m_session_cleanup_thread;
    }
    if (m_session_cleanup) {
        delete m_session_cleanup;
    }
    m_session_cleanup = new SessionCleanup(session);
    m_session_cleanup_thread = new Thread("Session cleanup");
    m_session_cleanup_thread->start(*m_session_cleanup, Thread::PRIO_LOW);
}

Application::SessionCleanup::SessionCleanup(RemoteSession *session) : m_remote_session(session), m_retries(0)
{
}

bool Application::SessionCleanup::run()
{
    static const int MAX_RETRIES(10);

    Thread::sleep(1000);

    if (m_remote_session->session()->get_state() != Session::STATE_CONNECTED) {
        CTVC_LOG_INFO("Deleting old session (after successful handoff)");
        delete m_remote_session;
        m_remote_session = 0;
        return true;          // true: we're done here
    }

    m_retries++;
    if (m_retries == MAX_RETRIES) {
        CTVC_LOG_WARNING("Old session still active for more than %d seconds after rfbtv(s) session handoff, terminate it", MAX_RETRIES);
        // If we're still connected, then we (have to) take the initiative.
        m_remote_session->session()->get_control().terminate();
    } else {
        CTVC_LOG_INFO("Old session still connected after handoff, try to delete it later");
    }

    return false;             // false: try again later
}
