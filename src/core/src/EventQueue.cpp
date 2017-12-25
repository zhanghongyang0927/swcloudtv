///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "EventQueue.h"
#include "IEvent.h"

#include <porting_layer/AutoLock.h>

#include <assert.h>

using namespace ctvc;

EventQueue::EventQueue()
{
}

EventQueue::~EventQueue()
{
    clear();
}

void EventQueue::clear()
{
    AutoLock lck(m_data_available);

    while (!m_queue.empty()) {
        delete m_queue.front();
        m_queue.pop_front();
    }
}

void EventQueue::put(const IEvent *event)
{
    assert(event);

    {
        AutoLock lck(m_data_available);
        m_queue.push_back(event);
    }

    // No lock needed to notify and it saves a context switch
    m_data_available.notify();
}

const IEvent *EventQueue::get()
{
    AutoLock lck(m_data_available);

    while (m_queue.empty()) {
        m_data_available.wait_without_lock();
    }

    const IEvent *event = m_queue.front();
    m_queue.pop_front();

    return event;
}
