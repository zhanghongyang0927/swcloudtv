///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <core/Session.h>
#include <core/SessionStateObserver.h>
#include <core/IHandoffHandler.h>

#include <stream/HttpLoader.h>
#include <stream/IStreamPlayer.h>
#include <stream/StreamForwarder.h>
#include <stream/SimpleMediaPlayer.h>

#include <porting_layer/Mutex.h>

#include <string>

class Application : public ctvc::IHandoffHandler
{
public:
    Application();
    virtual ~Application();

    void run(const std::string &server, const std::string &app_url);

    void session_error(ctvc::ClientErrorCode error_code);

protected:
    // Because we want to know which session has terminated, we MUST subclass so that
    // the 'state_update' callback will be invoked to the appropriate instance.
    class RemoteSession : public ctvc::Session::ISessionCallbacks
    {
    public:
        RemoteSession(Application &application, ctvc::IOverlayCallbacks *overlay_callbacks);
        virtual ~RemoteSession();

        bool initiate(const std::string &server, const std::string &app_url);
        void set_handoff_in_progress(bool in_handoff);
        ctvc::Session *session() const;

    private:
        // Implement ISessionCallbacks
        void state_update(ctvc::Session::State state, ctvc::ClientErrorCode reason);

        Application &m_application;
        ctvc::SessionStateObserver m_state_observer;
        ctvc::Session *m_session;
        bool m_in_handoff;
    };

    class SessionCleanup : public ctvc::Thread::IRunnable
    {
    public:
        SessionCleanup(RemoteSession *session);
    private:
        bool run();
        RemoteSession *m_remote_session;
        int m_retries;
    };

    // Implement IHandoffHandler
    HandoffResult handoff_request(const std::string &scheme, const std::string &uri, bool resume_session_when_done);

    class StreamPlayer : public ctvc::IStreamPlayer
    {
    public:
        StreamPlayer();
        ~StreamPlayer();
        void set_forward_url(const std::string &forward_url);
        ctvc::ResultCode start();
        void stop();
        void stream_data(const uint8_t *data, uint32_t length);
        void stream_error(ctvc::ResultCode error_code);

    private:
        std::string m_forward_url;
        ctvc::StreamForwarder m_forwarder;
    };

    ctvc::Session *current_session() const;

private:
    HandoffResult handoff_request_rfbtv(const std::string &scheme, const std::string &uri);

    RemoteSession *create_remote_session();
    void set_current_remote_session(RemoteSession*);
    void delete_later(RemoteSession *session);

    mutable ctvc::Mutex m_session_mutex;
    RemoteSession *m_current_remote_session;
    StreamPlayer m_stream_player;
    ctvc::SimpleMediaPlayerFactory<ctvc::HttpLoader> m_http_media_player_factory;
    SessionCleanup *m_session_cleanup;
    ctvc::Thread *m_session_cleanup_thread;
};
