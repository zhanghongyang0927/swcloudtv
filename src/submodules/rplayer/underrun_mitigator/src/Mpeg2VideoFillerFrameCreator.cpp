///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "Mpeg2VideoFillerFrameCreator.h"
#include "Frame.h"

#include <rplayer/utils/Logger.h>

#include <algorithm>
#include <assert.h>

using namespace rplayer;

#define SEQUENCE_HEADER_CODE 0xB3
#define PICTURE_START_CODE 0x00
#define GROUP_START_CODE 0xB8

const size_t UNINITIALIZED_OFFSET = ~0U;

struct Vlc
{
    unsigned short m_code;
    unsigned short m_len;
};

static const Vlc s_addrIncTab[33] = {
    { 0x01, 1 },  { 0x03, 3 },  { 0x02, 3 },  { 0x03, 4 },  { 0x02, 4 },  { 0x03, 5 },  { 0x02, 5 },  { 0x07, 7 },
    { 0x06, 7 },  { 0x0b, 8 },  { 0x0a, 8 },  { 0x09, 8 },  { 0x08, 8 },  { 0x07, 8 },  { 0x06, 8 },  { 0x17, 10 },
    { 0x16, 10 }, { 0x15, 10 }, { 0x14, 10 }, { 0x13, 10 }, { 0x12, 10 }, { 0x23, 11 }, { 0x22, 11 }, { 0x21, 11 },
    { 0x20, 11 }, { 0x1f, 11 }, { 0x1e, 11 }, { 0x1d, 11 }, { 0x1c, 11 }, { 0x1b, 11 }, { 0x1a, 11 }, { 0x19, 11 },
    { 0x18, 11 }
};

class NextStartCode
{
public:
    NextStartCode(const uint8_t *data, size_t sizeInBytes);
    bool operator()(size_t &startCodeOffset, size_t &bitstreamSize, int &startCodeValue);

private:
    const uint8_t *m_data;
    size_t m_size;

    size_t m_startCodeOffset;
    size_t m_parseOffset;
};

NextStartCode::NextStartCode(const uint8_t *data, size_t sizeInBytes) :
    m_data(data),
    m_size(sizeInBytes),
    m_startCodeOffset(UNINITIALIZED_OFFSET),
    m_parseOffset(0)
{
}

bool NextStartCode::operator()(size_t &startCodeOffset, size_t &bitstreamSize, int &startCodeValue)
{
    size_t size = m_size;
    if (size < 4) {
        return false;
    }

    const uint8_t *data = m_data;
    for (size_t i = m_parseOffset; i < size - 4; i++) {
        if (data[i + 0] == 0x0 && data[i + 1] == 0x0 && data[i + 2] == 0x1) {
            if (m_startCodeOffset == UNINITIALIZED_OFFSET) {
                m_startCodeOffset = i;
            } else {
                startCodeOffset = m_startCodeOffset;
                bitstreamSize = i - m_startCodeOffset;
                startCodeValue = data[m_startCodeOffset + 3];
                m_startCodeOffset = i;
                m_parseOffset = i + 3;
                return true;
            }
        }
    }

    m_parseOffset = size - 4;
    return false;
}

Mpeg2VideoFillerFrameCreator::Mpeg2VideoFillerFrameCreator() :
    m_isValidSequenceHeader(false),
    m_isValidPictureHeader(false),
    m_nextTemporalReference(0),
    m_horizontal_size_value(0),
    m_vertical_size_value(0)
{
}

Mpeg2VideoFillerFrameCreator::~Mpeg2VideoFillerFrameCreator()
{
}

void Mpeg2VideoFillerFrameCreator::processIncomingFrame(Frame *frame)
{
    RPLAYER_LOG_DEBUG("Processing MPEG2 frame");
    NextStartCode nextStartCode(&frame->m_data[0], frame->m_data.size());

    int startCodeValue;
    size_t startCodeOffset;
    size_t bitstreamSize;

    while (nextStartCode(startCodeOffset, bitstreamSize, startCodeValue) == true) {
        switch (startCodeValue) {
        case SEQUENCE_HEADER_CODE: {
            RPLAYER_LOG_DEBUG("MPEG2 sequence header");
            parseSequenceHeader((&frame->m_data[0]) + startCodeOffset, bitstreamSize);
            m_isValidSequenceHeader = true;
            break;
        }

        case PICTURE_START_CODE: {
            RPLAYER_LOG_DEBUG("MPEG2 picture header");
            if (bitstreamSize < 6) {
                RPLAYER_LOG_ERROR("Invalid picture header");
                break;
            }
            patchTemporalReference((&frame->m_data[0]) + startCodeOffset);
            m_isValidPictureHeader = true;
            break;
        }

        case GROUP_START_CODE: {
            RPLAYER_LOG_DEBUG("MPEG2 group header");
            // Reset temporal reference
            m_nextTemporalReference = 0;
            break;
        }

        default:
            break;
        }
    }
}

void Mpeg2VideoFillerFrameCreator::parseSequenceHeader(const uint8_t *data, size_t size)
{
    BitReader b(data, size, 0);
    b.skip(32); //Skip the sequenceStartCode along with header bytes
    m_horizontal_size_value = b.read(12);
    m_vertical_size_value = b.read(12);
    RPLAYER_LOG_DEBUG("MPEG2 sequence header: m_horizontal_size_value=%d, m_vertical_size_value=%d", m_horizontal_size_value, m_vertical_size_value);
}

void Mpeg2VideoFillerFrameCreator::patchTemporalReference(uint8_t *data)
{
    // Patch temporal_reference
    const int temporalReference = (data[4] << 2) | ((data[5] & 0xC0) >> 6);
    if (temporalReference != m_nextTemporalReference) {
        RPLAYER_LOG_DEBUG("MPEG2 patched temporal reference from %d to %d", temporalReference, m_nextTemporalReference);
    }

    // Patch temporal_reference
    data[4] = (m_nextTemporalReference >> 2) & 0xFF;
    data[5] = (data[5] & 0x3F) | ((m_nextTemporalReference << 6) & 0xC0);

    // Compute next temporal reference
    m_nextTemporalReference = (m_nextTemporalReference + 1) & 0x3FF;
}

Frame *Mpeg2VideoFillerFrameCreator::create()
{
    if ((m_isValidSequenceHeader == false) || (m_isValidPictureHeader == false)) {
        RPLAYER_LOG_DEBUG("No valid MPEG2 sequence/picture header received yet, can't generate video filler frame");
        return 0;
    }

    RPLAYER_LOG_DEBUG("Generating MPEG2 video filler frame");

    // Picture header
    uint8_t bitBuffer[512 * 4]; // Allocate enough data space for PictureStartCode and FillerSlice
    BitWriter bitOut(bitBuffer, sizeof(bitBuffer));
    encodeFillerPictureHeader(bitOut);

    for (int y = 0; y < (m_vertical_size_value >> 4); y++) {
        encodeFillerSlice(bitOut, y, (m_horizontal_size_value >> 4));
    }

    // Close bitstream
    bitOut.close();

    // Fill in and increment the temporal reference
    patchTemporalReference(bitBuffer);

    // Create frame
    Frame *frame = new Frame();
    frame->m_data.reserve(bitOut.getNBytesWritten());
    frame->m_data.insert(frame->m_data.end(), bitBuffer, bitBuffer + bitOut.getNBytesWritten());

    return frame;
}

void Mpeg2VideoFillerFrameCreator::encodeFillerPictureHeader(BitWriter &out)
{
    // N.B. The picture header is invariant so we could use a pre-encoded array here in order to to save some code and some cycles.
    int vbvDelay = 0xFFFF;
    int intraDcPrecision = 10;

    out.align();

    // picture_header()
    out.write(0x00000100, 32);
    out.write(0, 10); // 'real' temportal reference will be filled in later
    out.write(2, 3); // Case: VIDEO_FRAME_TYPE_P
    out.write(vbvDelay, 16);
    out.write(0, 1); // Case: VIDEO_FRAME_TYPE_P
    out.write(7, 3); // Case: VIDEO_FRAME_TYPE_P
    out.write(0, 1);
    out.align();

    // picture_coding_extension()
    out.write(0x000001B5, 32);
    out.write(8, 4);

    out.write(0x55, 8);
    out.write(0xFF, 8);

    switch (intraDcPrecision) {
    case 8:
        out.write(0, 2);
        break;
    case 9:
        out.write(1, 2);
        break;
    case 10:
        out.write(2, 2);
        break;
    default:
        RPLAYER_LOG_ERROR("frame header error ?????");
    }
    out.write(3, 2);
    out.write(0, 1);
    out.write(1, 1);
    out.write(0, 1);
    out.write(0, 1);
    out.write(0, 1);
    out.write(0, 1);
    out.write(0, 1);
    out.write(1, 1);
    out.write(1, 1);
    out.write(0, 1);

    out.align();
}

void Mpeg2VideoFillerFrameCreator::encodeFillerSlice(BitWriter &out, int mbY, int mbW)
{
    out.write(0, 8); //header
    out.write(0, 8);
    out.write(1, 8);
    out.write(static_cast<uint8_t>(mbY + 1), 8);

    out.write(2, 5); // quant
    out.write(0, 1); // extra_bit_slice
    out.write(1, 1);
    out.write(1, 3); // P MC, Not Coded
    out.write(1, 1); // motion x
    out.write(1, 1); // motion y

    if (mbW > 1) {
        int addressIncrement = mbW - 2;
        while (addressIncrement >= 33) {
            out.write(8, 11);
            addressIncrement -= 33;
        }
        out.write(s_addrIncTab[addressIncrement].m_code, s_addrIncTab[addressIncrement].m_len);
        out.write(1, 3); // P MC, Not Coded
        out.write(1, 1); // motion x
        out.write(1, 1); // motion y
    }
    out.close();
}
