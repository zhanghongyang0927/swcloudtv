///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <core/Session.h>
#include <core/SessionStateObserver.h>
#include <core/ClearKeyDrm.h>

#include <stream/IStreamPlayer.h>
#include <stream/StreamForwarder.h>

#include <string>

class Application
{
public:
    Application();
    virtual ~Application();

    void run(const std::string &server, const std::string &app_url, const std::string &forward_url);

protected:
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
    StreamPlayer m_stream_player;
    ClearKeyCdmSessionFactory m_clear_key_drm_system;
};
