///
/// \file EventQueue.h
///
/// \brief Queue class that is intended to store events in a thread-safe way.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/Condition.h>

#include <list>

namespace ctvc {

class IEvent;

class EventQueue
{
public:
    EventQueue();
    ~EventQueue();

    /// \brief Put an event in the queue. Passing a null pointer is not allowed.
    /// Ownership of the Event object is transferred to the EventQueue. The event will be deleted once it's handled.
    /// The call will never block.
    void put(const IEvent *event);

    /// \brief Get an event from the queue.
    /// The call will block until data is available.
    /// Ownership of the Event object is transferred to the caller. The event must be deleted once it's handled.
    /// The received pointer is guaranteed to be non-null.
    ///
    /// \returns An event that has been previously posted.
    const IEvent *get();

    /// \brief Empty the queue, any queued events will be deleted.
    void clear();

private:
    EventQueue(const EventQueue &);
    EventQueue &operator=(const EventQueue &);

    Condition m_data_available;
    std::list<const IEvent*> m_queue;
};

} // namespace
