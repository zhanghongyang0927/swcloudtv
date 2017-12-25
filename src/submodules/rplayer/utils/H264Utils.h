///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///
#pragma once

#include <inttypes.h>

// 7.4.1 NAL unit semantics
const unsigned H264_NAL_REF_IDC_HIGHEST = 3;
const unsigned H264_NAL_REF_IDC_HIGH = 2;
const unsigned H264_NAL_REF_IDC_LOW = 1;
const unsigned H264_NAL_REF_IDC_DISPOSABLE = 0;

// Table 7-1 - NAL unit type codes
const unsigned H264_NAL_UNIT_TYPE_SLICE = 1;
const unsigned H264_NAL_UNIT_TYPE_DPA = 2;
const unsigned H264_NAL_UNIT_TYPE_DPB = 3;
const unsigned H264_NAL_UNIT_TYPE_DPC = 4;
const unsigned H264_NAL_UNIT_TYPE_IDR = 5;
const unsigned H264_NAL_UNIT_TYPE_SEI = 6;
const unsigned H264_NAL_UNIT_TYPE_SPS = 7;
const unsigned H264_NAL_UNIT_TYPE_PPS = 8;
const unsigned H264_NAL_UNIT_TYPE_AUD = 9;
const unsigned H264_NAL_UNIT_TYPE_EOSEQ = 10;
const unsigned H264_NAL_UNIT_TYPE_EOSTREAM = 11;
const unsigned H264_NAL_UNIT_TYPE_FILL = 12;
const unsigned H264_NAL_UNIT_TYPE_META = 31;

// Table 7-6 - Name association to slice_type
const unsigned H264_P_SLICE = 0;
const unsigned H264_B_SLICE = 1;
const unsigned H264_I_SLICE = 2;
const unsigned H264_SP_SLICE = 3;
const unsigned H264_SI_SLICE = 4;

uint32_t countLeadingZeros(uint32_t pattern);

//Annex B: Encoding
unsigned h264e_annex_b_escape(unsigned prefixLength, unsigned nal_ref_idc, unsigned nal_unit_type, const uint32_t *__restrict rbsp, unsigned bytes, void *__restrict escaped);

//Annex B: Decoding
unsigned h264d_annex_b_length(const uint8_t *__restrict escaped, unsigned bytes);
unsigned h264d_annex_b_header(unsigned *prefixLength, unsigned *nal_ref_idc, unsigned *nal_unit_type, const uint8_t *__restrict escaped, unsigned bytes);
unsigned h264d_annex_b_unescape(const uint8_t *escaped, unsigned bytes, uint32_t *rbsp);
