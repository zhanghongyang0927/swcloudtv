///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/rams/IRamsChunkAllocator.h>

#include <vector>

#include <assert.h>

namespace ctvc {

struct IMediaChunkAllocator;

class RamsChunkAllocator : public rplayer::IRamsChunkAllocator
{
public:
    RamsChunkAllocator();
    ~RamsChunkAllocator();

    void register_media_chunk_allocator(IMediaChunkAllocator *allocator);

private:
    RamsChunkAllocator(const RamsChunkAllocator &);
    RamsChunkAllocator &operator=(const RamsChunkAllocator &);

    void clean_up();

    // Implements rplayer::IRamsChunkAllocator
    virtual uint32_t getChunkSize() const;
    virtual uint8_t *allocChunk();
    virtual void freeChunk(uint8_t *);

    IMediaChunkAllocator *m_media_chunk_allocator;
    uint32_t m_chunk_size;
    std::vector<uint8_t *> m_chunks;
};

} // namespace
