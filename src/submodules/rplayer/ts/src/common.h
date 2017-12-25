///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

/// ISO/IEC 13818-1:2007 Table 2-34 – Stream type assignments
enum TsProgramMapStreamType
{
    PMT_STREAM_TYPE_RESERVED    = 0x00,
    PMT_STREAM_TYPE_MPEG1_VIDEO = 0x01,
    PMT_STREAM_TYPE_MPEG2_VIDEO = 0x02,
    PMT_STREAM_TYPE_MPEG1_AUDIO = 0x03,
    PMT_STREAM_TYPE_MPEG2_AUDIO = 0x04,
    PMT_STREAM_TYPE_PRIVATE     = 0x05,
    PMT_STREAM_TYPE_AAC_AUDIO   = 0x0F,
    PMT_STREAM_TYPE_MPEG4_VIDEO = 0x10,
    PMT_STREAM_TYPE_H264_VIDEO  = 0x1B,
    PMT_STREAM_TYPE_AC3_AUDIO   = 0x81,
    PMT_STREAM_TYPE_LATENCY_DATA= 0xAF
};

static const int CA_DESCRIPTOR = 9; // Conditional Access descriptor
static const int ISO_639_LANGUAGE_DESCRIPTOR = 10; // ISO_639_language_descriptor
static const int PARTIAL_TRANSPORT_STREAM_DESCRIPTOR = 0x63; // partial_transport_stream_descriptor
static const int AC3_DESCRIPTOR = 0x6A; // AC-3_descriptor
static const int KEYFRAME_DESCRIPTOR = 0xFE; // ActiveVideo private key frame descriptor
static const char KEYFRAME_DESCRIPTOR_STRING[] = "KEY";
static const char KEYFRAME_DESCRIPTOR_STRING_LENGTH = 3;
static const uint8_t LATENCY_DATA_DESCRIPTOR_TAG = 0xF0;
static const char LATENCY_DATA_DESCRIPTOR_STRING[] = "AVLM";
static const int LATENCY_DATA_DESCRIPTOR_STRING_LENGTH = 4;

static const uint8_t TS_PACKET_SIZE = 188U;
static const uint8_t TS_MAX_PAYLOAD_SIZE = TS_PACKET_SIZE - 4;
static const uint8_t TS_SYNC_BYTE = 0x47;

static const int INVALID_PID = -1;
static const int PAT_PID = 0x0000;
static const int NULL_PACKET_PID = 0x1FFF;

static const int PAT_TABLE_ID = 0x00; // program_association_section
static const int PMT_TABLE_ID = 0x02; // program_map_section
static const int SIT_TABLE_ID = 0x7F; // selection_information_section

static const uint16_t CETS_CA_SYSTEM_ID = 0x6365; // 'ce'
static const uint32_t SCHM_SCHEME_TYPE = 0x63656E63; // 'cenc' (optional: 'cbc1' for AES-CBC encryption)
static const uint32_t SCHM_SCHEME_VERSION = 0x00010000; // Major 1, minor 0

// High byte is mask, low byte is value, and a flag signaling PES syntax
struct PesStreamId
{
    bool m_hasPesSyntax;
    uint8_t m_mask;
    uint8_t m_value;
};
static const PesStreamId PES_PRIVATE1_STREAM_ID = { true, 0xFF, 0xBD };
static const PesStreamId PES_PRIVATE2_STREAM_ID = { false, 0xFF, 0xBF };
static const PesStreamId PES_AUDIO_STREAM_ID = { true, 0xE0, 0xC0 };
static const PesStreamId PES_VIDEO_STREAM_ID = { true, 0xF0, 0xE0 };
static const PesStreamId PES_ECM_STREAM_ID = { false, 0xFF, 0xF0 };
static const PesStreamId PES_EMM_STREAM_ID = { false, 0xFF, 0xF1 };

uint32_t crc32_13818AnnexA(const uint8_t *data, int len);

} // namespace rplayer
