///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

class TimeStamp
{
public:
    TimeStamp() : m_timeStampIn90kHzTicks(INVALID) {}

    void setAsMilliseconds(uint64_t ms) { m_timeStampIn90kHzTicks = ms * 90; normalize(); }
    uint64_t getAsMilliseconds() const { return (m_timeStampIn90kHzTicks + 45) / 90; }
    void setAs90kHzTicks(uint64_t ticks) { m_timeStampIn90kHzTicks = ticks; normalize(); }
    uint64_t getAs90kHzTicks() const { return m_timeStampIn90kHzTicks; }
    void setAsSeconds(double seconds) { m_timeStampIn90kHzTicks = static_cast<int64_t>(seconds * 90000 + 0.5); normalize(); }
    double getAsSeconds() const { return m_timeStampIn90kHzTicks / 90000.0; }

    bool isValid() const { return m_timeStampIn90kHzTicks != INVALID; }
    void invalidate() { m_timeStampIn90kHzTicks = INVALID; }

    bool operator==(const TimeStamp &rhs) const { return m_timeStampIn90kHzTicks == rhs.m_timeStampIn90kHzTicks; }
    bool operator>=(const TimeStamp &rhs) const { return ((m_timeStampIn90kHzTicks - rhs.m_timeStampIn90kHzTicks) & MASK_33rd_BIT) == 0; }
    bool operator<=(const TimeStamp &rhs) const { return ((rhs.m_timeStampIn90kHzTicks - m_timeStampIn90kHzTicks) & MASK_33rd_BIT) == 0; }
    bool operator!=(const TimeStamp &rhs) const { return m_timeStampIn90kHzTicks != rhs.m_timeStampIn90kHzTicks; }
    bool operator<(const TimeStamp &rhs) const { return ((m_timeStampIn90kHzTicks - rhs.m_timeStampIn90kHzTicks) & MASK_33rd_BIT) != 0; }
    bool operator>(const TimeStamp &rhs) const { return ((rhs.m_timeStampIn90kHzTicks - m_timeStampIn90kHzTicks) & MASK_33rd_BIT) != 0; }

    TimeStamp operator+(const TimeStamp &rhs) const { TimeStamp tmp(*this); tmp += rhs; return tmp; }
    TimeStamp operator-(const TimeStamp &rhs) const { TimeStamp tmp(*this); tmp -= rhs; return tmp; }
    TimeStamp &operator+=(const TimeStamp &rhs) { m_timeStampIn90kHzTicks += rhs.m_timeStampIn90kHzTicks; normalize(); return *this; }
    TimeStamp &operator-=(const TimeStamp &rhs) { m_timeStampIn90kHzTicks -= rhs.m_timeStampIn90kHzTicks; normalize(); return *this; }

    static TimeStamp zero() { TimeStamp t; t.m_timeStampIn90kHzTicks = 0; return t; }
    static TimeStamp milliseconds(uint64_t ms) { TimeStamp t; t.setAsMilliseconds(ms); return t; }
    static TimeStamp ticks(uint64_t ticks) { TimeStamp t; t.setAs90kHzTicks(ticks); return t; }
    static TimeStamp seconds(double seconds) { TimeStamp t; t.setAsSeconds(seconds); return t; }

private:
    static const int64_t INVALID = -1;
    static const int64_t MASK_33_BITS = 0x1FFFFFFFFLL;
    static const int64_t MASK_33rd_BIT = 0x100000000LL;

    int64_t m_timeStampIn90kHzTicks;

    void normalize()
    {
        m_timeStampIn90kHzTicks &= MASK_33_BITS;
    }
};

} // namespace rplayer
