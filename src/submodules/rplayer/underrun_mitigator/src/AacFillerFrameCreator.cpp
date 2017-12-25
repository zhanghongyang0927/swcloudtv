///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "AacFillerFrameCreator.h"

#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/BitWriter.h>
#include <rplayer/utils/Logger.h>

#include <string.h>

using namespace rplayer;

static const uint32_t s_aacSamplingFrequencyTable[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000
};

static const uint8_t s_aacSilenceData[7][25] = {
    // Index is channel_configuration - 1
    // 1st byte is the number of bytes in the packet.
    // Packets are SCE, CPE, LFE and TERM element sequences coding silence. These are independent of the sampling frequency.
    // 1ch: 01 18 20 07
    { 4, 0x01, 0x18, 0x20, 0x07 },
    // 2ch: 21 10 04 60 8C 1C
    { 6, 0x21, 0x10, 0x04, 0x60, 0x8C, 0x1C },
    // 3ch: 01 18 20 01 08 80 23 04 60 E0
    { 10, 0x01, 0x18, 0x20, 0x01, 0x08, 0x80, 0x23, 0x04, 0x60, 0xE0 },
    // 4ch: 01 18 20 01 08 80 23 04 60 03 18 20 07
    { 13, 0x01, 0x18, 0x20, 0x01, 0x08, 0x80, 0x23, 0x04, 0x60, 0x03, 0x18, 0x20, 0x07 },
    // 5ch: 01 18 20 01 08 80 23 04 60 23 10 04 60 8C 1C
    { 15, 0x01, 0x18, 0x20, 0x01, 0x08, 0x80, 0x23, 0x04, 0x60, 0x23, 0x10, 0x04, 0x60, 0x8C, 0x1C },
    // 6ch: 01 18 20 01 08 80 23 04 60 23 10 04 60 8C 0C 23 00 00 E0
    { 19, 0x01, 0x18, 0x20, 0x01, 0x08, 0x80, 0x23, 0x04, 0x60, 0x23, 0x10, 0x04, 0x60, 0x8C, 0x0C, 0x23, 0x00, 0x00, 0xE0 },
    // 7ch: N/A
    // 8ch: 01 18 20 01 08 80 23 04 60 23 10 04 60 8C 04 A2 00 8C 11 81 84 60 00 1C
    { 24, 0x01, 0x18, 0x20, 0x01, 0x08, 0x80, 0x23, 0x04, 0x60, 0x23, 0x10, 0x04, 0x60, 0x8C, 0x04, 0xA2, 0x00, 0x8C, 0x11, 0x81, 0x84, 0x60, 0x00, 0x1C },
};

void AacFillerFrameCreator::processIncomingFrame(Frame *frame)
{
    const std::vector<uint8_t> &data(frame->m_data);

    const unsigned int ADTS_HEADER_SIZE = 7; // ADTS header is 7 bytes

    // Data cannot be ADTS, so don't try to parse it.
    if (data.size() < ADTS_HEADER_SIZE) {
        return;
    }

    BitReader bits(data);

    unsigned syncword = bits.read(12);
    unsigned id = bits.read(1);
    unsigned layer = bits.read(2);
    unsigned protectionAbsent = bits.read(1); // protection_absent
    unsigned profile = bits.read(2); // profile
    unsigned samplingFrequencyIndex = bits.read(4); // sampling_frequency_index
    unsigned privateBit = bits.read(1); // private_bit
    unsigned channelConfiguration = bits.read(3); // channel_configuration
    unsigned originalCopy = bits.read(1); // original_copy
    unsigned home = bits.read(1);
    // Variable part of the header follows; we only use number_of_raw_data_blocks_in_frame in order to be able to compute the frame duration.
    bits.skip(26); // Skip copyright bits(2), frame_length(13) and adts_buffer_fullness(11)
    unsigned number_of_raw_data_blocks_in_frame = bits.read(2) + 1;

    if (syncword != 0xFFF /*|| id != 1*/ || layer != 0) {
        RPLAYER_LOG_WARNING("Unrecognized or unexpected AAC header: sync=0x%03X, id=%d, layer=%d", syncword, id, layer);
        return;
    }

    if (protectionAbsent != 1) {
        RPLAYER_LOG_WARNING("AAC CRC not yet supported"); // TODO
        return;
    }

    if (samplingFrequencyIndex >= sizeof(s_aacSamplingFrequencyTable) / sizeof(s_aacSamplingFrequencyTable[0])) {
        RPLAYER_LOG_WARNING("AAC unsupported sampling frequency");
        return;
    }

    if (channelConfiguration == 0) {
        RPLAYER_LOG_WARNING("AAC channel configuration of 0 is not supported"); // TODO
        return;
    }

    // Compute and set the duration
    // NOTE: For 44.1kHz-based sampling rates, this is inaccurate. We must take care that no error build-up will take place...
    // For an accurate representation, there should be a multiple of 49*90000 ticks per second. (49 = 7*7; 90000*49==44100*100.)
    uint32_t durationIn90kHzTicks = (90000 * // Ticks per second
                                    1024 * // Samples per data block
                                    number_of_raw_data_blocks_in_frame) / // Data blocks per frame
                                    s_aacSamplingFrequencyTable[samplingFrequencyIndex]; // Samples per second
                                    // -> Ticks per frame

    frame->m_duration.setAs90kHzTicks(durationIn90kHzTicks);

    // We're done if the first 3.5 bytes of the header are the same: in that case the stream parameters did not change.
    if (m_silentAudioFrame.m_data.size() >= ADTS_HEADER_SIZE &&
        data[1] == m_silentAudioFrame.m_data[1] &&
        data[2] == m_silentAudioFrame.m_data[2] &&
        (data[3] & 0xF0) == (m_silentAudioFrame.m_data[3] & 0xF0)) {
        return;
    }

    RPLAYER_LOG_INFO("New AAC frame read: sf=%dHz, channels=%d", s_aacSamplingFrequencyTable[samplingFrequencyIndex], channelConfiguration == 7 ? 8 : channelConfiguration);

    //
    // Generate silence frame
    //
    unsigned byteCount = s_aacSilenceData[channelConfiguration - 1][0];
    const uint8_t *bytes = &s_aacSilenceData[channelConfiguration - 1][1];

    // Reserve space for the ADTS header
    m_silentAudioFrame.m_data.clear();
    m_silentAudioFrame.m_data.resize(7);

    BitWriter bitsOut(&m_silentAudioFrame.m_data[0], m_silentAudioFrame.m_data.size());
    bitsOut.write(0xFFF, 12); // syncword
    bitsOut.write(id, 1); // id
    bitsOut.write(layer, 2); // layer
    bitsOut.write(protectionAbsent, 1); // protection_absent
    bitsOut.write(profile, 2); // profile
    bitsOut.write(samplingFrequencyIndex, 4); // sampling_frequency_index
    bitsOut.write(privateBit, 1); // private_bit
    bitsOut.write(channelConfiguration, 3); // channel_configuration
    bitsOut.write(originalCopy, 1); // original_copy
    bitsOut.write(home, 1); // home
    bitsOut.write(0, 1); // copyright_id_bit
    bitsOut.write(0, 1); // copyright_id_start
    bitsOut.write(byteCount + ADTS_HEADER_SIZE, 13); // frame_length
    bitsOut.write(0x7FF, 11); // adts_buffer_fullness
    bitsOut.write(0, 2); // number_of_raw_data_blocks_in_frame
    bitsOut.close();

    m_silentAudioFrame.m_data.insert(m_silentAudioFrame.m_data.end(), bytes, bytes + byteCount);

    m_silentAudioFrame.m_duration.setAs90kHzTicks(durationIn90kHzTicks / number_of_raw_data_blocks_in_frame); // Silence frame only has one raw data block.

    RPLAYER_LOG_INFO("New AAC silence frame is %u bytes", static_cast<unsigned>(m_silentAudioFrame.m_data.size()));
}

Frame *AacFillerFrameCreator::create()
{
    if (m_silentAudioFrame.m_data.empty()) {
        return 0;
    }

    return new Frame(m_silentAudioFrame);
}
