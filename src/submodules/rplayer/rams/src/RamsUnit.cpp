///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsUnit.h"

#include <rplayer/rams/IRamsChunkAllocator.h>

#include <algorithm>

#include <string.h>
#include <stdint.h>
#include <assert.h>

using namespace rplayer;

RamsUnit::RamsUnit(IRamsChunkAllocator &allocator) :
    m_allocator(allocator),
    m_size(0),
    m_currentChunkIndex(0)
{
}

RamsUnit::~RamsUnit()
{
    clear();
}

void RamsUnit::clear()
{
    for (unsigned int i = 0; i < m_chunks.size(); i++) {
        m_allocator.freeChunk(m_chunks[i]);
    }

    m_chunks.clear();
    m_size = 0;
    m_currentChunkIndex = 0;
}

bool RamsUnit::addBytes(const uint8_t *data, uint32_t size)
{
    const uint32_t chunkSize = m_allocator.getChunkSize();
    if (chunkSize == 0) {
        // Only possible if there is no, or an invalid, allocator
        return false;
    }

    assert(m_size <= m_chunks.size() * chunkSize);

    // First fill the remainder of the current last chunk (if any).
    uint32_t bytesLeft = m_chunks.size() * chunkSize - m_size;
    uint32_t nToCopy = std::min(bytesLeft, size);
    if (nToCopy > 0) {
        memcpy(m_chunks.back() + chunkSize - bytesLeft, data, nToCopy);
        data += nToCopy;
        size -= nToCopy;
        m_size += nToCopy;
    }

    // Then successively put the remaining bytes to copy (if any) into next chunks.
    while (size > 0) {
        uint8_t *p = m_allocator.allocChunk();
        if (!p) {
            return false;
        }

        m_chunks.push_back(p);

        uint32_t nToCopy = std::min(chunkSize, size);
        if (nToCopy > 0) {
            memcpy(p, data, nToCopy);
            data += nToCopy;
            size -= nToCopy;
            m_size += nToCopy;
        }
    }

    return true;
}

const uint8_t *RamsUnit::getDataSegment(bool isFirst, uint32_t &size/*out*/)
{
    const uint32_t chunkSize = m_allocator.getChunkSize();

    assert(m_size <= m_chunks.size() * chunkSize);

    if (isFirst) {
        m_currentChunkIndex = 0;
    }

    if (m_currentChunkIndex < m_chunks.size()) {
        size = std::min(m_size - m_currentChunkIndex * chunkSize, chunkSize);

        return m_chunks[m_currentChunkIndex++];
    } else {
        size = 0;

        return 0;
    }
}

uint32_t RamsUnit::getSize() const
{
    return m_size;
}

bool RamsUnit::applyPatch(uint32_t offset, const uint8_t *patch, uint32_t length)
{
    if (offset + length > m_size) {
        // Overflow, bail out
        return false;
    }

    const uint32_t chunkSize = m_allocator.getChunkSize();
    if (chunkSize == 0) {
        // Only possible if there is no, or an invalid, allocator and patching 0 bytes at offset 0 with m_size == 0
        return false;
    }

    uint32_t chunkIndex = offset / chunkSize;
    uint32_t chunkOffset = offset - chunkIndex * chunkSize;
    assert(chunkOffset < chunkSize);
    uint32_t n1 = std::min(chunkOffset + length, chunkSize) - chunkOffset;
    uint32_t n2 = length - n1;

    if (n1 > 0) {
        if (n2 > chunkSize) {
            // We can't (won't) patch more than 2 chunks
            return false;
        }

        // Apply patch in first chunk
        assert(chunkIndex < m_chunks.size());
        memcpy(m_chunks[chunkIndex] + chunkOffset, patch, n1);

        if (n2 > 0) {
            // Apply patch in second chunk
            chunkIndex++;
            assert(chunkIndex < m_chunks.size());
            memcpy(m_chunks[chunkIndex], patch + n1, n2);
        }
    }

    return true;
}
