///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "Ac3FillerFrameCreator.h"

#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/BitWriter.h>
#include <rplayer/utils/Logger.h>

#include <string.h>

using namespace rplayer;

static const uint32_t s_ac3SamplingFrequencyTable[] = {
    48000, 44100, 32000
};

// Nominal bitrate in kbps; index is frmsizecod
static const uint32_t s_ac3BitrateTable[] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

// Channels table based on acmod
static const uint8_t s_ac3ChannelsTable[8] = {
    2, 1, 2, 3, 3, 4, 4, 5
};

// Polynomial x16 + x15 + x2 + 1, or 0x8005
static uint16_t crc16(const uint8_t *data, uint32_t length)
{
    static const uint16_t s_crcTable[] = {
        0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011, 0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
        0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072, 0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
        0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2, 0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
        0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1, 0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
        0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192, 0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
        0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1, 0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
        0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151, 0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
        0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132, 0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
        0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312, 0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
        0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371, 0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
        0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1, 0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
        0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2, 0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
        0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291, 0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
        0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2, 0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
        0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252, 0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
        0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231, 0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
    };

    uint16_t crc = 0;
    while (length--) {
        uint8_t tmp = *data++ ^ (crc >> 8);
        crc <<= 8;
        crc ^= s_crcTable[tmp];
    }
    return crc;
}

// The same, but now implemented rear to front
static uint16_t reverseCrc16(const uint8_t *data, uint32_t length)
{
    static const uint16_t s_crcTable[] = {
        0x0000, 0x7F81, 0xFF02, 0x8083, 0x7E01, 0x0180, 0x8103, 0xFE82, 0xFC02, 0x8383, 0x0300, 0x7C81, 0x8203, 0xFD82, 0x7D01, 0x0280,
        0x7801, 0x0780, 0x8703, 0xF882, 0x0600, 0x7981, 0xF902, 0x8683, 0x8403, 0xFB82, 0x7B01, 0x0480, 0xFA02, 0x8583, 0x0500, 0x7A81,
        0xF002, 0x8F83, 0x0F00, 0x7081, 0x8E03, 0xF182, 0x7101, 0x0E80, 0x0C00, 0x7381, 0xF302, 0x8C83, 0x7201, 0x0D80, 0x8D03, 0xF282,
        0x8803, 0xF782, 0x7701, 0x0880, 0xF602, 0x8983, 0x0900, 0x7681, 0x7401, 0x0B80, 0x8B03, 0xF482, 0x0A00, 0x7581, 0xF502, 0x8A83,
        0x6001, 0x1F80, 0x9F03, 0xE082, 0x1E00, 0x6181, 0xE102, 0x9E83, 0x9C03, 0xE382, 0x6301, 0x1C80, 0xE202, 0x9D83, 0x1D00, 0x6281,
        0x1800, 0x6781, 0xE702, 0x9883, 0x6601, 0x1980, 0x9903, 0xE682, 0xE402, 0x9B83, 0x1B00, 0x6481, 0x9A03, 0xE582, 0x6501, 0x1A80,
        0x9003, 0xEF82, 0x6F01, 0x1080, 0xEE02, 0x9183, 0x1100, 0x6E81, 0x6C01, 0x1380, 0x9303, 0xEC82, 0x1200, 0x6D81, 0xED02, 0x9283,
        0xE802, 0x9783, 0x1700, 0x6881, 0x9603, 0xE982, 0x6901, 0x1680, 0x1400, 0x6B81, 0xEB02, 0x9483, 0x6A01, 0x1580, 0x9503, 0xEA82,
        0xC002, 0xBF83, 0x3F00, 0x4081, 0xBE03, 0xC182, 0x4101, 0x3E80, 0x3C00, 0x4381, 0xC302, 0xBC83, 0x4201, 0x3D80, 0xBD03, 0xC282,
        0xB803, 0xC782, 0x4701, 0x3880, 0xC602, 0xB983, 0x3900, 0x4681, 0x4401, 0x3B80, 0xBB03, 0xC482, 0x3A00, 0x4581, 0xC502, 0xBA83,
        0x3000, 0x4F81, 0xCF02, 0xB083, 0x4E01, 0x3180, 0xB103, 0xCE82, 0xCC02, 0xB383, 0x3300, 0x4C81, 0xB203, 0xCD82, 0x4D01, 0x3280,
        0x4801, 0x3780, 0xB703, 0xC882, 0x3600, 0x4981, 0xC902, 0xB683, 0xB403, 0xCB82, 0x4B01, 0x3480, 0xCA02, 0xB583, 0x3500, 0x4A81,
        0xA003, 0xDF82, 0x5F01, 0x2080, 0xDE02, 0xA183, 0x2100, 0x5E81, 0x5C01, 0x2380, 0xA303, 0xDC82, 0x2200, 0x5D81, 0xDD02, 0xA283,
        0xD802, 0xA783, 0x2700, 0x5881, 0xA603, 0xD982, 0x5901, 0x2680, 0x2400, 0x5B81, 0xDB02, 0xA483, 0x5A01, 0x2580, 0xA503, 0xDA82,
        0x5001, 0x2F80, 0xAF03, 0xD082, 0x2E00, 0x5181, 0xD102, 0xAE83, 0xAC03, 0xD382, 0x5301, 0x2C80, 0xD202, 0xAD83, 0x2D00, 0x5281,
        0x2800, 0x5781, 0xD702, 0xA883, 0x5601, 0x2980, 0xA903, 0xD682, 0xD402, 0xAB83, 0x2B00, 0x5481, 0xAA03, 0xD582, 0x5501, 0x2A80
    };

    uint16_t crc = 0;
    const uint8_t *p = data + length;
    for (uint32_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ s_crcTable[crc & 0xFF] ^ (*--p << 8);
    }
    return crc;
}

Ac3FillerFrameCreator::Ac3FillerFrameCreator() :
    m_sampleRateCode(0),
    m_frameSizeCode(0),
    m_audioCodingMode(0),
    m_lfePresent(0)
{
}

void Ac3FillerFrameCreator::processIncomingFrame(Frame *frame)
{
    const std::vector<uint8_t> &data(frame->m_data);

    const unsigned int MIN_AC3_FRAME_SIZE = 64;

    // Data cannot be AC3, so don't try to parse it.
    if (data.size() < MIN_AC3_FRAME_SIZE) {
        RPLAYER_LOG_WARNING("Frame too small for AC-3: size=%u", static_cast<uint32_t>(data.size()));
        return;
    }

    BitReader bits(data);

    // Read sync info
    unsigned syncword = bits.read(16);
    bits.skip(16); // crc1
    unsigned fscod = bits.read(2); // Sample rate code
    unsigned frmsizecod = bits.read(6); // Frame size code

    // Read bsi (bit stream information)
    unsigned bsid = bits.read(5);
    unsigned bsmod = bits.read(3);
    unsigned acmod = bits.read(3);
    unsigned cmixlev = 0;
    if ((acmod & 0x1) && (acmod != 0x1)) { // If 3 front channels
        cmixlev = bits.read(2);
    }
    unsigned surmixlev = 0;
    if (acmod & 0x4) { // If a surround channel exists
        surmixlev = bits.read(2);
    }
    if (acmod == 0x2) { // If in 2/0 mode
        bits.skip(2); // dsurmod
    }
    unsigned lfeon = bits.read(1);
    unsigned dialnorm = bits.read(5);

    // The rest of bsi is discarded because we don't need it.

    if (syncword != 0x0B77 ||
        fscod >= sizeof(s_ac3SamplingFrequencyTable) / sizeof(s_ac3SamplingFrequencyTable[0]) ||
        (frmsizecod >> 1) >= sizeof(s_ac3BitrateTable) / sizeof(s_ac3BitrateTable[0]) ||
        bsid > 8) {
        RPLAYER_LOG_WARNING("Unrecognized or unexpected AC-3 header: sync=0x%03X, fscod=%d, frmsizecod=%d", syncword, fscod, frmsizecod);
        return;
    }

    uint32_t samplingFrequency = s_ac3SamplingFrequencyTable[fscod];

    static const uint32_t SAMPLES_PER_FRAME = 6 * 256;
    uint32_t frameSizeInWords = s_ac3BitrateTable[frmsizecod >> 1]/*kbits/s*/ * (SAMPLES_PER_FRAME * 1000/*bits/kbit*/ / 16/*bits/word*/) / samplingFrequency/*samples/s*/; // -> words per frame
    if (fscod == 1 && (frmsizecod & 1) != 0) {
        frameSizeInWords++;
    }
    uint32_t frameSize = 2 * frameSizeInWords;
    if (data.size() != frameSize) {
        RPLAYER_LOG_WARNING("Frame size mismatch for AC-3: actual=%u, expected=%u", static_cast<uint32_t>(data.size()), frameSize);
        return;
    }

    // Compute and set the duration
    // NOTE: For 44.1kHz-based sampling rates, this is inaccurate. We must take care that no error build-up will take place...
    // For an accurate representation, there should be a multiple of 49*90000 ticks per second. (49 = 7*7; 90000*49==44100*100.)
    uint32_t durationIn90kHzTicks = 90000/*ticks/s*/ * SAMPLES_PER_FRAME / samplingFrequency/*samples/s*/; // -> Ticks per frame

    frame->m_duration.setAs90kHzTicks(durationIn90kHzTicks);

    // We're done if the selected parts of the header are the same: in that case the essential stream parameters did not change.
    if (m_silentAudioFrame.m_data.size() >= MIN_AC3_FRAME_SIZE &&
        fscod == m_sampleRateCode && acmod == m_audioCodingMode && lfeon == m_lfePresent && (frmsizecod & ~1) == (m_frameSizeCode & ~1)) {
        return;
    }

    // Cache our essential parameters
    m_sampleRateCode = fscod;
    m_audioCodingMode = acmod;
    m_frameSizeCode = frmsizecod;
    m_lfePresent = lfeon;

    const int nfchans = s_ac3ChannelsTable[acmod];

    RPLAYER_LOG_INFO("New AC-3 frame read: sf=%dHz, nChannels=%d, bitrate=%d, size=%d, bsid=%d, bsmod=%d, acmod=%d, lfeon=%d", samplingFrequency, nfchans, s_ac3BitrateTable[frmsizecod >> 1], frameSize, bsid, bsmod, acmod, lfeon);

    //
    // Generate silence frame
    //
    m_silentAudioFrame.m_data.clear();
    m_silentAudioFrame.m_data.resize(frameSize);

    BitWriter bitsOut(&m_silentAudioFrame.m_data[0], m_silentAudioFrame.m_data.size());

    // synchronization information
    bitsOut.write(syncword, 16);
    bitsOut.write(0, 16); // crc1 is computed later
    bitsOut.write(fscod, 2);
    bitsOut.write(frmsizecod, 6);

    // bit stream information
    bitsOut.write(bsid, 5);
    bitsOut.write(bsmod, 3);
    bitsOut.write(acmod, 3);
    if ((acmod & 0x1) && (acmod != 0x1)) { // If 3 front channels
        bitsOut.write(cmixlev, 2);
    }
    if (acmod & 0x4) { // If a surround channel exists
        bitsOut.write(surmixlev, 2);
    }
    if (acmod == 0x2) { // If in 2/0 mode
        bitsOut.write(0, 2); // dsurmod
    }
    bitsOut.write(lfeon, 1);
    bitsOut.write(dialnorm, 5);
    bitsOut.write(0, 1); // compre, no compression gain word
    bitsOut.write(0, 1); // langcode, no language code
    bitsOut.write(0, 1); // audprodie, no audio production information
    if (acmod == 0) { // If 1+1 mode (dual mono, so some items need a second value)
        bitsOut.write(dialnorm, 5); // dialnorm2
        bitsOut.write(0, 1); // compr2e, no compression gain word
        bitsOut.write(0, 1); // langcod2e, no language code
        bitsOut.write(0, 1); // audprodi2e, no audio production information
    }
    bitsOut.write(0, 1); // copyrightb, no copyright on silence!
    bitsOut.write(1, 1); // origbs, original bit stream
    bitsOut.write(0, 1); // timecod1e, no time code 1
    bitsOut.write(0, 1); // timecod2e, no time code 2
    bitsOut.write(0, 1); // addbsie, no additional bit stream information

    // Write beginning of first audio block
    for (int i = 0; i < nfchans; i++) {
        bitsOut.write(0, 1); // blksw
    }
    for (int i = 0; i < nfchans; i++) {
        bitsOut.write(0, 1); // dithflag
    }
    const int n = acmod == 0 ? 2 : 1;  // dual mono has these items twice
    for (int i = 0; i < n; i++) {
        bitsOut.write(0, 1);  // dynrnge
    }
    // first audio block must have a coupling strategy
    bitsOut.write(1, 1);  // cplstre
    bitsOut.write(0, 1); // cplinu
    if (acmod == 2) {
        bitsOut.write(1, 1); // rematstr
        for (int i = 0; i < 4; i++) {
            bitsOut.write(0, 1); // rematflg[i]
        }
    }
    // no cplexpstr
    for (int i = 0; i < nfchans; i++) {
        bitsOut.write(1, 2); // chexpstr[ch]
    }
    if (lfeon) {
        bitsOut.write(1, 1); // lfeexpstr
    }
    for (int i = 0; i < nfchans; i++) {
        bitsOut.write(0, 6); // chbwcod[ch]
    }
    // exponents
    const int exps[] = { 15, 124, 117, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62 };
    for (int i = 0; i < nfchans; i++) {
        //const int endmant[i] = chbwcod[i]*3 + 73;
        const int group_size = 3 << (1 /* chexpstr[i] */ - 1);
        const int nchgrps = (73 /*endmant[i]*/ + group_size-4) / group_size;

        bitsOut.write(exps[0], 4); // exps[ch][0]

        for (int grp = 1; grp <= nchgrps; grp++) {
            bitsOut.write(exps[grp], 7); // exps[ch][grp]
        }
        bitsOut.write(0, 2); // gainrng[ch]
    }
    if (lfeon) {
        bitsOut.write(exps[0], 4); // lfeexps[0]
        for (int grp = 1; grp <= 2; grp++) {
            bitsOut.write(exps[grp], 7); // lfeexps[grp]
        }
    }
    // first audio block must have bit-allocation parametric info
    bitsOut.write(1, 1); // baie
    bitsOut.write(0, 2); // sdcycod
    bitsOut.write(0, 2); // fdcycod
    bitsOut.write(0, 2); // sgaincod
    bitsOut.write(0, 2); // dbpbcod
    bitsOut.write(0, 3); // floorcod
    // first audio block must have SNR offset information
    bitsOut.write(1, 1); // snroffste
    bitsOut.write(0, 6); // csnroffst
    for (int i = 0; i < nfchans; i++) {
        bitsOut.write(0, 4); // fsnroffst[ch]
        bitsOut.write(0, 3); // fgaincod[ch]
    }
    if (lfeon) {
        bitsOut.write(0, 4); // lfefsnroffst
        bitsOut.write(0, 3); // lfefgaincod
    }
    bitsOut.write(0, 1); // deltbaie
    bitsOut.write(0, 1); // skiple

    // We leave all other fields 0. All coefficients of the first block will have 0 value (whatever that means)
    // and all following blocks will have all enable flags set to 0 and all other codes and coefficients set to
    // 0 as well. This seems to work allright...

    bitsOut.close();

    // Compute and fill in crc1
    unsigned framesize_5_8 = ((frameSize >> 2) + (frameSize >> 4)) << 1;
    uint16_t crc1 = reverseCrc16(&m_silentAudioFrame.m_data[2], framesize_5_8 - 2);
    m_silentAudioFrame.m_data[2] = crc1 >> 8;
    m_silentAudioFrame.m_data[3] = crc1 & 0xFF;

    // clear auxdatae and crcrsv
    m_silentAudioFrame.m_data[m_silentAudioFrame.m_data.size() - 3] &= 0xFC;

    // Compute and fill in crc2
    // NB: we may leave this out and set the final bytes to 0 because typically the
    // remaining 3/8 of the generated frame contains only zeros. In that case, crc1
    // will make sure that crc2 will end up being 0...
    uint16_t crc2 = crc16(&m_silentAudioFrame.m_data[2], frameSize - 4);
    m_silentAudioFrame.m_data[m_silentAudioFrame.m_data.size() - 2] = crc2 >> 8; // And fill in crc2
    m_silentAudioFrame.m_data[m_silentAudioFrame.m_data.size() - 1] = crc2 & 0xFF;

    m_silentAudioFrame.m_duration = frame->m_duration;

    RPLAYER_LOG_INFO("New AC-3 silence frame is %u bytes", static_cast<unsigned>(m_silentAudioFrame.m_data.size()));
}

Frame *Ac3FillerFrameCreator::create()
{
    if (m_silentAudioFrame.m_data.empty()) {
        return 0;
    }

    return new Frame(m_silentAudioFrame);
}
