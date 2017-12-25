///
/// \file SessionStateObserver.h
///
/// \brief Class that may help observing Session state changes and waiting for certain state changes to occur.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "Session.h"

#include <porting_layer/Condition.h>

namespace ctvc {

class SessionStateObserver : public Session::ISessionCallbacks
{
public:
    SessionStateObserver();

    /// \brief Set a certain set of states to wait for until one of them has arrived.
    /// \param [in] ok_states Logical OR of states that would indicate a desired situation.
    /// \param [in] error_states Logical OR of states that would indicate an error.
    /// \note As soon as this method is called, the first state change that matches one of the
    /// given sets will unblock the next call to wait_for_states().
    /// \note This function is typically used immediately prior to a call to IControl::initiate(),
    /// IControl::terminate() or similar.
    /// \see wait_for_states()
    void set_states_to_wait_for(int ok_states, int error_states);

    /// \brief Wait for one of the states set by set_states_to_wait_for() to have arrived.
    /// \returns true if the state matches one of the ok_states, false if the state matched
    /// one of the error_states.
    /// \note The first matching state transition after the last call to set_states_to_wait_for()
    /// will unblock this method.
    /// \note This function is typically used right after to a call to IControl::initiate(),
    /// IControl::terminate() or similar.
    /// \see set_states_to_wait_for()
    bool wait_for_states() const;

    // Implements ISessionCallbacks
    void state_update(Session::State state, ClientErrorCode error_code);

private:
    SessionStateObserver(const SessionStateObserver &);
    SessionStateObserver &operator=(const SessionStateObserver &);

    int m_ok_states;
    int m_error_states;
    bool m_is_ok_flagged;
    bool m_is_error_flagged;
    mutable Condition m_condition;
};

} // namespace
