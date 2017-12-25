///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/Logger.h>

#include <assert.h>

using namespace rplayer;

BitReader::BitReader(const uint8_t *data, uint32_t size, uint32_t bitIndex) :
    m_data(data),
    m_size(size),
    m_bitIndex(0),
    m_nextData(0)
{
    init(bitIndex);
}

BitReader::BitReader(const std::vector<uint8_t> &data) :
    m_data(&data[0]),
    m_size(data.size()),
    m_bitIndex(0),
    m_nextData(0)
{
    fillNextData(0, 8);
}


void BitReader::setData(const uint8_t *data, uint32_t size, uint32_t bitIndex)
{
    m_data = data;
    m_size = size;

    init(bitIndex);
}

void BitReader::init(uint32_t bitIndex)
{
    m_bitIndex = 0;
    m_nextData = 0;
    fillNextData(0, 8);
    skip(bitIndex);
}

void BitReader::skip(uint32_t n)
{
    fillNextData((m_bitIndex >> 3) + 8, ((m_bitIndex + n) >> 3) - (m_bitIndex >> 3));
    m_bitIndex += n;

    if (m_bitIndex > (m_size * 8)) {
        RPLAYER_LOG_ERROR("Read past end of data, size=%u, m_bitIndex=%u", m_size, m_bitIndex);
    }
}

uint32_t BitReader::read(uint32_t n)
{
    uint32_t r = peek(n);
    skip(n);
    return r;
}

uint32_t BitReader::peek(uint32_t n) const
{
    assert((n > 0) && (n <= 32));
    const uint32_t mask = (1ULL << n) - 1;
    return (m_nextData >> (64 - n - (m_bitIndex & 7))) & mask;
}

void BitReader::readBytes(uint8_t *data, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        data[i] = read(8);
    }
}

void BitReader::fillNextData(uint32_t dataOffset, uint32_t fillSize)
{
    while (fillSize) {
        m_nextData <<= 8;
        if (dataOffset < m_size) {
            m_nextData |= m_data[dataOffset];
        }
        dataOffset++;
        fillSize--;
    }
}
