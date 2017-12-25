///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsChunkAllocator.h"

#include <stream/IMediaChunkAllocator.h>

#include <assert.h>

using namespace ctvc;

RamsChunkAllocator::RamsChunkAllocator() :
    m_media_chunk_allocator(0),
    m_chunk_size(0)
{
}

RamsChunkAllocator::~RamsChunkAllocator()
{
    clean_up();
}

void RamsChunkAllocator::clean_up()
{
    if (m_chunks.size() > 0) {
        assert(m_media_chunk_allocator);

        for (uint32_t i = 0; i < m_chunks.size(); i++) {
            m_media_chunk_allocator->free_chunk(m_chunks[i]);
        }

        m_chunks.clear();
    }
}

void RamsChunkAllocator::register_media_chunk_allocator(IMediaChunkAllocator *allocator)
{
    clean_up();

    m_media_chunk_allocator = allocator;

    m_chunk_size = allocator ? allocator->get_chunk_size() : 0;
}

uint32_t RamsChunkAllocator::getChunkSize() const
{
    return m_chunk_size;
}

uint8_t *RamsChunkAllocator::allocChunk()
{
    if (m_chunks.size() > 0) {
        uint8_t *p = m_chunks.back();
        m_chunks.pop_back();
        return p;
    } else if (m_media_chunk_allocator) {
        return m_media_chunk_allocator->alloc_chunk();
    } else {
        return 0;
    }
}

void RamsChunkAllocator::freeChunk(uint8_t *p)
{
    m_chunks.push_back(p);
}
