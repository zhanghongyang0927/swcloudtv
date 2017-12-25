///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

#include <vector>

namespace rplayer {

class BitReader
{
public:
    explicit BitReader(const uint8_t *data, uint32_t size, uint32_t bitIndex);
    explicit BitReader(const std::vector<uint8_t> &data);

    void setData(const uint8_t *data, uint32_t size, uint32_t bitIndex);

    uint32_t getNBitsRead() const { return m_bitIndex; }
    int32_t getNBitsAvailable() const { return m_size * 8 - m_bitIndex; }

    void skip(uint32_t n);
    uint32_t read(uint32_t n);
    uint32_t peek(uint32_t n) const;

    void readBytes(uint8_t *data, uint32_t n);

private:
    void init(uint32_t bitIndex);
    void fillNextData(uint32_t dataOffset, uint32_t fillSize);

    const uint8_t *m_data;
    uint32_t m_size;
    uint32_t m_bitIndex;
    uint64_t m_nextData;
};

} // namespace rplayer
