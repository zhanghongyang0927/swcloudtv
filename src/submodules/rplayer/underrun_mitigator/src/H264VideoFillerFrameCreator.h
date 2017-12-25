///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IFillerFrameCreator.h"

#include <inttypes.h>

struct avctk_scanner_t;

namespace rplayer {

enum NalUnitType
{
    NAL_UNIT_TYPE_Unspecified = 0,
    NAL_UNIT_TYPE_CODED_SLICE_NON_IDR,
    NAL_UNIT_TYPE_CODED_SLICE_DATA_PART_A,
    NAL_UNIT_TYPE_CODED_SLICE_DATA_PART_B,
    NAL_UNIT_TYPE_CODED_SLICE_DATA_PART_C,
    NAL_UNIT_TYPE_CODED_SLICE_IDR,
    NAL_UNIT_TYPE_SEI,
    NAL_UNIT_TYPE_SPS,
    NAL_UNIT_TYPE_PPS,
    NAL_UNIT_TYPE_AUD,
    NAL_UNIT_TYPE_END_SEQUENCE,
    NAL_UNIT_TYPE_END_STREAM,
    NAL_UNIT_TYPE_FILLER_DATA,
    NAL_UNIT_TYPE_SPS_EXT,
    NAL_UNIT_TYPE_META = 31
};

enum SliceType
{
    SLICE_TYPE_P      = 0,
    SLICE_TYPE_B      = 1,
    SLICE_TYPE_I      = 2,
    SLICE_TYPE_SP     = 3,
    SLICE_TYPE_SI     = 4,
    SLICE_TYPE_P_ALL  = 5,
    SLICE_TYPE_B_ALL  = 6,
    SLICE_TYPE_I_ALL  = 7,
    SLICE_TYPE_SP_ALL = 8,
    SLICE_TYPE_SI_ALL = 9
};

struct SequenceParameterSet
{
    bool isValid;

    int profile_idc;
    int constraint_set0_flag;
    int constraint_set1_flag;
    int constraint_set2_flag;
    int constraint_set3_flag;
    int reserved_zero_4bits;
    int level_idc;
    int seq_parameter_set_id;
    int chroma_format_idc;
    int residual_colour_transform_flag;
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    int qpprime_y_zero_transform_bypass_flag;
    int seq_scaling_matrix_present_flag;
    int seq_scaling_list_present_flag[8];
    int log2_max_frame_num_minus4;
    int pic_order_cnt_type;
    int log2_max_pic_order_cnt_lsb_minus4;
    int delta_pic_order_always_zero_flag;
    int offset_for_non_ref_pic;
    int offset_for_top_to_bottom_field;
    int num_ref_frames_in_pic_order_cnt_cycle;
    int offset_for_ref_frame[256];
    int num_ref_frames;
    int gaps_in_frame_num_value_allowed_flag;
    int pic_width_in_mbs_minus1;
    int pic_height_in_map_units_minus1;
    int frame_mbs_only_flag;
    int mb_adaptive_frame_field_flag;
    int direct_8x8_inference_flag;
    int frame_cropping_flag;
    int frame_crop_left_offset;
    int frame_crop_right_offset;
    int frame_crop_top_offset;
    int frame_crop_bottom_offset;
    int vui_parameters_present_flag;
    int aspect_ratio_info_present_flag;
    int aspect_ratio_idc;
    int sar_width;
    int sar_height;
    int overscan_info_present_flag;
    int overscan_appropriate_flag;
    int video_signal_type_present_flag;
    int video_format;
    int video_full_range_flag;
    int colour_description_present_flag;
    int colour_primaries;
    int transfer_characteristics;
    int matrix_coefficients;
    int chroma_loc_info_present_flag;
    int chroma_sample_loc_type_top_field;
    int chroma_sample_loc_type_bottom_field;
    int timing_info_present_flag;
    int num_units_in_tick;
    int time_scale;
    int fixed_frame_rate_flag;
    int nal_hrd_parameters_present_flag;
    int vcl_hrd_parameters_present_flag;
    struct
    {
        int cpb_cnt_minus1;
        int bit_rate_scale;
        int cpb_size_scale;
        int bit_rate_value_minus1[32];
        int cpb_size_value_minus1[32];
        int cbr_flag[32];
        int initial_cpb_removal_delay_length_minus1;
        int cpb_removal_delay_length_minus1;
        int dpb_output_delay_length_minus1;
        int time_offset_length;
    } nal_hrd, vcl_hrd;
    int low_delay_hrd_flag;
    int pic_struct_present_flag;
    int bitstream_restriction_flag;
    int motion_vectors_over_pic_boundaries_flag;
    int max_bytes_per_pic_denom;
    int max_bits_per_mb_denom;
    int log2_max_mv_length_horizontal;
    int log2_max_mv_length_vertical;
    int num_reorder_frames;
    int max_dec_frame_buffering;
};

struct PictureParameterSet
{
    bool isValid;

    int pic_parameter_set_id;
    int seq_parameter_set_id;
    int entropy_coding_mode_flag;
    int pic_order_present_flag;
    int num_slice_groups_minus1;
    int slice_group_map_type;
    int run_length_minus1[8];
    int top_left[8];
    int bottom_right[8];
    int slice_group_change_direction_flag;
    int slice_group_change_rate_minus1;
    int pic_size_in_map_units_minus1;
    int *slice_group_id;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
    int weighted_pred_flag;
    int weighted_bipred_idc;
    int pic_init_qp_minus26;
    int pic_init_qs_minus26;
    int chroma_qp_index_offset;
    int deblocking_filter_control_present_flag;
    int constrained_intra_pred_flag;
    int redundant_pic_cnt_present_flag;
    int transform_8x8_mode_flag;
    int pic_scaling_matrix_present_flag;
    int pic_scaling_list_present_flag[8];
    int second_chroma_qp_index_offset;

    // Defaults used when no pps is available. Should not really be
    // defined here.
    const static int dfcpf_default = 0;
    const static int dipf_default = 1;
};

struct SliceHeader
{
    static const int MAX_MEM_CONTROL = 16;

    bool isValid;

    // NAL INFO
    unsigned nal_ref_idc;
    NalUnitType nal_unit_type;

    unsigned first_mb_in_slice;
    SliceType slice_type;
    unsigned pic_parameter_set_id;
    unsigned frame_num;
    unsigned field_pic_flag;
    unsigned bottom_field_flag;
    unsigned idr_pic_id;
    unsigned pic_order_cnt_lsb;
    int delta_pic_order_cnt_bottom;
    int delta_pic_order_cnt[2];
    unsigned redundant_pic_cnt;
    unsigned direct_spatial_mv_pred_flag;
    unsigned num_ref_idx_active_override_flag;
    unsigned num_ref_idx_l0_active_minus1;
    unsigned num_ref_idx_l1_active_minus1;
    unsigned ref_pic_list_reordering_flag_l0;
    unsigned reordering_of_pic_nums_idc;
    unsigned abs_diff_pic_num_minus1;
    unsigned long_term_pic_num;
    unsigned ref_pic_list_reordering_flag_l1;
    unsigned luma_log2_weight_denom;
    unsigned chroma_log2_weight_denom;
    unsigned luma_weight_l0_flag;
    unsigned luma_weight_l0[32];
    unsigned luma_offset_l0[32];
    unsigned chroma_weight_l0_flag;
    unsigned chroma_weight_l0[32][2];
    unsigned chroma_offset_l0[32][2];
    unsigned luma_weight_l1_flag;
    unsigned luma_weight_l1[32];
    unsigned luma_offset_l1[32];
    unsigned chroma_weight_l1_flag;
    unsigned chroma_weight_l1[32][2];
    unsigned chroma_offset_l1[32][2];
    unsigned no_output_of_prior_pics_flag;
    unsigned long_term_reference_flag;
    unsigned adaptive_ref_pic_marking_mode_flag;
    unsigned memory_management_control_op[MAX_MEM_CONTROL];
    unsigned memory_management_control_value[2 * MAX_MEM_CONTROL];
    unsigned difference_of_pic_nums_minus1;
    unsigned long_term_frame_idx;
    unsigned max_long_term_frame_idx_plus1;
    unsigned cabac_init_idc;
    int slice_qp_delta;
    unsigned sp_for_switch_flag;
    int slice_qs_delta;
    unsigned disable_deblocking_filter_idc;
    unsigned slice_alpha_c0_offset_div2;
    unsigned slice_beta_offset_div2;
};

class H264VideoFillerFrameCreator: public IFillerFrameCreator
{
public:

    H264VideoFillerFrameCreator();
    ~H264VideoFillerFrameCreator();
    StreamType getStreamType()
    {
        return STREAM_TYPE_H264_VIDEO;
    }

    void processIncomingFrame(Frame *frame);
    Frame *create();

private:
    H264VideoFillerFrameCreator(const H264VideoFillerFrameCreator&);
    H264VideoFillerFrameCreator &operator=(const H264VideoFillerFrameCreator &);

    void processNalUnit(const uint8_t *data, uint32_t nalLength);
    void parseSpsHeader(const uint8_t *data, uint32_t nalLength);
    void parsePpsHeader(const uint8_t *data, uint32_t nalLength);
    void parseSliceHeader(const uint8_t *data, uint32_t nalLength, int nalUnitType);
    Frame *encodeEmptyPSlice(void);

    SequenceParameterSet m_sps;
    PictureParameterSet m_pps;
    SliceHeader m_sliceHeader;
};
} // namespace
