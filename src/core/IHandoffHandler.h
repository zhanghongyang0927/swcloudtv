///
/// \file IHandoffHandler.h
///
/// \brief CloudTV Nano SDK Session handoff handler.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>

namespace ctvc {

/// \brief RFB-TV Session handoff handling interface.
struct IHandoffHandler
{
    IHandoffHandler() {}
    virtual ~IHandoffHandler() {}

    /// \brief Result values of \a handoff_request().
    enum HandoffResult
    {
        HANDOFF_SUCCESS, // The handoff was successful and the session will close or suspend.
        HANDOFF_UNSUPPORTED_URI, // This is also returned if the handoff scheme is not registered.
        HANDOFF_FAILED_TO_DESCRAMBLE_STREAM,
        HANDOFF_FAILED_TO_DECODE_STREAM,
        HANDOFF_NO_TRANSPORT_STREAM_WITH_INDICATED_ID, // TSID
        HANDOFF_NO_NETWORK_WITH_INDICATED_ID,
        HANDOFF_NO_PROGRAM_WITH_INDICATED_ID,
        HANDOFF_PHYSICAL_LAYER_ERROR,
        HANDOFF_REQUIRED_MEDIA_PLAYER_ABSENT, // Or not installed
        HANDOFF_ERRONEOUS_REQUEST,
        HANDOFF_ASSET_NOT_FOUND, // Or URL or stream not found
        HANDOFF_TRANSPORT_LAYER_ERROR,
        HANDOFF_PLAYER_ERROR,
        HANDOFF_APP_NOT_FOUND,
        HANDOFF_UNSPECIFIED_ERROR
    };

    /// \brief Handle a hand off request to an internal app, like video on demand.
    /// \param [in] scheme The scheme that this handler was registered with. Passing the scheme back to the handler
    ///                    enables registering the same handler for multiple schemes if this is desirable. Otherwise,
    ///                    this parameter can be ignored.
    /// \param [in] uri Uniform Resource Indicator that the handoff is supplied with, application specific.
    /// \param [in] resume_session_when_done Resume session after playback has finished. (The session will be suspended in that case.)
    /// \result HANDOFF_SUCCESS if successful, another IHandoffHandler::HandoffResult value otherwise.
    /// \note Not supported by all protocol versions and depending on platform application.
    virtual HandoffResult handoff_request(const std::string &scheme, const std::string &uri, bool resume_session_when_done) = 0;
};

} // namespace
