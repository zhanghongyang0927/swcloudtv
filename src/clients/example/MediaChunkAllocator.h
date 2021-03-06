/// \file MediaChunkAllocator.h
/// \brief Example chunked media memory allocator
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <stream/IMediaChunkAllocator.h>

class MediaChunkAllocator : public ctvc::IMediaChunkAllocator
{
public:
    MediaChunkAllocator()
    {
    }

    ~MediaChunkAllocator()
    {
    }

    static const uint32_t CHUNK_SIZE = 4096;

    virtual uint32_t get_chunk_size() const
    {
        return CHUNK_SIZE;
    }

    virtual uint8_t *alloc_chunk()
    {
        return new uint8_t[CHUNK_SIZE];
    }

    virtual void free_chunk(uint8_t *p)
    {
        delete[] p;
    }
};
