///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/utils/H264SyntaxEncoder.h>
#include <rplayer/utils/H264Utils.h>
#include <rplayer/utils/Logger.h>

#include <assert.h>
#include <math.h>
#include <stdlib.h>

using namespace rplayer;

H264SyntaxEncoder::H264SyntaxEncoder(uint8_t *data, uint32_t size) :
    BitWriter(data, size)
{
    m_bufferSize = size;
    m_isError = false;
}

void H264SyntaxEncoder::setData(uint8_t *data, uint32_t size)
{
    BitWriter::setData(data, size);
    m_bufferSize = size;
    m_isError = false;
}

bool H264SyntaxEncoder::isSpaceFor(uint32_t size)
{
    const uint32_t bitsInBuffer = getNBitsWritten();
    return ((bitsInBuffer + size) > (m_bufferSize << 3)) ? false : true;
}

void H264SyntaxEncoder::u(uint32_t value, unsigned int size)
{
    if (isSpaceFor(size)) {
        write(value, size);
    } else {
        RPLAYER_LOG_ERROR("no more space available in the buffer, bufferSize(bytes)=%u, bitsInBuffer = %u, bitsToGo=%u", m_bufferSize, getNBitsWritten(), size);
        m_isError = true;
    }
}

void H264SyntaxEncoder::ue(uint16_t value)
{
    assert(value != 0xffff);

    const uint32_t codeNum = value + 1;
    const uint32_t M = 31 - countLeadingZeros(codeNum);
    const uint32_t codeLength = (M << 1) + 1;

    if (isSpaceFor(codeLength)) {
        write(codeNum, codeLength);
    } else {
        RPLAYER_LOG_ERROR("no more space available in the buffer, bufferSize(bytes)=%u, bitsInBuffer = %u, bitsToGo=%u, value=%u, code=%u", m_bufferSize, getNBitsWritten(), codeLength, value, codeNum);
        m_isError = true;
    }
}

void H264SyntaxEncoder::se(int16_t value)
{
    assert(value != -32768);

    uint32_t codeNum = abs(value) << 1;
    if (value > 0) {
        codeNum = codeNum - 1;
    }
    ue(codeNum);
}
