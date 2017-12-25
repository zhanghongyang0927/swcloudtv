///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsUnitStore.h"
#include "RamsUnit.h"

#include <string.h>

using namespace rplayer;

//
// For speed reasons, the units are stored in a fixed-sized array. Since there are only
// 4K units at most, the required storage size is limited.
// We could use an std::map, but since units will be often added and removed, the CPU
// requirements are expected not to be negligible in this case.
// Having chosen a fixed-sized array, there still is the possibility to use 4K RamsUnit
// objects or use pointers and allocate them as required. Since a RamsUnit will require
// around 40 bytes of storage, allocating 4K of them will be require 160K of memory,
// which is a significant amount.
// Still, the burden of continually allocating and freeing memory is high. Therefore we
// also create a pool of RamsUnits that is used to store freed units for later reuse.
// Only if this pool gets empty, we'll allocate new RamsUnits.
// A call to reset() only clears and removes all units in use and returns the allocated
// units to the unit pool. Only a call to cleanUp() really cleans up all storage.
//

RamsUnitStore::RamsUnitStore() :
    m_chunkAllocator(0),
    m_units(MAX_UNIT_COUNT)
{
    memset(&m_units[0], 0, MAX_UNIT_COUNT * sizeof(m_units[0]));
}

RamsUnitStore::~RamsUnitStore()
{
    cleanUp();
}

void RamsUnitStore::registerRamsChunkAllocator(IRamsChunkAllocator *ramsChunkAllocator)
{
    // Make sure we free any contents using the old allocator, if any
    cleanUp();

    // And register the new one
    m_chunkAllocator = ramsChunkAllocator;
}

void RamsUnitStore::cleanUp()
{
    // Move everything in use from the unit store to the pool.
    reset();

    // Free everything in the pool
    for (uint32_t i = 0; i < m_pool.size(); i++) {
        delete m_pool[i];
    }
    m_pool.clear();
}

void RamsUnitStore::reset()
{
    // Remove and clear all units in use from the unit store and put them into the pool.
    for (uint32_t i = 0; i < m_units.size(); i++) {
        RamsUnit *unit = m_units[i];
        if (unit) {
            m_units[i] = 0;
            unit->clear();
            m_pool.push_back(unit);
        }
    }
}

RamsUnit *RamsUnitStore::getUnit(uint32_t unitId) const
{
    if (unitId >= MAX_UNIT_COUNT) {
        return 0;
    }

    // Get an existing unit, if present.
    return m_units[unitId];
}

RamsUnit *RamsUnitStore::getOrAllocateUnit(uint32_t unitId)
{
    if (unitId >= MAX_UNIT_COUNT) {
        return 0;
    }

    // Get an existing unit or allocate one if it didn't exist.
    RamsUnit *unit = m_units[unitId];
    if (!unit) {
        if (m_pool.size() > 0) {
            unit = m_pool.back();
            m_pool.pop_back();
        } else {
            if (m_chunkAllocator) {
                unit = new RamsUnit(*m_chunkAllocator);
            }
        }

        m_units[unitId] = unit;
    }

    return unit;
}

void RamsUnitStore::deleteUnit(uint32_t unitId)
{
    if (unitId >= MAX_UNIT_COUNT) {
        return;
    }

    RamsUnit *unit = m_units[unitId];
    if (unit) {
        m_units[unitId] = 0;
        unit->clear();
        m_pool.push_back(unit);
    }
}
