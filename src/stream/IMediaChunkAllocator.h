///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace ctvc {

/// \brief Abstract interface class for allocating chunks of media memory.
///
/// IMediaChunkAllocator is an interface to an allocator of chunks of memory
/// to be used by the media store. The memory is typically allocated more
/// toward the early life of the object and tends to be less after. The memory
/// is typically only freed near the very end of the object's lifetime.
/// All allocated chunks have the same size, but this size can be determined
/// by the implementation.
///
struct IMediaChunkAllocator
{
public:
    IMediaChunkAllocator() {}
    virtual ~IMediaChunkAllocator() {}

    /// \brief Get the chunk size for this allocator.
    ///
    /// \result The chunk size.
    ///
    /// Gets the fixed chunk size for this allocator.
    /// The chunk size should never change during the lifetime of the object.
    /// It should be a natural chunk size that optimizes performance with respect
    /// to memory access such as copies while keeping the memory overhead limited.
    /// Memory overhead occurs when storing small media segments using big chunks.
    ///
    virtual uint32_t get_chunk_size() const = 0;

    /// \brief Allocate a single chunk of memory.
    ///
    /// \result Pointer to the allocated chunk or null if no memory is left.
    ///
    /// Allocates single chunk of media memory.
    ///
    virtual uint8_t *alloc_chunk() = 0;

    /// \brief Free a previously allocated chunk of media memory.
    /// \param[in] p Pointer to the previously allocated chunk.
    ///
    /// \result void
    ///
    /// Frees a previously allocated chunk of media memory. A freed chunk is
    /// no longer accessed by the system.
    ///
    virtual void free_chunk(uint8_t *p) = 0;
};

} // namespace
