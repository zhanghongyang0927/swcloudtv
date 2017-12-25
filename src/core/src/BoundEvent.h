///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IEvent.h"

namespace ctvc {

template<class Handler, class Event> class BoundEvent : public IEvent
{
public:
    BoundEvent(Handler &object, void (Handler::*handler)(const Event &)) :
        m_object(object),
        m_handler(handler)
    {
    }

    void handle() const
    {
        (m_object.*m_handler)(*static_cast<const Event *>(this));
    }

private:
    Handler &m_object;
    void (Handler::*m_handler)(const Event &);
};

} // namespace
