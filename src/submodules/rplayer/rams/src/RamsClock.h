///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer
{

class RamsOutput;

class RamsClock
{
public:
    RamsClock(RamsOutput &);
    ~RamsClock();

    void reset();

    // Synchronize clock
    // A new RAMS packet has arrived with a new clock reference that we need to take over.
    // The clock value is in ms units from the RAMS time base. The origin may differ from the real time.
    void synchronizeClock(uint16_t currentRamsClockInMs);

    // Set current real time in ms. The time may (and will) wrap around. This is no problem.
    // It should be continuous, however, meaning that any difference in the real time should
    // equal the difference in the time passed.
    // The origin of the absolute value does not matter.
    // If used, this method must be called immediately prior to each call to synchronizeClock()
    // for time management to properly operate.
    // A real-time thread may/can/will additionally call this on regular basis.
    void setCurrentTime(uint16_t currentRealTimeClockInMs);

private:
    RamsClock(const RamsClock &);
    RamsClock &operator=(const RamsClock &);

    RamsOutput &m_ramsOutput;

    bool m_isTimeSet; // Marks whether setCurrentTime() has been called at least once.
    uint16_t m_lastTime; // Keeps track of the last real time as passed to setCurrentTime().
    uint16_t m_clockSlowdownRemainder; // Keeps track of clock slowdown cycles not yet taken into account.

    bool m_isTimeSynchronized; // Marks whether synchronizeClock() has been called at least once.
    uint16_t m_currentRamsClock; // Keeps track of the current RAMS clock as passed by synchronizeClock() or updated by setCurrentTime().
};

}
