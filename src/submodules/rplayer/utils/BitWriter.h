///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

class BitWriter
{
public:
    explicit BitWriter(uint8_t *data, uint32_t size);

    void setData(uint8_t *data, uint32_t size);

    uint32_t getNBitsWritten() const { return m_bitIndex; }
    uint32_t getNBytesWritten() const { return m_bitIndex >> 3; }

    void reset();
    void align();
    void write(uint32_t bits, unsigned int num);
    void close();

    void writeBytes(const uint8_t *data, uint32_t n);

private:
    void flush();

    uint8_t *m_data;
    uint32_t m_size;
    uint32_t m_bitIndex;
    uint32_t m_index;
    uint32_t m_bits;
    uint32_t m_pos;
};

} // namespace rplayer
