///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <vector>

#include <inttypes.h>

namespace rplayer {

struct IRamsChunkAllocator;
class RamsUnit;

class RamsUnitStore
{
public:
    RamsUnitStore();
    ~RamsUnitStore();

    static const uint32_t MAX_UNIT_COUNT = (1U << 12); // RAMS has 12 bit unit IDs

    // Registration of a RAMS chunk allocator
    void registerRamsChunkAllocator(IRamsChunkAllocator *ramsChunkAllocator);

    // Reset and clear the entire store.
    void reset();

    // Get the unit with given unit ID.
    // If the unit ID is out of range or the unit is not found, a null pointer is returned.
    RamsUnit *getUnit(uint32_t unitId) const;

    // Get the unit with given unit ID, or allocate one if no was available yet.
    // If the unit ID is out of range or there is no memory, a null pointer is returned.
    RamsUnit *getOrAllocateUnit(uint32_t unitId);

    // Delete the unit with given unit ID (or empty its contents).
    void deleteUnit(uint32_t unitId);

private:
    RamsUnitStore(const RamsUnitStore &);
    RamsUnitStore &operator=(const RamsUnitStore &);

    void cleanUp();

    IRamsChunkAllocator *m_chunkAllocator;
    std::vector<RamsUnit *> m_units; // Fixed size
    std::vector<RamsUnit *> m_pool;
};

} // namespace
