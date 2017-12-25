///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

//
// IRamsChunkAllocator is an interface to an allocator of chunks of memory
// to be used by the RAMS unit store. The memory is allocated and freed
// on demand, which is quite often. The implementation might optimize the
// underlying allocations in a number of ways, one being keeping a pool of
// pre-allocated or freed chunks.
//
struct IRamsChunkAllocator
{
public:
    IRamsChunkAllocator() {}
    virtual ~IRamsChunkAllocator() {}

    // Get the fixed-sized chunk size for this allocator
    virtual uint32_t getChunkSize() const = 0;

    // Allocate and free a single chunk
    virtual uint8_t *allocChunk() = 0;
    virtual void freeChunk(uint8_t *) = 0;
};

} // namespace
