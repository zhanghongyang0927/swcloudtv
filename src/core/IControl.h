///
/// \file IControl.h
///
/// \brief CloudTV Nano SDK session control interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>
#include <map>

#include <inttypes.h>

namespace ctvc {

/// \brief Control interface for session control events.
///
struct IControl
{
    IControl() {}
    virtual ~IControl() {}

    /// \brief Initiate the client session with the remote server host and start the application.
    ///        The session will be in STATE_CONNECTING until the session is fully set up.
    /// \param [in] host  The remote host URL, e.g. "rfbtv://127.0.0.1:8095".
    /// \param [in] url   The application URL, e.g. "ctvprogram:youtube".
    /// \param [in] screen_width  The width of the client screen in pixels.
    /// \param [in] screen_height The height of the client screen in pixels.
    /// \param [in] optional_parameters A map of key-value pairs that will be added to the session setup message.
    ///
    /// \note This is a non-blocking call. The session is handled in its own thread. Check the actual status with
    ///       the get_state() method. When one or more mandatory properties are not set or an invalid host URL is given
    ///       then the session will not be set up, even though this method initially returns success (true).
    ///
    /// Possible parameter names and their values:
    /// \li "lang" The natural language to use by the UI application. The format is an IETF
    ///            language tag, e.g. "en". If not specified, the application will use a
    ///            default language.
    /// \li "lan"  Type of network connection that the client is using. Valid values are: "wlan",
    ///            "eth", "eth10", "eth100", "eth1000" and "LSC".
    /// \li "fw"   The firmware version running on the device, e.g. "1.3.2.300".
    /// \li "configured_display" Preferred display, typically used to indicate to the
    ///            server that an SD screen is connected. Example value: "pal4x3".
    ///
    /// Please refer to the documentation of the underlying protocol for details.
    virtual void initiate(const std::string &host, const std::string &url, uint32_t screen_width, uint32_t screen_height, const std::map<std::string, std::string> &optional_parameters) = 0;

    /// \brief Stop the session and disconnect from the server.
    ///
    /// \see initiate()
    virtual void terminate() = 0;

    /// \brief Suspend the session and disconnect from the server.
    ///
    /// Notifies the remote server that the client wishes to suspend the session.
    ///
    /// \see resume()
    virtual void suspend() = 0;

    /// \brief Connect to the server and resume the suspended session.
    ///
    /// Reconnects to the remote server and attempts to resume the session with
    /// the session identification that was saved when resume() was called.
    ///
    /// \see suspend()
    virtual void resume() = 0;

    /// \brief Update a number of session setup parameter key-value pairs at once.
    ///        May be called when a session is active. The existing parameters are updated and an update
    ///        message is sent to the server for those parameters that have their value changed.
    /// \note If multiple parameters are changed while a session is active, it is preferred to call this
    ///       method once rather than issuing multiple calls.
    ///
    /// \param [in] key_value_pairs A number of key-value pairs to be set or updated. See \a initiate()
    ///             for further info about individual keys and values for optional parameters.
    ///
    /// \see initiate()
    virtual void update_session_optional_parameters(const std::map<std::string, std::string> &key_value_pairs) = 0;
};

} // namespace
