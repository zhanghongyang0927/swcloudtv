///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace ctvc {

class IEvent
{
public:
    IEvent()
    {
    }

    virtual ~IEvent()
    {
    }

    /// \brief Handler method that must be overridden by the
    /// event implementation. Calling the method is intended
    /// to handle the event. The method should be called once
    /// at most, but is not guaranteed to be called.
    virtual void handle() const = 0;

private:
    // Events are not meant to be copied
    IEvent(const IEvent &);
    IEvent &operator=(const IEvent &);
};

class NullEvent : public IEvent
{
    /// \brief The NullEvent has an empty handler that does nothing.
    void handle() const
    {
    }
};

} // namespace
