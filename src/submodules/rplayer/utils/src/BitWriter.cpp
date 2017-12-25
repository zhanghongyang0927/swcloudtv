///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/utils/BitWriter.h>
#include <rplayer/utils/Logger.h>

#include <assert.h>

using namespace rplayer;

BitWriter::BitWriter(uint8_t *data, uint32_t size) :
    m_data(data),
    m_size(size),
    m_bitIndex(0),
    m_index(0),
    m_bits(0),
    m_pos(32)
{
}

void BitWriter::setData(uint8_t *data, uint32_t size)
{
    m_data = data;
    m_size = size;
    m_bitIndex = 0;
    m_index = 0;
    m_bits = 0;
    m_pos = 32;
}

void BitWriter::reset()
{
    m_bitIndex = 0;
    m_index = 0;
    m_bits = 0;
    m_pos = 32;
}

void BitWriter::align()
{
    if (m_pos & 7) {
        write(0, m_pos & 7);
    }
}

void BitWriter::write(uint32_t bits, unsigned int n)
{
    assert((n > 0) && (n <= 32));
    if (n < 32) {
        bits &= ~(~0U << n);
    }
    m_bitIndex += n;
    if (n <= m_pos) {
        m_pos -= n;
        m_bits |= bits << m_pos;
    } else {
        if (m_pos) { // workaround for >>32 issue on x86
            m_bits |= (bits >> (n - m_pos));
        }

        uint32_t tmp = m_bitIndex & 0x1F;
        m_bitIndex &= ~tmp;

        flush();

        m_bitIndex |= tmp;

        m_pos = 32 - n + m_pos;
        m_bits = bits << m_pos;
    }
}

void BitWriter::close()
{
    align();
    flush();
}

void BitWriter::flush()
{
    while ((m_index << 3) < m_bitIndex) {
        if (m_index < m_size) {
            m_data[m_index] = m_bits >> 24;
        } else {
            RPLAYER_LOG_ERROR("Write past end of data, size=%u, m_bitIndex=%u", m_size, m_bitIndex);
            break;
        }
        m_index++;
        m_bits <<= 8;
    }
}

void BitWriter::writeBytes(const uint8_t *data, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        write(data[i], 8);
    }
}
