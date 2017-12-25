///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <core/SessionStateObserver.h>

#include <porting_layer/AutoLock.h>

using namespace ctvc;

SessionStateObserver::SessionStateObserver() :
    m_ok_states(0),
    m_error_states(0),
    m_is_ok_flagged(false),
    m_is_error_flagged(false)
{
}

void SessionStateObserver::set_states_to_wait_for(int ok_states, int error_states)
{
    AutoLock lck(m_condition);
    m_ok_states = ok_states;
    m_error_states = error_states;
    m_is_ok_flagged = false;
    m_is_error_flagged = false;
}

bool SessionStateObserver::wait_for_states() const
{
    while (true) {
        AutoLock lck(m_condition);
        if (m_is_ok_flagged) {
            return true;
        }
        if (m_is_error_flagged) {
            return false;
        }
        m_condition.wait_without_lock();
    }
}

void SessionStateObserver::state_update(Session::State state, ClientErrorCode /*error_code*/)
{
    {
        AutoLock lck(m_condition);
        // Update OK and error flags only if the flags are not yet set.
        if (!m_is_ok_flagged && !m_is_error_flagged) {
            m_is_ok_flagged = (state & m_ok_states) != 0;
            m_is_error_flagged = (state & m_error_states) != 0;
        }
    }
    m_condition.notify();
}
