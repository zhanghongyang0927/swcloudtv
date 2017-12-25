///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "H264VideoFillerFrameCreator.h"
#include "Frame.h"

#include <rplayer/utils/Logger.h>
#include <rplayer/utils/H264Utils.h>
#include <rplayer/utils/H264SyntaxEncoder.h>
#include <rplayer/utils/H264SyntaxDecoder.h>

#include <algorithm>

#include <string.h>
#include <assert.h>

using namespace rplayer;

H264VideoFillerFrameCreator::H264VideoFillerFrameCreator()
{
    memset(&m_sps, 0, sizeof(SequenceParameterSet));
    memset(&m_pps, 0, sizeof(PictureParameterSet));
    memset(&m_sliceHeader, 0, sizeof(SliceHeader));

    m_sps.isValid = false;
    m_pps.isValid = false;
    m_sliceHeader.isValid = false;
}

H264VideoFillerFrameCreator::~H264VideoFillerFrameCreator()
{
}

void H264VideoFillerFrameCreator::processIncomingFrame(Frame *frame)
{
    const uint8_t *data = &frame->m_data[0];
    uint32_t sizeInBytes = frame->m_data.size();

    RPLAYER_LOG_DEBUG("Processing H264 frame");

    // The data-buffer received contain n complete NAL units.
    while (sizeInBytes > 0) {
        // Compute length of the next NAL unit in the bitstream
        uint32_t nalLength = h264d_annex_b_length(data, sizeInBytes);
        if (nalLength == 0) {
            RPLAYER_LOG_ERROR("Invalid nal unit (nal unit length = 0)");
            break;
        }

        // Process NAL unit. Extract and Update SPS/PPS syntax elements with the latest values
        processNalUnit(data, nalLength);

        // Update remaing bitstream size
        data += nalLength;
        sizeInBytes -= nalLength;
    }
}

Frame *H264VideoFillerFrameCreator::create()
{
    if (!m_sps.isValid || !m_pps.isValid) {
        RPLAYER_LOG_ERROR("No sps and/or pps. Can't generate a valid video filler-frame.");
        return 0;
    }
    return encodeEmptyPSlice();
}

Frame *H264VideoFillerFrameCreator::encodeEmptyPSlice(void)
{
    if (m_pps.entropy_coding_mode_flag) { // CABAC
        RPLAYER_LOG_ERROR("CABAC not supported yet. Can't generate a video filler-frame");
        return 0;
    }

    if (!m_sliceHeader.isValid) { // m_sliceHeader contains valid syntax elements values from last frame?
        RPLAYER_LOG_ERROR("A valid slice not received yet. Can't generate a video filler-frame");
        return 0;
    }

    const int maxFillerFrameNalSize = 512; // bytes
    uint8_t rbsp[maxFillerFrameNalSize] __attribute__ ((aligned(32)));
    H264SyntaxEncoder bitstream(&rbsp[0], maxFillerFrameNalSize);

    // Encode empty slice header syntax elements.
    bitstream.ue(0); // first_mb_in_slice
    bitstream.ue(H264_P_SLICE); // slic_type
    bitstream.ue(m_pps.pic_parameter_set_id); //pic_parameter_set_id
    bitstream.u(m_sliceHeader.frame_num, m_sps.log2_max_frame_num_minus4 + 4); // encode frame_num from last frame received
    if (m_sps.pic_order_cnt_type == 0) {
        bitstream.u(m_sliceHeader.pic_order_cnt_lsb, m_sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
        if (m_pps.pic_order_present_flag /*&& !field_pic_flag*/) {
            bitstream.se(m_sliceHeader.delta_pic_order_cnt[0]);
        }
    } else if (m_sps.pic_order_cnt_type == 1 && !m_sps.delta_pic_order_always_zero_flag) {
        bitstream.se(m_sliceHeader.delta_pic_order_cnt[0]);
        if (m_pps.pic_order_present_flag /*&& !field_pic_flag*/) {
            bitstream.se(m_sliceHeader.delta_pic_order_cnt[1]);
        }
    }

    const uint8_t numRefIdxL0ActiveMinus1 = 0; // 1 ref frame
    bitstream.u(1, 1); // num_ref_idx_active_override_flag
    bitstream.ue(numRefIdxL0ActiveMinus1);
    bitstream.u(0, 1); // ref_pic_list_reordering_flag_l0
    if (m_pps.weighted_pred_flag) {
        bitstream.ue(0); // luma_log2_weight_denom
        bitstream.ue(0); // chroma_log2_weight_denom
        for (int i = 0; i <= numRefIdxL0ActiveMinus1; i++) {
            bitstream.u(0, 1); // luma_weight_l0_flag
            bitstream.u(0, 1); // chroma_weight_l0_flag
        }
    }

    const unsigned nalRefIdc = H264_NAL_REF_IDC_HIGH;
    if (nalRefIdc != 0) {
        bitstream.u(0, 1); // adaptive_ref_pic_marking_mode_flag
    }
    if (m_pps.entropy_coding_mode_flag) {
        bitstream.ue(m_sliceHeader.cabac_init_idc);
    }
    bitstream.se(0); // slice_qp_delta
    if (m_pps.deblocking_filter_control_present_flag) {
        bitstream.ue(1); // disable_deblocking_filter_idc
    }

    //Append slice data
    //
    // This ignores field pictures
    // assert(field_pic_flag == 0);
    // assert(frame_mbs_only_flag == 1)
    const int picHeightInMbs = m_sps.pic_height_in_map_units_minus1 + 1;
    const int picWidthInMbs = m_sps.pic_width_in_mbs_minus1 + 1;
    const int picSizeInMbs = picWidthInMbs * picHeightInMbs;
    if (m_pps.entropy_coding_mode_flag) { // CABAC
        // To do: CABAC
        RPLAYER_LOG_ERROR("CABAC not supported yet.");
    } else {  // CAVLC
        RPLAYER_LOG_DEBUG("Generating H264 filler frame using CAVLC");
        // Skip all blocks
        bitstream.ue(picSizeInMbs); // mb_skip_run
        bitstream.u(1, 1); // stop bits
        bitstream.align();
        bitstream.close();
    }

    // We are done with encoding, check if there was any error while generating this filler frame
    if (bitstream.hasError()) {
        RPLAYER_LOG_ERROR("Bitstream error while encoding. Can't generate filler frame");
        return 0;
    }

    // Create nals
    const unsigned audNalSize = 6; // bytes
    const uint8_t audNal[audNalSize] = { 0x00, 0x00, 0x00, 0x01, (H264_NAL_REF_IDC_DISPOSABLE << 5 | H264_NAL_UNIT_TYPE_AUD), 0x30 /* P-frame */};

    uint8_t sliceNal[maxFillerFrameNalSize];
    unsigned sliceNalLength = h264e_annex_b_escape(3, nalRefIdc, H264_NAL_UNIT_TYPE_SLICE, (const uint32_t *)rbsp, bitstream.getNBytesWritten(), sliceNal);

    // Create frame
    Frame *frame = new Frame();
    frame->m_data.reserve(sliceNalLength + 6);

    // Insert AUD Nal and filler-slice NAL
    frame->m_data.insert(frame->m_data.end(), audNal, audNal + audNalSize);
    frame->m_data.insert(frame->m_data.end(), sliceNal, sliceNal + sliceNalLength);

    return frame;
}

void H264VideoFillerFrameCreator::processNalUnit(const uint8_t *data, uint32_t nalLength)
{
    unsigned unitType, refIdc, startCodeLength;
    uint32_t length = h264d_annex_b_header(&startCodeLength, &refIdc, &unitType, data, nalLength);
    if (length == 0) {
        RPLAYER_LOG_ERROR("Invalid nal unit with no data bytes ");
        return;
    }

    uint32_t buffer[64]; // Enough buffer length to accomodate single SPS/PPS/Slice header. No VCL data parsing beyond slice header required
    uint32_t inputLength = std::min(nalLength - length, static_cast<uint32_t>(sizeof(buffer)));
    length = h264d_annex_b_unescape(data + length, inputLength, buffer);
    if (length == 0) {
        RPLAYER_LOG_ERROR("Invalid nal unit with no data bytes");
        return;
    }

    switch (unitType) {
    case H264_NAL_UNIT_TYPE_SPS:
        parseSpsHeader((const uint8_t*)&buffer, length);
        break;

    case H264_NAL_UNIT_TYPE_PPS:
        parsePpsHeader((const uint8_t*)&buffer, length);
        break;

    case H264_NAL_UNIT_TYPE_SLICE:
    case H264_NAL_UNIT_TYPE_IDR:
        if (m_sps.isValid && m_pps.isValid) {
            parseSliceHeader((const uint8_t*)&buffer, length, unitType);
        }
        break;

    default:
        break;
    }
}

void H264VideoFillerFrameCreator::parseSpsHeader(const uint8_t *data, uint32_t nalLength)
{
    //Reset sps structure
    memset(&m_sps, 0, sizeof(SequenceParameterSet));
    m_sps.isValid = false;

    //Set pps false too.
    m_pps.isValid = false;

    H264SyntaxDecoder parser(data, nalLength, 0);

    //Parse SPS header syntax elements
    //m_sps.profile_idc = h264d_u_v(scanner, 8);
    m_sps.profile_idc = parser.u(8);

    // constraint_set0_flag
    // constraint_set1_flag
    // constraint_set2_flag
    // constraint_set3_flag
    // reserved_zero_4bits
    // level_idc
    parser.u_skip(16);

    m_sps.seq_parameter_set_id = parser.ue();

    if (m_sps.profile_idc == 100 || m_sps.profile_idc == 110 || m_sps.profile_idc == 122 || m_sps.profile_idc == 144) {
        if (parser.ue() != 1) { // chroma_format_idc
            RPLAYER_LOG_ERROR("Unsupported chroma_format_idc");
            return;
        }

        parser.ue_skip(); // bit_depth_luma_minus8
        parser.ue_skip(); // bit_depth_chroma_minus8
        parser.u_skip(1); // qpprime_y_zero_transform_bypass_flag
        if (parser.u(1)) { // seq_scaling_matrix_present_flag
            RPLAYER_LOG_ERROR("Unsupported seq_scaling_matrix_present_flag");
            return;
        }
    }

    m_sps.log2_max_frame_num_minus4 = parser.ue();
    m_sps.pic_order_cnt_type = parser.ue();
    if (m_sps.pic_order_cnt_type == 0) {
        m_sps.log2_max_pic_order_cnt_lsb_minus4 = parser.ue();
    } else if (m_sps.pic_order_cnt_type == 1) {
        m_sps.delta_pic_order_always_zero_flag = parser.u(1);
        parser.se_skip(); // offset_for_non_ref_pic
        parser.se_skip(); // offset_for_top_to_bottom_field

        unsigned num_ref_frames_in_pic_order_cnt_cycle = parser.ue();
        for (unsigned i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
            parser.se_skip(); // offset_for_ref_frame[i]
        }
    }

    parser.ue_skip(); // num_ref_frames
    parser.u_skip(1); // gaps_in_frame_num_value_allowed_flag

    m_sps.pic_width_in_mbs_minus1 = parser.ue();
    m_sps.pic_height_in_map_units_minus1 = parser.ue();

    // set sps is valid, if there was no error during parsing bitstream for SPS
    if (parser.hasError()) {
        RPLAYER_LOG_ERROR("error while parsing bitstream for sps");
    } else {
        m_sps.isValid = true;
    }
}

void H264VideoFillerFrameCreator::parsePpsHeader(const uint8_t *data, uint32_t nalLength)
{
    //Reset sps structure
    memset(&m_pps, 0, sizeof(PictureParameterSet));
    m_pps.isValid = false;

    H264SyntaxDecoder parser(data, nalLength, 0);

    //Parse PPS header syntax elements
    m_pps.pic_parameter_set_id = parser.ue();
    m_pps.seq_parameter_set_id = parser.ue();
    m_pps.entropy_coding_mode_flag = parser.u(1);
    m_pps.pic_order_present_flag = parser.u(1);

    if (parser.ue() > 0) {
        RPLAYER_LOG_ERROR("slice groups not supported");
        return;
    }

    m_pps.num_ref_idx_l0_active_minus1 = parser.ue();

    parser.ue_skip(); // num_ref_idx_l1_active_minus1
    m_pps.weighted_pred_flag = parser.u(1);
    parser.u_skip(2); // weighted_bipred_idc
    m_pps.pic_init_qp_minus26 = parser.se();
    parser.se_skip(); // pic_init_qs_minus26
    parser.se_skip(); // chroma_qp_index_offset
    m_pps.deblocking_filter_control_present_flag = parser.u(1);

    // set PPS is valid, if there was no error during parsing bitstream for PPS
    if (parser.hasError()) {
        RPLAYER_LOG_ERROR("error while parsing bitstream for pps");
    } else {
        m_pps.isValid = true;
    }
}

void H264VideoFillerFrameCreator::parseSliceHeader(const uint8_t *data, uint32_t nalLength, int nalUnitType)
{
    //Reset slice header structure
    memset(&m_sliceHeader, 0, sizeof(SliceHeader));
    m_sliceHeader.isValid = false;

    H264SyntaxDecoder parser(data, nalLength, 0);

    parser.ue_skip(); // first_mb_in_slice
    parser.ue_skip(); // slice_type

    int pic_parameter_set_id = parser.ue();
    if (pic_parameter_set_id != m_pps.pic_parameter_set_id) {
        RPLAYER_LOG_ERROR("Slice header pic_parameter_set_id refers to unavailable pps (sliceHeader.pps_id=%d pps_id=%d)", pic_parameter_set_id, m_pps.pic_parameter_set_id);
        return;
    }
    if (m_pps.seq_parameter_set_id != m_sps.seq_parameter_set_id) {
        RPLAYER_LOG_ERROR("PPS seq_parameter_set_id refers to unavailable sps (pps.sps_id=%d sps_id=%d)", m_pps.seq_parameter_set_id, m_sps.seq_parameter_set_id);
        return;
    }

    m_sliceHeader.frame_num = parser.u(m_sps.log2_max_frame_num_minus4 + 4);
    if (nalUnitType == 5) {
        parser.ue_skip(); // idr_pic_id
    }
    if (m_sps.pic_order_cnt_type == 0) {
        m_sliceHeader.pic_order_cnt_lsb = parser.u(m_sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
        if (m_pps.pic_order_present_flag /*&& !field_pic_flag*/) {
            m_sliceHeader.delta_pic_order_cnt[0] = parser.se();
        }
    } else if (m_sps.pic_order_cnt_type == 1 && !m_sps.delta_pic_order_always_zero_flag) {
        m_sliceHeader.delta_pic_order_cnt[0] = parser.se();
        if (m_pps.pic_order_present_flag /*&& !field_pic_flag*/) {
            m_sliceHeader.delta_pic_order_cnt[1] = parser.se();
        }
    }

    // set SH is valid, if there was no error during parsing bitstream for SH
    if (parser.hasError()) {
        RPLAYER_LOG_ERROR("error while parsing bitstream for slice header");
    } else {
        m_sliceHeader.isValid = true;
    }

}
