///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "MpegAudioFillerFrameCreator.h"

#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/BitWriter.h>
#include <rplayer/utils/Logger.h>

#include <string.h>

using namespace rplayer;

static const uint32_t s_mpeg2SamplingFrequencyTable[] = {
    44100, 48000, 32000
};

static const uint32_t s_mpeg2Layer1BitrateTable[] = {
    // Layer I bitrates in kbits/s
    0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448
};

static const uint32_t s_mpeg2Layer2BitrateTable[] = {
    // Layer II bitrates in kbits/s
    0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384
};

void MpegAudioFillerFrameCreator::processIncomingFrame(Frame *frame)
{
    const std::vector<uint8_t> &data(frame->m_data);

    const unsigned int MPEG_AUDIO_HEADER_SIZE = 4; // MPEG audio header is 4 bytes
    enum {
        // MPEG audio layers are strangely coded
        LAYER1 = 3, LAYER2 = 2, LAYER3 = 1
    };

    // Data cannot be MPEG audio, so don't try to parse it.
    if (data.size() < MPEG_AUDIO_HEADER_SIZE) {
        return;
    }

    BitReader bits(data);

    // 32 bits total
    unsigned syncword = bits.read(12);
    unsigned id = bits.read(1);
    unsigned layer = bits.read(2); // 3 for layer I, 2 for layer II and 1 for layer III...
    unsigned protectionBit = bits.read(1);
    unsigned bitrateIndex = bits.read(4);
    unsigned samplingFrequencyIndex = bits.read(2);
    unsigned paddingBit = bits.read(1);
    unsigned privateBit = bits.read(1);
    unsigned mode = bits.read(2);
    unsigned modeExtension = bits.read(2);
    unsigned copyright = bits.read(1);
    unsigned originalCopy = bits.read(1);
    unsigned emphasis = bits.read(2);

    if (syncword != 0xFFF /*|| id != 1*/ || layer == 0 || bitrateIndex == 15) {
        RPLAYER_LOG_WARNING("Unrecognized or unexpected MPEG audio header: sync=0x%03X, id=%d, layer=%d, bitrate=%d", syncword, id, layer, bitrateIndex);
        return;
    }

    if (samplingFrequencyIndex >= sizeof(s_mpeg2SamplingFrequencyTable) / sizeof(s_mpeg2SamplingFrequencyTable[0])) {
        RPLAYER_LOG_WARNING("MPEG audio illegal sampling frequency");
        return;
    }

    if (protectionBit != 1) {
        RPLAYER_LOG_WARNING("MPEG audio CRC not yet supported"); // TODO
        return;
    }

    if (bitrateIndex == 0) {
        RPLAYER_LOG_WARNING("MPEG free bitrate not supported"); // TODO
        return;
    }

    if (layer != LAYER1 && layer != LAYER2) {
        RPLAYER_LOG_WARNING("MPEG audio layer %d not supported", 4 - layer); // TODO
        return;
    }

    // Compute the frame length
    unsigned frameSize = (layer == LAYER1) ? 384 : 1152;
    unsigned bitrate = (layer == LAYER1) ? s_mpeg2Layer1BitrateTable[bitrateIndex] : s_mpeg2Layer2BitrateTable[bitrateIndex];
    unsigned samplingFrequency = s_mpeg2SamplingFrequencyTable[samplingFrequencyIndex];

    unsigned frameLength = frameSize * bitrate * 125 / samplingFrequency + paddingBit;

    if (frameLength != data.size()) {
        RPLAYER_LOG_WARNING("MPEG audio unexpected frame size, received %u, expected %u", static_cast<uint32_t>(data.size()), frameLength);
        return;
    }

    // Compute and set the duration
    // NOTE: For 44.1kHz-based sampling rates, this is inaccurate. We must take care that no error build-up will take place...
    // For an accurate representation, there should be a multiple of 49*90000 ticks per second. (49 = 7*7; 90000*49==44100*100.)
    uint32_t durationIn90kHzTicks = 90000 * frameSize / samplingFrequency; // Ticks per second * samples per frame / samples per second = ticks per frame

    frame->m_duration.setAs90kHzTicks(durationIn90kHzTicks);

    // We're done if the essential parts the first few bytes of the header are the same: in that case the main stream parameters did not change.
    if (m_silentAudioFrame.m_data.size() >= MPEG_AUDIO_HEADER_SIZE &&
        data[1] == m_silentAudioFrame.m_data[1] && // ID, layer & protection_bit
        (data[2] & 0xFC) == (m_silentAudioFrame.m_data[2] & 0xFC) && // bitrate_index & sampling_frequency
        (data[3] & 0xF0) == (m_silentAudioFrame.m_data[3] & 0xF0)) { // mode & mode_extension
        return;
    }

    RPLAYER_LOG_INFO("New MPEG audio frame read: layer %d, sf=%dHz, bitrate=%d, duration=%u, mode=%d", 4 - layer, samplingFrequency, bitrate, durationIn90kHzTicks, mode);

    //
    // Generate silence frame
    //
    // Reserve space for the entire frame
    m_silentAudioFrame.m_data.clear();
    m_silentAudioFrame.m_data.resize(frameLength - paddingBit); // We never pad filler frames for now

    // Write the header
    BitWriter bitsOut(&m_silentAudioFrame.m_data[0], m_silentAudioFrame.m_data.size());
    bitsOut.write(0xFFF, 12); // syncword
    bitsOut.write(id, 1); // id
    bitsOut.write(layer, 2); // layer
    bitsOut.write(protectionBit, 1); // protection_bit
    bitsOut.write(bitrateIndex, 4); // bitrate_index
    bitsOut.write(samplingFrequencyIndex, 2); // sampling_frequency
    bitsOut.write(0, 1); // padding_bit
    bitsOut.write(privateBit, 1); // private_bit
    bitsOut.write(mode, 2); // mode
    bitsOut.write(modeExtension, 2); // mode_extension
    bitsOut.write(copyright, 1); // copyright
    bitsOut.write(originalCopy, 1); // original_copy
    bitsOut.write(emphasis, 2); // emphasis
    bitsOut.close();

    // Fill the rest of the frame with zeros
    // The fun thing is that, while the format is quite complex, the audio data starts with
    // allocation bins that have a variable length (depending on various factors). All these
    // entries will map to 0 if their bits are 0. With allocation entries being 0, no sample
    // data will be encoded. And after the sample data, all remaining data is ancillary data.
    // So despite the fact that the individual boundaries are not straightforward, the final
    // frame will be effectively empty when all bits are 0. This greatly simplifies the creation
    // of filler frames. :-)
    memset(&m_silentAudioFrame.m_data[4], 0, m_silentAudioFrame.m_data.size() - 4);

    m_silentAudioFrame.m_duration.setAs90kHzTicks(durationIn90kHzTicks);

    RPLAYER_LOG_INFO("New MPEG silence frame is %u bytes", static_cast<unsigned>(m_silentAudioFrame.m_data.size()));
}

Frame *MpegAudioFillerFrameCreator::create()
{
    if (m_silentAudioFrame.m_data.empty()) {
        return 0;
    }

    return new Frame(m_silentAudioFrame);
}
