///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/utils/H264SyntaxDecoder.h>
#include <rplayer/utils/H264Utils.h>
#include <rplayer/utils/Logger.h>

#include <algorithm>

#include <assert.h>

using namespace rplayer;

H264SyntaxDecoder::H264SyntaxDecoder(const uint8_t *data, uint32_t size, uint32_t bitIndex) :
    BitReader(data, size, bitIndex),
    m_isError(false)
{
}

H264SyntaxDecoder::H264SyntaxDecoder(const std::vector<uint8_t> &data) :
    BitReader(data),
    m_isError(false)
{
}

void H264SyntaxDecoder::setData(const uint8_t *data, uint32_t size, uint32_t bitIndex)
{
    BitReader::setData(data, size, bitIndex);
    m_isError = false;
}

uint32_t H264SyntaxDecoder::codeNum()
{

    int32_t bitsAvailable = getNBitsAvailable();
    if (bitsAvailable <= 0) {
        RPLAYER_LOG_ERROR("no more bits available in the buffer, bitsAvailable=%d", bitsAvailable);
        m_isError = true;
        return 0;
    }

    const uint32_t peekBits = std::min((uint32_t)bitsAvailable, (uint32_t)32);
    uint32_t pattern = peek(peekBits);
    pattern = pattern << (32 - peekBits);
    if (pattern > 0) {
        const uint32_t leadingZeroBits = countLeadingZeros(pattern);
        const uint32_t codeLength = (leadingZeroBits << 1) + 1;
        if (static_cast<int32_t>(codeLength) > bitsAvailable) {
            RPLAYER_LOG_ERROR("not enough bits available in the buffer, pattern=0x%x, codeLength=%u, bitsAvailable=%u", pattern, codeLength, bitsAvailable);
            m_isError = true;
            return 0;
        }

        skip(leadingZeroBits + 1); // Remove [M leadingZeros] [1] part from the bitstream
        uint32_t info = 0;
        if (leadingZeroBits) {
            info = read(leadingZeroBits); // Remove [Info] part from the bitstream, if any.
        }
        uint32_t codeNumVal = (1 << leadingZeroBits) - 1 + info;
        return codeNumVal;
    } else {
        // pattern == 0
        if (peekBits == 32) { // pattern == 0 AND peekBits == 32 => code too long
            RPLAYER_LOG_ERROR("code too long to parse, pattern=%u, peekBits=%u", pattern, peekBits);
        } else { // pattern == 0 AND peekBits < 32 == bitsAvailable => not enough bits
            RPLAYER_LOG_ERROR("not enough bits available in the buffer, pattern=%u, bitsAvailable=%u", pattern, bitsAvailable);
        }
        m_isError = true;
        return 0;
    }
}

uint32_t H264SyntaxDecoder::u(uint32_t n)
{
    int32_t bitsAvailable = getNBitsAvailable();
    if ((n <= 32) && (bitsAvailable >= (int32_t)n))
        return read(n);
    else {
        if (bitsAvailable <= 0) {
            RPLAYER_LOG_ERROR("no more bits available in the buffer, bitsAvailable=%d", bitsAvailable);
        } else if (n > (uint32_t)bitsAvailable) {
            RPLAYER_LOG_ERROR("not enough bits available in the buffer, bitsToRead=%u, bitsAvailable=%u", n, bitsAvailable);
        } else if (n > 32) {
            RPLAYER_LOG_ERROR("can't read more than 32-bits in one go, bitsToRead=%u", n);
        }
        m_isError = true;
        return 0;
    }
}

uint32_t H264SyntaxDecoder::ue()
{
    return codeNum();
}

int32_t H264SyntaxDecoder::se()
{
    const uint32_t code = codeNum();
    const uint32_t value = (code + 1) >> 1;
    return (code & 0x01) ? (int32_t)value : -value;
}

void H264SyntaxDecoder::u_skip(uint32_t n)
{
    int32_t bitsAvailable = getNBitsAvailable();
    if (bitsAvailable <= 0) {
        RPLAYER_LOG_ERROR("no more bits available in the buffer, bitsAvailable=%d", bitsAvailable);
        m_isError = true;
        return;
    }

    if (n > (uint32_t)bitsAvailable) {
        RPLAYER_LOG_ERROR("not enough bits available in the buffer, bitsToSkip=%u, bitsAvailable=%u", n, bitsAvailable);
        m_isError = true;
        return;
    }

    skip(n);
}

void H264SyntaxDecoder::ue_skip()
{
    codeNum();
}

void H264SyntaxDecoder::se_skip()
{
    codeNum();
}
