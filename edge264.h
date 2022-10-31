/**
 * Copyright (c) 2013-2014, Celticom / TVLabs
 * Copyright (c) 2014-2022 Thibault Raffaillac <traf@kth.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of their
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef EDGE264_H
#define EDGE264_H

#include <stdint.h>

typedef struct {
	// The first 16 bytes uniquely determine the frame buffer size and format.
	int8_t chroma_format_idc; // 2 significant bits
	int8_t BitDepth_Y; // 4 significant bits
	int8_t BitDepth_C;
	int8_t max_dec_frame_buffering; // 5 significant bits
	uint16_t pic_width_in_mbs; // 10 significant bits
	int16_t pic_height_in_mbs; // 10 significant bits
	int16_t frame_crop_left_offset; // in luma samples
	int16_t frame_crop_right_offset; // in luma samples
	int16_t frame_crop_top_offset; // in luma samples
	int16_t frame_crop_bottom_offset; // in luma samples
	
	// private fields
	uint16_t qpprime_y_zero_transform_bypass_flag:1;
	uint16_t pic_order_cnt_type:2;
	uint16_t delta_pic_order_always_zero_flag:1; // pic_order_cnt_type==1
	uint16_t frame_mbs_only_flag:1;
	uint16_t mb_adaptive_frame_field_flag:1;
	uint16_t entropy_coding_mode_flag:1;
	uint16_t bottom_field_pic_order_in_frame_present_flag:1;
	uint16_t weighted_pred_flag:1;
	uint16_t deblocking_filter_control_present_flag:1;
	uint16_t constrained_intra_pred_flag:1;
	int8_t ChromaArrayType; // 2 significant bits
	int8_t log2_max_frame_num; // 5 significant bits
	int8_t log2_max_pic_order_cnt_lsb; // 5 significant bits, pic_order_cnt_type==0
	uint8_t num_ref_frames_in_pic_order_cnt_cycle; // pic_order_cnt_type==1
	int8_t max_num_ref_frames; // 5 significant bits
	int8_t max_num_reorder_frames; // 5 significant bits
	int8_t direct_8x8_inference_flag; // 1 significant bit
	int8_t num_ref_idx_active[2]; // 6 significant bits
	int8_t weighted_bipred_idc; // 2 significant bits
	int8_t QPprime_Y; // 7 significant bits
	int8_t chroma_qp_index_offset; // 5 significant bits
	int8_t transform_8x8_mode_flag; // 1 significant bit
	int8_t second_chroma_qp_index_offset; // 5 significant bits
	int16_t offset_for_non_ref_pic; // pic_order_cnt_type==1
	int16_t offset_for_top_to_bottom_field; // pic_order_cnt_type==1
	uint8_t weightScale4x4[6][16] __attribute__((aligned(16)));
	uint8_t weightScale8x8[6][64] __attribute__((aligned(16)));
} Edge264_parameter_set;


typedef struct Edge264_stream {
	// These fields must be set prior to decoding, the rest zeroed.
	const uint8_t *CPB; // should point to a NAL unit (after the 001 prefix)
	const uint8_t *end; // first byte past the end of the buffer
	
	// public read-only fields
	const uint8_t *samples_Y;
	const uint8_t *samples_Cb;
	const uint8_t *samples_Cr;
	int8_t pixel_depth_Y; // 0 for 8-bit, 1 for 16-bit
	int8_t pixel_depth_C;
	int16_t width_Y;
	int16_t width_C;
	int16_t height_Y;
	int16_t height_C;
	int16_t stride_Y;
	int16_t stride_C;
	
	// private fields
	uint8_t *DPB; // NULL before the first SPS is decoded
	int32_t plane_size_Y;
	int32_t plane_size_C;
	int32_t frame_size;
	uint32_t reference_flags; // bitfield for indices of reference frames
	uint32_t pic_reference_flags; // to be applied after decoding all slices of the current frame
	uint32_t long_term_flags; // bitfield for indices of long-term frames
	uint32_t pic_long_term_flags; // to be applied after decoding all slices of the current frame
	uint32_t output_flags; // bitfield for frames waiting to be output
	int8_t pic_idr_or_mmco5; // when set, all POCs will be decreased after completing the current frame
	int8_t currPic; // index of current incomplete frame, or -1
	int32_t pic_remaining_mbs; // when zero the picture is complete
	int32_t prevRefFrameNum;
	int32_t prevPicOrderCnt;
	int32_t dispPicOrderCnt;
	int32_t FrameNum[32];
	int8_t LongTermFrameIdx[32] __attribute__((aligned(16)));
	int8_t pic_LongTermFrameIdx[32] __attribute__((aligned(16))); // to be applied after decoding all slices of the current frame
	int32_t FieldOrderCnt[2][32] __attribute__((aligned(16))); // lower/higher half for top/bottom fields
	Edge264_parameter_set SPS;
	Edge264_parameter_set PPSs[4];
	int16_t PicOrderCntDeltas[256]; // too big to fit in Edge264_parameter_set
} Edge264_stream;


/**
 * Scan memory for the next three-byte 00n pattern, returning a pointer to the
 * first following byte (or end if no pattern was found).
 */
const uint8_t *Edge264_find_start_code(int n, const uint8_t *CPB, const uint8_t *end);


/**
 * Decode a single NAL unit, with e->CPB pointing at its first byte.
 * When the NAL is followed by a start code (for annex B streams), e->CPB will
 * be updated to point at the next unit.
 * 
 * Return codes are (negative if no NAL was consumed):
 * -2: end of buffer (e->CPB==e->end, so fetch some new data to proceed)
 * -1: DPB is full (more frames should be consumed before decoding can resume,
 *     only returned while decoding a picture NAL)
 *  0: success
 *  1: unsupported stream (decoding may proceed but could return zero frames)
 *  2: decoding error (decoding may proceed but could show visual artefacts,
 *     if you can validate with another decoder that the stream is correct,
 *     please consider filling a bug report, thanks!)
 */
int Edge264_decode_NAL(Edge264_stream *e);


/**
 * Fetch a decoded frame and store it in e->frame (pass drain=1 to extract all
 * remaining frames at the end of stream).
 * 
 * Return codes are:
 * -1: no frame ready for output
 *  0: success
 * 
 * Example code (single buffer in annex B format):
 *    Edge264_stream e = {.CPB=buffer_start, .end=buffer_end};
 *    while (1) {
 *       int res = Edge264_decode_NAL(&e);
 *       if (Edge264_get_frame(&e, res == -2) >= 0) // drain when reaching the end
 *          process_frame(&e, e->frame);
 *       else if (res == -2)
 *          break;
 *    }
 *    Edge264_clear(&e);
 */
int Edge264_get_frame(Edge264_stream *e, int drain);


/**
 * Free all internal memory and reset the structure to zero.
 */
void Edge264_clear(Edge264_stream *e);

#endif
