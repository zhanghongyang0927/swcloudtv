///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <core/Session.h>
#include <core/SessionStateObserver.h>
#include <core/IOverlayCallbacks.h>
#include <core/DefaultContentLoader.h>

#include <stream/IStreamPlayer.h>
#include <stream/StreamForwarder.h>

#include <string>

class Application : public ctvc::Session::ISessionCallbacks, public ctvc::IOverlayCallbacks
{
public:
    Application();
    virtual ~Application();

    void run(const std::string &server, const std::string &app_url);

    // Overlay callbacks
    virtual void overlay_blit_image(const ctvc::PictureParameters &picture_params);
    virtual void overlay_clear();
    virtual void overlay_flip();

protected:
    // Implement ISessionCallbacks
    void state_update(ctvc::Session::State state, ctvc::ClientErrorCode reason);

private:
    ctvc::SessionStateObserver m_state_observer;
    ctvc::DefaultContentLoader m_default_content_loader;
};
