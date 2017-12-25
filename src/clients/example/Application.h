///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "MyProtocolExtension.h"

#include <core/Session.h>
#include <core/SessionStateObserver.h>
#include <core/IOverlayCallbacks.h>

#include <stream/IStreamPlayer.h>
#include <stream/StreamForwarder.h>

#include <string>

class Application : public ctvc::Session::ISessionCallbacks
{
public:
    Application();
    virtual ~Application();

    void run(const std::string &server, const std::string &app_url, const std::string &forward_url);

protected:
    // Implement ISessionCallbacks
    void state_update(ctvc::Session::State state, ctvc::ClientErrorCode reason);

    // Implement the graphics overlay callbacks
    class OverlayCallbacks : public ctvc::IOverlayCallbacks
    {
        // overlay support
        virtual void overlay_blit_image(const ctvc::PictureParameters &picture_params);
        virtual void overlay_clear();
        virtual void overlay_flip();
    };

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

private:
    ctvc::SessionStateObserver m_state_observer;
    OverlayCallbacks m_overlay_callbacks;
    StreamPlayer m_stream_player;

    MyProtocolExtension m_my_protocol_extension;
};
