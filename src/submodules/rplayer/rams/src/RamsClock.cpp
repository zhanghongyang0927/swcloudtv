///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsClock.h"
#include "RamsOutput.h"

// With respect to synchronization, five things may happen. Each of them may have their own way to deal with.
// 1. RAMS stream clock and real-time clock are synchronous but RAMS experiences some (minor) jitter.
//    This jitter will result in fluctuating lag and lead times of a few (tens of) milliseconds but average out to zero.
//    If the clock is always taken over immediately, the net effect would be that the resulting delay would be minimal.
//    Stream jitter will immediately be forwarded into output jitter, however.
// 2. RAMS stream clock runs (slightly) faster than the real-time clock.
//    This will result in continuous (small) lead times, building up if the clock is not synchronized.
//    If the clock is always taken over immediately, the net effect would be that the resulting delay would be minimal.
//    Performance would be fine, the clock would have small skips forward each time a new RAMS packet arrives. Some jitter
//    will be introduced in the stream output.
// 3. RAMS stream clock runs (slightly) slower than the real-time clock.
//    This will result in continuous (small) lag times, building up if the clock is not synchronized.
//    If the clock is always taken over immediately, the net effect would be that the resulting delay would be minimal.
//    Performance would be fine, the clock would have small skips backward each time a new RAMS packet arrives. Some jitter
//    will be introduced in the stream output.
// 4. RAMS stream suffers a temporary bandwidth shortage. This is somewhat equivalent to case 3 but probably with a greater
//    difference in clock speed.
//    This will result in lag times, building up if the clock is not synchronized.
//    If the clock is always taken over immediately, the low bandwidth would have immediate effect on the output bandwidth
//    because the output clock will lag as well. This is undesired behavior and delay will build up. Therefore, this needs
//    special action and the internal clock should remain free-running, albeit synchronized to long-term variations.
// 5. RAMS stream recovers after a temporary bandwidth shortage. This is somewhat equivalent to case 2 but with larger jumps
//    in time. If case 4 is handled properly, the internal clock should not have been deviated too much from the RAMS clock,
//    so resynchronization is not really necessary.
//
// This supports the following clock synchronization algorithm:
//  - If the RAMS clock leads the internal clock, the clock is simply synchronized immediately (covers 1, 2 and 5).
//  - If the RAMS clock lags the internal clock, a differentiation between cases 1, 3 and 4 is needed. We could add a clock
//    filter (e.g. a simple first or second order linear filter) that tries to follow the incoming clock with some time delay.
//    However, such filters are complex to manage and might lead to unforeseen behavior. In practice, we know that clocks
//    don't deviate a lot, so we might get away wit just letting the internal clock run a tiny bit slower than the RAMS clock.
//    This way, because RAMS packets will arrive regularly, we'll always get synchronized by RAMS. And /if/ there is a lag,
//    we know its a lag of type 4 of significant jitter of type 1. We don't need to adjust the internal clock then. Running
//    the internal clock somewhat slower than real-time means that case 3 simply cannot be possible. To it suffices to not
//    take over the RAMS clock if it lags. The only parameter that needs careful tuning is the real-time clock scaling factor.
//    If we assume that a clock has an accuracy of 2000ppm, it can deviate no more than 2.88 minutes (173 seconds) a day.
//    This seems a safe assumption. With the same accuracy, a stream lagging for 30 seconds will have been delayed (because
//    of the clock scaling factor) by 60ms. This seems acceptable. So, using a clock scaling factor of 499/500 (2000ppm slower
//    running clock) will be a good start.
//    The clock scaling is achieved by taking one unit every CLOCK_SLOWDOWN_FRACTION units of the incoming clock. This way,
//    the internal clock will run 1 / CLOCK_SLOWDOWN_FRACTION slower than real-time.

using namespace rplayer;

static const uint16_t CLOCK_SLOWDOWN_FRACTION = 512; // Power-of-2 speeds-up division and modulo operators but is not essential.

RamsClock::RamsClock(RamsOutput &ramsOutput) :
    m_ramsOutput(ramsOutput),
    m_isTimeSet(false),
    m_lastTime(0),
    m_clockSlowdownRemainder(0),
    m_isTimeSynchronized(false),
    m_currentRamsClock(0)
{
}

RamsClock::~RamsClock()
{
}

void RamsClock::reset()
{
    m_isTimeSet = false;
    m_lastTime = 0;
    m_clockSlowdownRemainder = 0;
    m_isTimeSynchronized = false;
    m_currentRamsClock = 0;
}

void RamsClock::synchronizeClock(uint16_t currentRamsClockInMs)
{
    bool synchronize = true;
    if (m_isTimeSet && m_isTimeSynchronized) {
        // We'll process lead or lag here (assuming these are not more than half the clock range).
        // Positive values indicate a lead (RAMS stream time is leading the real time).
        // Negative values indicate a lag (RAMS stream time is lagging the real time).
        // Lagging times we don't take.
        int lead = static_cast<int16_t>(currentRamsClockInMs - m_currentRamsClock);
        if (lead < 0) {
            synchronize = false;
        }
    }

    // Synchronize if required.
    if (synchronize) {
        m_currentRamsClock = currentRamsClockInMs;
        m_isTimeSynchronized = true;
    }

    // Output all units that are scheduled up to this time.
    m_ramsOutput.outputAllUnitsUntil(m_currentRamsClock);
}

void RamsClock::setCurrentTime(uint16_t currentRealTimeClockInMs)
{
    if (!m_isTimeSet) {
        // First time, the delta is 0
        m_lastTime = currentRealTimeClockInMs;
        m_isTimeSet = true;
    }

    uint16_t delta = currentRealTimeClockInMs - m_lastTime;

    m_lastTime = currentRealTimeClockInMs;

    // And correct for any slowdown we need to apply
    m_clockSlowdownRemainder += delta;
    delta -= m_clockSlowdownRemainder / CLOCK_SLOWDOWN_FRACTION;
    m_clockSlowdownRemainder %= CLOCK_SLOWDOWN_FRACTION;

    // Now process the new RAMS clock using the computed real-time delta.
    m_currentRamsClock += delta;

    // Output all units that are scheduled up to this time.
    m_ramsOutput.outputAllUnitsUntil(m_currentRamsClock);
}
