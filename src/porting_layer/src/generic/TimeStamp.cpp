///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/TimeStamp.h>

#include <assert.h>

using namespace ctvc;

bool TimeStamp::operator==(const TimeStamp &rhs) const
{
    assert(is_comparable(rhs));
    return m_time == rhs.m_time;
}

bool TimeStamp::operator>=(const TimeStamp &rhs) const
{
    assert(is_comparable(rhs));
    return m_time >= rhs.m_time;
}

bool TimeStamp::operator<=(const TimeStamp &rhs) const
{
    assert(is_comparable(rhs));
    return m_time <= rhs.m_time;
}

bool TimeStamp::operator!=(const TimeStamp &rhs) const
{
    assert(is_comparable(rhs));
    return m_time != rhs.m_time;
}

bool TimeStamp::operator<(const TimeStamp &rhs) const
{
    assert(is_comparable(rhs));
    return m_time < rhs.m_time;
}

bool TimeStamp::operator>(const TimeStamp &rhs) const
{
    assert(is_comparable(rhs));
    return m_time > rhs.m_time;
}

TimeStamp &TimeStamp::operator+=(const TimeStamp &rhs)
{
    assert(is_valid() && rhs.is_valid());
    assert(((m_flags & rhs.m_flags) & IS_ABSOLUTE) == 0); // We cannot add two absolute times
    m_time += rhs.m_time;
    // If any is absolute time, we become absolute time
    m_flags |= (rhs.m_flags & IS_ABSOLUTE);
    return *this;
}

TimeStamp &TimeStamp::operator-=(const TimeStamp &rhs)
{
    assert(is_valid() && rhs.is_valid());
    assert(((~m_flags & rhs.m_flags) & IS_ABSOLUTE) == 0); // We cannot subtract an absolute time from a relative time
    m_time -= rhs.m_time;
    // If we're absolute time and rhs is relative time, we stay absolute time
    // If we're relative time and rhs is relative time, we stay relative time
    // If we're absolute time and rhs is absolute time, we become relative time
    m_flags &= ~(rhs.m_flags & IS_ABSOLUTE);
    return *this;
}
