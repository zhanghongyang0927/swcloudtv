///
/// \file IInput.h
///
/// \brief CloudTV Nano SDK mouse and key input interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace ctvc {

/// \brief Input interface for key and pointer events.
///
struct IInput
{
    IInput() {}
    virtual ~IInput() {}

    /// \brief Values for \a action parameter in send_keycode() or send_pointer_event().
    enum Action
    {
        ACTION_NONE, ///< No buttons or keys were pressed.
        ACTION_DOWN, ///< Button or key was pressed.
        ACTION_UP, ///< Button or key was released.
        ACTION_KEYINPUT, ///< Character has been generated. This is only applicable from RFB-TV 2.0 onwards.
        ACTION_DOWN_AND_UP ///< Button or key was pressed and released
    };

    /// \brief Values for \a button parameter in send_pointer_event().
    enum Button
    {
        NO_BUTTON,     ///< No button has changed state.
        LEFT_BUTTON,   ///< Left button has changed state.
        RIGHT_BUTTON,  ///< Right button has changed state.
        MIDDLE_BUTTON, ///< Middle button has changed state.
        WHEEL_UP,      ///< Wheel button has changed state upward.
        WHEEL_DOWN     ///< Wheel button has changed state downward.
    };

    /// \brief Send key code to the server.
    ///
    /// \param [in]  key     The native remote control value.
    /// \param [in]  action  The Action value.
    /// \param [out] client_must_handle_key_code true if the client needs to handle the key, false otherwise.
    ///
    /// If the key code map was initialized, then the \a key will be translated.
    ///
    /// \note If your platform is unable to distinguish the difference between a
    ///       pressed and a released key, then call this method with ACTION_DOWN_AND_UP.
    ///
    /// \note The value returned in \a client_must_handle_key_code depends on the state of the key filter. The
    ///       key filter is updated by server commands from the platform. This happens asynchronously, so there
    ///       is always a 'window' where a new application is entered on the platform where the key filter may
    ///       not yet be updated while the user presses a key. As a result, this key is sent to the platform
    ///       application instead of being handled locally or vice-versa.
    ///
    /// \see X11KeyMap
    virtual void send_keycode(int key, Action action, bool &client_must_handle_key_code) = 0;

    /// \brief Send pointer event to the server.
    /// \param [in] x       The X-coordinate
    /// \param [in] y       The Y-coordinate
    /// \param [in] button  Button or wheel that changed state.
    /// \param [in] action  Action that indicates the type of state change.
    ///
    /// \note If only a pointer move event needs to be sent, \a button should be NO_BUTTON
    ///       and \a action should be ACTION_NONE
    virtual void send_pointer_event(uint32_t x, uint32_t y, Button button, Action action) = 0;
};

} // namespace
