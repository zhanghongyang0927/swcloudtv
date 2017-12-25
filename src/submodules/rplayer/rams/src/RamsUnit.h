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

class RamsUnit
{
public:
    RamsUnit(IRamsChunkAllocator &);
    ~RamsUnit();

    // Clear the unit.
    void clear();

    // Add a number of bytes to the unit (allocates memory and copies bytes).
    // Returns true on success or false when not enough memory was available.
    bool addBytes(const uint8_t *data, uint32_t size);

    // Get a data segment in the unit.
    // Data segments can (and should) be obtained sequentially.
    // When the first data segment is to be obtained, set the isFirst flag to true.
    // If data is available, the method returns the data pointer and the size of the data.
    // If no (more) data is available, the method returns 0 and the size is set to 0.
    const uint8_t *getDataSegment(bool isFirst, uint32_t &size/*out*/);

    // Get the current aggregate unit size
    uint32_t getSize() const;

    // Apply a patch
    // The contents of the unit at location 'offset' are replaced with 'length' bytes from 'patch'.
    // Returns true on success or false when the patch couldn't be applied (out of bounds or covering more than 2 chunks).
    bool applyPatch(uint32_t offset, const uint8_t *patch, uint32_t length);

private:
    RamsUnit(const RamsUnit &);
    RamsUnit &operator=(const RamsUnit &);

    IRamsChunkAllocator &m_allocator;
    std::vector<uint8_t *> m_chunks;
    uint32_t m_size;
    uint32_t m_currentChunkIndex;
};

} // namespace
