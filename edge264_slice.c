#include "edge264_common.h"



static const v16qi sig_inc_4x4 =
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const v16qi sig_inc_8x8[2][4] = {{
	{ 0,  1,  2,  3,  4,  5,  5,  4,  4,  3,  3,  4,  4,  4,  5,  5},
	{ 4,  4,  4,  4,  3,  3,  6,  7,  7,  7,  8,  9, 10,  9,  8,  7},
	{ 7,  6, 11, 12, 13, 11,  6,  7,  8,  9, 14, 10,  9,  8,  6, 11},
	{12, 13, 11,  6,  9, 14, 10,  9, 11, 12, 13, 11, 14, 10, 12,  0},
	}, {
	{ 0,  1,  1,  2,  2,  3,  3,  4,  5,  6,  7,  7,  7,  8,  4,  5},
	{ 6,  9, 10, 10,  8, 11, 12, 11,  9,  9, 10, 10,  8, 11, 12, 11},
	{ 9,  9, 10, 10,  8, 11, 12, 11,  9,  9, 10, 10,  8, 13, 13,  9},
	{ 9, 10, 10,  8, 13, 13,  9,  9, 10, 10, 14, 14, 14, 14, 14,  0},
}};
static const v16qi last_inc_8x8[4] = {
	{0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4},
	{5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8},
};
static const v8qi sig_inc_chromaDC[2] =
	{{0, 1, 2, 0}, {0, 0, 1, 1, 2, 2, 2, 0}};

// transposed scan tables
static const v16qi scan_4x4[2] = {
	{0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15},
	{0, 1, 4, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const v16qi scan_8x8[2][4] = {{
	{ 0,  8,  1,  2,  9, 16, 24, 17, 10,  3,  4, 11, 18, 25, 32, 40},
	{33, 26, 19, 12,  5,  6, 13, 20, 27, 34, 41, 48, 56, 49, 42, 35},
	{28, 21, 14,  7, 15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30},
	{23, 31, 38, 45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63},
	}, {
	{ 0,  1,  2,  8,  9,  3,  4, 10, 16, 11,  5,  6,  7, 12, 17, 24},
	{18, 13, 14, 15, 19, 25, 32, 26, 20, 21, 22, 23, 27, 33, 40, 34},
	{28, 29, 30, 31, 35, 41, 48, 42, 36, 37, 38, 39, 43, 49, 50, 44},
	{45, 46, 47, 51, 56, 57, 52, 53, 54, 55, 58, 59, 60, 61, 62, 63},
}};
static const v8qi scan_chromaDC[2] =
	{{0, 1, 2, 3}, {0, 2, 1, 4, 6, 3, 5, 7}};

static const v4hi ctxIdxOffsets_16x16DC[3][2] = {
	{{85, 105, 166, 227}, {85, 277, 338, 227}}, // ctxBlockCat==0
	{{460, 484, 572, 952}, {460, 776, 864, 952}}, // ctxBlockCat==6
	{{472, 528, 616, 982}, {472, 820, 908, 982}}, // ctxBlockCat==10
};
static const v4hi ctxIdxOffsets_16x16AC[3][2] = {
	{{89, 119, 180, 237}, {89, 291, 352, 237}}, // ctxBlockCat==1
	{{464, 498, 586, 962}, {464, 790, 878, 962}}, // ctxBlockCat==7
	{{476, 542, 630, 992}, {476, 834, 922, 992}}, // ctxBlockCat==11
};
static const v4hi ctxIdxOffsets_chromaDC[2] =
	{{97, 149, 210, 257}, {97, 321, 382, 257}}; // ctxBlockCat==3
static const v4hi ctxIdxOffsets_chromaAC[2] =
	{{101, 151, 212, 266}, {101, 323, 384, 266}}; // ctxBlockCat==4
static const v4hi ctxIdxOffsets_4x4[3][2] = {
	{{93, 134, 195, 247}, {93, 306, 367, 247}}, // ctxBlockCat==2
	{{468, 528, 616, 972}, {468, 805, 893, 972}}, // ctxBlockCat==8
	{{480, 557, 645, 1002}, {480, 849, 937, 1002}}, // ctxBlockCat==12
};
static const v4hi ctxIdxOffsets_8x8[3][2] = {
	{{1012, 402, 417, 426}, {1012, 436, 451, 426}}, // ctxBlockCat==5
	{{1016, 660, 690, 708}, {1016, 675, 699, 708}}, // ctxBlockCat==9
	{{1020, 718, 748, 766}, {1020, 733, 757, 766}}, // ctxBlockCat==13
};



/**
 * This function parses a group of significant_flags, then the corresponding
 * sequence of coeff_abs_level_minus1/coeff_sign_flag pairs (9.3.2.3).
 *
 * Bypass bits can be extracted all at once using a binary division (!!).
 * coeff_abs_level expects at most 2^(7+14), i.e 43 bits as Exp-Golomb, so we
 * use two 32bit divisions (second one being executed for long codes only).
 */
static __attribute__((noinline)) void FUNC(parse_residual_block, unsigned coded_block_flag, int startIdx, int endIdx)
{
	// Sharing this test here should limit branch predictor cache pressure.
	if (!coded_block_flag)
		JUMP(decode_samples);
	
	// significant_coeff_flags are stored as a bit mask
	uint64_t significant_coeff_flags = 0;
	int i = startIdx;
	do {
		if (CALL(get_ae, ctx->ctxIdxOffsets[1] + ctx->sig_inc[i])) {
			significant_coeff_flags |= (uint64_t)1 << i;
			if (CALL(get_ae, ctx->ctxIdxOffsets[2] + ctx->last_inc[i]))
				break;
		}
	} while (++i < endIdx);
	significant_coeff_flags |= (uint64_t)1 << i;
	ctx->significant_coeff_flags = significant_coeff_flags;
	
	// Now loop on set bits to parse all non-zero coefficients.
	int ctxIdx0 = ctx->ctxIdxOffsets[3] + 1;
	int ctxIdx1 = ctx->ctxIdxOffsets[3] + 5;
	do {
		int coeff_level = 1;
		int ctxIdx = ctxIdx0;
		while (coeff_level < 15 && CALL(get_ae, ctxIdx))
			coeff_level++, ctxIdx = ctxIdx1;
	
		// Unsigned division uses one extra bit, so the first renorm is correct.
		if (coeff_level >= 15) {
			CALL(renorm, 0, 0); // Hardcore!!!
			codIRange >>= SIZE_BIT - 9;
			uint32_t num = codIOffset >> (SIZE_BIT - 32);
			uint32_t quo = num / (uint32_t)codIRange;
			uint32_t rem = num % (uint32_t)codIRange;
		
			// 32bit/9bit division yields 23 bypass bits.
			int k = clz32(~(quo << 9));
			unsigned shift = 9 + k;
			if (__builtin_expect(k >= 12, 0)) { // At k==11, code length is 23 bits.
				codIRange <<= SIZE_BIT - 32;
				codIOffset = (SIZE_BIT == 32) ? rem : (uint32_t)codIOffset | (size_t)rem << 32;
				CALL(renorm, 2, 0); // Next division will yield 21 bypass bits...
				codIRange >>= SIZE_BIT - 11;
				num = codIOffset >> (SIZE_BIT - 32);
				quo = num / (uint32_t)codIRange + (quo << 21); // ... such that we keep 11 as msb.
				rem = num % (uint32_t)codIRange;
				shift -= 21;
			}
		
			// Return the unconsumed bypass bits to codIOffset, and compute coeff_level.
			size_t mul = ((uint32_t)-1 >> 1 >> (shift + k) & quo) * (uint32_t)codIRange + rem;
			codIRange <<= SIZE_BIT - 1 - (shift + k);
			codIOffset = (SIZE_BIT == 32) ? mul : (uint32_t)codIOffset | mul << 32;
			coeff_level = 14 + (1 << k) + (quo << shift >> (31 - k));
		}
		
		// not the brightest part of spec (9.3.3.1.3), I did my best
		static const int8_t trans[5] = {0, 2, 3, 4, 4};
		int last_sig_offset = ctx->ctxIdxOffsets[3];
		int ctxIdxInc = trans[ctxIdx0 - last_sig_offset];
		ctxIdx0 = last_sig_offset + (coeff_level > 1 ? 0 : ctxIdxInc);
		ctxIdx1 = min(ctxIdx1 + (coeff_level > 1), (last_sig_offset == 257 ? last_sig_offset + 8 : last_sig_offset + 9));
	
		// parse coeff_sign_flag
		codIRange >>= 1;
		int c = (codIOffset >= codIRange) ? -coeff_level : coeff_level;
		codIOffset = (codIOffset >= codIRange) ? codIOffset - codIRange : codIOffset;
		
		// scale and store
		int i = 63 - clz64(significant_coeff_flags);
		int scan = ctx->scan[i]; // beware, scan is transposed already
		ctx->d[scan] = (c * ctx->LevelScale[scan] + 32) >> 6; // cannot overflow since spec says result is 22 bits
		significant_coeff_flags &= ~((uint64_t)1 << i);
		fprintf(stderr, "coeffLevel[%d]: %d\n", i - startIdx, c);
		// fprintf(stderr, "coeffLevel[%d](%d): %d\n", i - startIdx, ctx->BlkIdx, c);
	} while (significant_coeff_flags != 0);
	JUMP(decode_samples);
}



/**
 * This function parses one mvd_lX symbol, using the same binary division trick
 * as above to read all bypass bits at once.
 */
static __attribute__((noinline)) int FUNC(parse_mvd, int pos, int ctxBase) {
	int absMvdCompA = *(mb->absMvdComp + (pos & 48) + ctx->absMvdComp_A[pos & 15]);
	int absMvdCompB = *(mb->absMvdComp + (pos & 48) + ctx->absMvdComp_B[pos & 15]);
	int sum = absMvdCompA + absMvdCompB;
	int ctxIdx = ctxBase + (sum >= 3) + (sum > 32);
	int mvd = 0;
	ctxBase += 3;
	while (mvd < 9 && CALL(get_ae, ctxIdx))
		ctxIdx = ctxBase + min(mvd++, 3);
	
	// Once again, we use unsigned division to read all bypass bits.
	if (mvd >= 9) {
		CALL(renorm, 0, 0);
		codIRange >>= SIZE_BIT - 9;
		uint32_t num = codIOffset >> (SIZE_BIT - 32);
		uint32_t quo = num / (uint32_t)codIRange;
		uint32_t rem = num % (uint32_t)codIRange;
		
		// 32bit/9bit division yields 23 bypass bits.
		int k = 3 + clz32(~(quo << 9));
		unsigned shift = 6 + k;
		if (__builtin_expect(k >= 13, 0)) { // At k==12, code length is 22 bits.
			codIRange <<= (SIZE_BIT - 32);
			codIOffset = (SIZE_BIT == 32) ? rem : (uint32_t)codIOffset | (size_t)rem << 32;
			CALL(renorm, 4, 0); // Next division will yield 19 bypass bits...
			codIRange >>= SIZE_BIT - 13;
			num = codIOffset >> (SIZE_BIT - 32);
			quo = num / (uint32_t)codIRange + (quo << 19); // ... such that we keep 13 as msb.
			rem = num % (uint32_t)codIRange;
			shift -= 19;
		}
		
		// Return the unconsumed bypass bits to codIOffset, and compute mvd.
		size_t mul = ((uint32_t)-1 >> 1 >> (shift + k) & quo) * (uint32_t)codIRange + rem;
		codIRange <<= SIZE_BIT - 1 - (shift + k);
		codIOffset = (SIZE_BIT == 32) ? mul : (uint32_t)codIOffset | mul << 32;
		mvd = 1 + (1 << k) + (quo << shift >> (31 - k));
	}
	
	// This will stall a few cycles due to failed store forwarding but is simple to read.
	mb->absMvdComp[pos] = min(mvd, 66);
	mb->absMvdComp_v[pos >> 4] = byte_shuffle(mb->absMvdComp_v[pos >> 4], ctx->mvs_shuffle_v);
	
	// Parse the sign flag.
	if (mvd > 0) {
		codIRange >>= 1;
		mvd = (codIOffset >= codIRange) ? -mvd : mvd;
		codIOffset = (codIOffset >= codIRange) ? codIOffset - codIRange : codIOffset;
	}
	fprintf(stderr, "mvd: %d\n", mvd);
	// fprintf(stderr, "mvd_l%x: %d\n", pos >> 1 & 1, mvd);
	return mvd;
}



/**
 * As its name says, parses mb_qp_delta (9.3.2.7 and 9.3.3.1.1.5).
 *
 * cond should contain the values in coded_block_pattern as stored in mb,
 * such that Intra16x16 can request unconditional parsing by passing 1.
 */
static __attribute__((noinline)) void FUNC(parse_mb_qp_delta, unsigned cond) {
	static const int QP_C[100] = {-36, -35, -34, -33, -32, -31, -30, -29, -28,
		-27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14,
		-13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4,
		5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36,
		37, 37, 37, 38, 38, 38, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39};
	// TODO: Put initialisation for neighbouring Inter/Intra cbf values here
	
	CALL(check_ctx, RESIDUAL_QP_LABEL);
	int mb_qp_delta = 0;
	ctx->mb_qp_delta_non_zero = cond && CALL(get_ae, 60 + ctx->mb_qp_delta_non_zero);
	if (ctx->mb_qp_delta_non_zero) {
		
		// cannot loop forever since binVal will oscillate past end of RBSP
		int count = 1, ctxIdx = 62;
		while (CALL(get_ae, ctxIdx))
			count++, ctxIdx = 63;
		mb_qp_delta = map_se(count);
		int QP = ctx->ps.QP_Y + mb_qp_delta;
		int QpBdOffset_Y = (ctx->ps.BitDepth_Y - 8) * 6;
		ctx->ps.QP_Y = (QP < -QpBdOffset_Y) ? QP + 52 + QpBdOffset_Y :
			(QP >= 52) ? QP - (52 + QpBdOffset_Y) : QP;
		fprintf(stderr, "mb_qp_delta: %d\n", mb_qp_delta);
	}
	
	// deduce this macroblock's QP values
	int QpBdOffset_C = (ctx->ps.BitDepth_C - 8) * 6;
	int QP_Cb = ctx->ps.QP_Y + ctx->ps.chroma_qp_index_offset;
	int QP_Cr = ctx->ps.QP_Y + ctx->ps.second_chroma_qp_index_offset;
	mb->QP[0] = ctx->ps.QP_Y;
	mb->QP[1] = QP_C[36 + clip3(-QpBdOffset_C, 51, QP_Cb)];
	mb->QP[2] = QP_C[36 + clip3(-QpBdOffset_C, 51, QP_Cr)];
}



/**
 * Parsing for chroma 4:2:2 and 4:2:0 is put in a separate function to be
 * tail-called from parse_NxN_residual and parse_Intra16x16_residual.
 */
static __attribute__((noinline)) void FUNC(parse_chroma_residual)
{
	int is422 = ctx->ps.ChromaArrayType - 1;
	if (is422 < 0)
		return;
	
	// As in Intra16x16, DC blocks are parsed to ctx->d[0..7], then transformed to ctx->d[16..31]
	ctx->LevelScale_v[0] = ctx->LevelScale_v[1] = (v4si){64, 64, 64, 64};
	ctx->ctxIdxOffsets_l = ctxIdxOffsets_chromaDC[mb->f.mb_field_decoding_flag];
	ctx->sig_inc_l = ctx->last_inc_l = sig_inc_chromaDC[is422];
	ctx->scan_l = scan_chromaDC[is422];
	
	// One 2x2 or 2x4 DC block for the Cb component
	memset(ctx->d, 0, 32);
	int coded_block_flag_Cb = 0;
	if (mb->f.CodedBlockPatternChromaDC) {
		coded_block_flag_Cb = CALL(get_ae, ctx->ctxIdxOffsets[0] +
			ctx->inc.coded_block_flags_16x16[1]);
		mb->f.coded_block_flags_16x16[1] = coded_block_flag_Cb;
	}
	CALL(check_ctx, RESIDUAL_CB_DC_LABEL);
	CALL(parse_residual_block, coded_block_flag_Cb, 0, is422 * 4 + 3);
	ctx->PredMode[16] = ctx->PredMode[17];
	ctx->pred_buffer_v[16] = ctx->pred_buffer_v[17]; // backup for CHROMA_NxN_BUFFERED
	
	// Another 2x2/2x4 DC block for the Cr component
	ctx->BlkIdx = 20 + is422 * 4;
	memset(ctx->d, 0, 32);
	int coded_block_flag_Cr = 0;
	if (mb->f.CodedBlockPatternChromaDC) {
		coded_block_flag_Cr = CALL(get_ae, ctx->ctxIdxOffsets[0] +
			ctx->inc.coded_block_flags_16x16[2]);
		mb->f.coded_block_flags_16x16[2] = coded_block_flag_Cr;
	}
	CALL(check_ctx, RESIDUAL_CR_DC_LABEL);
	CALL(parse_residual_block, coded_block_flag_Cr, 0, is422 * 4 + 3);
	ctx->PredMode[ctx->BlkIdx] = ctx->PredMode[ctx->BlkIdx + 1];
	
	// Eight or sixteen 4x4 AC blocks for the Cb/Cr components
	CALL(compute_LevelScale4x4, 1);
	ctx->sig_inc_v[0] = ctx->last_inc_v[0] = sig_inc_4x4;
	ctx->scan_v[0] = scan_4x4[mb->f.mb_field_decoding_flag];
	ctx->ctxIdxOffsets_l = ctxIdxOffsets_chromaAC[mb->f.mb_field_decoding_flag];
	for (ctx->BlkIdx = 16; ctx->BlkIdx < 24 + is422 * 8; ctx->BlkIdx++) {
		if (ctx->BlkIdx == 20 + is422 * 4) {
			ctx->pred_buffer_v[16] = ctx->pred_buffer_v[17];
			CALL(compute_LevelScale4x4, 2);
		}
		
		// neighbouring access uses pointer arithmetic to avoid bounds checks
		int coded_block_flag = 0;
		if (mb->f.CodedBlockPatternChromaAC) {
			int cbfA = *(mb->coded_block_flags_4x4 + ctx->coded_block_flags_4x4_A[ctx->BlkIdx]);
			int cbfB = *(mb->coded_block_flags_4x4 + ctx->coded_block_flags_4x4_B[ctx->BlkIdx]);
			coded_block_flag = CALL(get_ae, ctx->ctxIdxOffsets[0] + cbfA + cbfB * 2);
		}
		mb->coded_block_flags_4x4[ctx->BlkIdx] = coded_block_flag;
		memset(ctx->d, 0, 64);
		ctx->d[0] = ctx->d[ctx->BlkIdx];
		ctx->significant_coeff_flags = 1;
		CALL(check_ctx, RESIDUAL_CHROMA_LABEL);
		CALL(parse_residual_block, coded_block_flag, 1, 15);
	}
}



/**
 * Intra16x16 residual blocks have so many differences with Intra4x4 that they
 * deserve their own function.
 */
static __attribute__((noinline)) void FUNC(parse_Intra16x16_residual)
{
	CALL(parse_mb_qp_delta, 1);
	
	// Both AC and DC coefficients are initially parsed to ctx->d[0..15]
	int mb_field_decoding_flag = mb->f.mb_field_decoding_flag;
	ctx->stride = ctx->stride_Y;
	ctx->clip_v = ctx->clip_C;
	ctx->sig_inc_v[0] = ctx->last_inc_v[0] = sig_inc_4x4;
	ctx->scan_v[0] = scan_4x4[mb_field_decoding_flag];
	ctx->BlkIdx = 0;
	do {
		// Parse a DC block, then transform it to ctx->d[16..31]
		int iYCbCr = ctx->BlkIdx >> 4;
		ctx->LevelScale_v[0] = ctx->LevelScale_v[1] = ctx->LevelScale_v[2] =
			ctx->LevelScale_v[3] = (v4si){64, 64, 64, 64};
		memset(ctx->d, 0, 64);
		ctx->ctxIdxOffsets_l = ctxIdxOffsets_16x16DC[iYCbCr][mb_field_decoding_flag];
		mb->f.coded_block_flags_16x16[iYCbCr] = CALL(get_ae, ctx->ctxIdxOffsets[0] +
			ctx->inc.coded_block_flags_16x16[iYCbCr]);
		CALL(check_ctx, RESIDUAL_DC_LABEL);
		CALL(parse_residual_block, mb->f.coded_block_flags_16x16[iYCbCr], 0, 15);
		
		// All AC blocks pick a DC coeff, then go to ctx->d[1..15]
		ctx->ctxIdxOffsets_l = ctxIdxOffsets_16x16AC[iYCbCr][mb_field_decoding_flag];
		ctx->PredMode[ctx->BlkIdx] = ctx->PredMode[ctx->BlkIdx + 1];
		do {
			int coded_block_flag = 0;
			if (mb->CodedBlockPatternLuma[ctx->BlkIdx >> 2]) {
				int cbfA = *(mb->coded_block_flags_4x4 + ctx->coded_block_flags_4x4_A[ctx->BlkIdx]);
				int cbfB = *(mb->coded_block_flags_4x4 + ctx->coded_block_flags_4x4_B[ctx->BlkIdx]);
				coded_block_flag = CALL(get_ae, ctx->ctxIdxOffsets[0] + cbfA + cbfB * 2);
			}
			mb->coded_block_flags_4x4[ctx->BlkIdx] = coded_block_flag;
			memset(ctx->d, 0, 64);
			ctx->d[0] = ctx->d[16 + (ctx->BlkIdx & 15)];
			ctx->significant_coeff_flags = 1;
			CALL(check_ctx, RESIDUAL_4x4_LABEL);
			CALL(parse_residual_block, coded_block_flag, 1, 15);
		// not a loop-predictor-friendly condition, but would it make a difference?
		} while (++ctx->BlkIdx & 15);
		
		// nice optimisation for 4:4:4 modes
		ctx->stride = ctx->stride_C;
		ctx->clip_v = ctx->clip_C;
		if (ctx->ps.ChromaArrayType <3)
			JUMP(parse_chroma_residual);
	} while (ctx->BlkIdx < 48);
}



/**
 * This block is dedicated to the parsing of Intra_NxN and Inter_NxN, since
 * they share much in common.
 */
static __attribute__((noinline)) void FUNC(parse_NxN_residual)
{
	CALL(parse_mb_qp_delta, mb->f.CodedBlockPatternChromaDC | mb->CodedBlockPatternLuma_s);
	
	// next few blocks will share many parameters, so we cache a LOT of them
	ctx->stride = ctx->stride_Y;
	ctx->clip_v = ctx->clip_Y;
	ctx->BlkIdx = 0;
	do {
		int iYCbCr = ctx->BlkIdx >> 4;
		int mb_field_decoding_flag = mb->f.mb_field_decoding_flag;
		if (!mb->f.transform_size_8x8_flag) {
			ctx->ctxIdxOffsets_l = ctxIdxOffsets_4x4[iYCbCr][mb_field_decoding_flag];
			ctx->scan_v[0] = scan_4x4[mb_field_decoding_flag];
			ctx->sig_inc_v[0] = ctx->last_inc_v[0] = sig_inc_4x4;
			CALL(compute_LevelScale4x4, iYCbCr);
			
			// Decoding directly follows parsing to avoid duplicate loops.
			do {
				int coded_block_flag = 0;
				if (mb->CodedBlockPatternLuma[ctx->BlkIdx >> 2]) {
					int cbfA = *(mb->coded_block_flags_4x4 + ctx->coded_block_flags_4x4_A[ctx->BlkIdx]);
					int cbfB = *(mb->coded_block_flags_4x4 + ctx->coded_block_flags_4x4_B[ctx->BlkIdx]);
					coded_block_flag = CALL(get_ae, ctx->ctxIdxOffsets[0] + cbfA + cbfB * 2);
				}
				mb->coded_block_flags_4x4[ctx->BlkIdx] = coded_block_flag;
				memset(ctx->d, 0, 64);
				ctx->significant_coeff_flags = 0;
				CALL(check_ctx, RESIDUAL_4x4_LABEL);
				CALL(parse_residual_block, coded_block_flag, 0, 15);
			} while (++ctx->BlkIdx & 15);
		} else {
			
			ctx->ctxIdxOffsets_l = ctxIdxOffsets_8x8[iYCbCr][mb_field_decoding_flag];
			const v16qi *p = sig_inc_8x8[mb_field_decoding_flag];
			const v16qi *r = scan_8x8[mb_field_decoding_flag];
			for (int i = 0; i < 4; i++) {
				ctx->sig_inc_v[i] = p[i];
				ctx->last_inc_v[i] = last_inc_8x8[i];
				ctx->scan_v[i] = r[i];
			}
			CALL(compute_LevelScale8x8, iYCbCr);
			
			do {
				int luma8x8BlkIdx = ctx->BlkIdx >> 2;
				int coded_block_flag = mb->CodedBlockPatternLuma[luma8x8BlkIdx & 3];
				if (coded_block_flag && ctx->ps.ChromaArrayType == 3) {
					int cbfA = *(mb->coded_block_flags_8x8 + ctx->coded_block_flags_8x8_A[luma8x8BlkIdx]);
					int cbfB = *(mb->coded_block_flags_8x8 + ctx->coded_block_flags_8x8_B[luma8x8BlkIdx]);
					coded_block_flag = CALL(get_ae, ctx->ctxIdxOffsets[0] + cbfA + cbfB * 2);
				}
				mb->coded_block_flags_8x8[luma8x8BlkIdx] = coded_block_flag;
				mb->coded_block_flags_4x4_s[luma8x8BlkIdx] = coded_block_flag ? 0x01010101 : 0;
				memset(ctx->d, 0, 256);
				ctx->significant_coeff_flags = 0;
				CALL(check_ctx, RESIDUAL_8x8_LABEL);
				CALL(parse_residual_block, coded_block_flag, 0, 63);
			} while ((ctx->BlkIdx += 4) & 15);
		}
		
		// nice optimisation for 4:4:4 modes
		ctx->stride = ctx->stride_C;
		ctx->clip_v = ctx->clip_C;
		if (ctx->ps.ChromaArrayType <3)
			JUMP(parse_chroma_residual);
	} while (ctx->BlkIdx < 48);
}



/**
 * Parses CodedBlockPatternLuma/Chroma (9.3.2.6 and 9.3.3.1.1.4).
 *
 * As with mb_qp_delta, coded_block_pattern is parsed in two distinct code
 * paths, thus put in a non-inlined function.
 */
static __attribute__((noinline)) void FUNC(parse_coded_block_pattern) {
	CALL(check_ctx, RESIDUAL_CBP_LABEL);
	
	// Luma prefix
	for (int i = 0; i < 4; i++) {
		int cbpA = *(mb->CodedBlockPatternLuma + ctx->CodedBlockPatternLuma_A[i]);
		int cbpB = *(mb->CodedBlockPatternLuma + ctx->CodedBlockPatternLuma_B[i]);
		mb->CodedBlockPatternLuma[i] = CALL(get_ae, 76 - cbpA - cbpB * 2);
	}
	
	// Chroma suffix
	if (ctx->ps.ChromaArrayType == 1 || ctx->ps.ChromaArrayType == 2) {
		mb->f.CodedBlockPatternChromaDC = CALL(get_ae, 77 + ctx->inc.CodedBlockPatternChromaDC);
		if (mb->f.CodedBlockPatternChromaDC)
			mb->f.CodedBlockPatternChromaAC = CALL(get_ae, 81 + ctx->inc.CodedBlockPatternChromaAC);
	}
	
	fprintf(stderr, "coded_block_pattern: %u\n",
		mb->CodedBlockPatternLuma[0] * 1 + mb->CodedBlockPatternLuma[1] * 2 +
		mb->CodedBlockPatternLuma[2] * 4 + mb->CodedBlockPatternLuma[3] * 8 +
		(mb->f.CodedBlockPatternChromaDC + mb->f.CodedBlockPatternChromaAC) * 16);
}



/**
 * This function parses the syntax elements mvd_lX (from function),
 * coded_block_pattern (from function), and transform_size_8x8_flag for an
 * Inter macroblock, then branches to residual decoding through tail call.
 *
 * Motion vector prediction is one of the hardest parts to decode (8.4.1.3),
 * here is a summary of the rules:
 * _ A/B/C/D are 4x4 blocks on pixels at relative positions (-1,0)/(0,-1)/(W,-1)/(-1,-1)
 * _ unavailable A/B/C/D blocks count as refIdx=-1 and mv=0
 * _ if C is unavailable, replace it with D
 * _ for 8x16 and 16x8, predict from (A,C) and (B,A) if their refIdx matches
 * _ otherwise if B, C and D are unavailable, replace B and C with A
 * _ then if only one of refIdxA/B/C is equal to refIdx, predict mv from it
 * _ otherwise predict mv as median(mvA, mvB, mvC)
 *
 * In this function, refIdx4x4_eq covers rules 1~5 already, so we only need to
 * switch on values of each equality mask to determine predicted mvs.
 */
static __attribute__((noinline)) void FUNC(parse_mvs)
{
	// parsing mvd components in pairs, to reduce branches
	do {
		int i = ctz32(ctx->mvd_flags);
		int x = CALL(parse_mvd, i, 40);
		int y = CALL(parse_mvd, i + 32, 47);
		v8hi mv = {x, y};
		
		// load neighbouring motion vectors and equality mask
		v8hi mvA = (v8hi)(v4si){*(mb->mvs_s + ctx->mvs_A[i])};
		v8hi mvB = (v8hi)(v4si){*(mb->mvs_s + ctx->mvs_B[i])};
		v8hi mvC = (v8hi)(v4si){*(mb->mvs_s + ctx->mvs_C[i])};
		int eq = ctx->refIdx4x4_eq[i];
		
		// This branch is unavoidable if we don't want to mess with mvA/B addresses too.
		if (__builtin_expect(0xe9 >> eq & 1, 1)) {
			mv += vector_median(mvA, mvB, mvC);
		} else {
			mv += (eq == 1) ? mvA : (eq == 2) ? mvB : mvC;
		}
		
		// interleaving, masking and storing
		v8hi mvs = (v8hi)__builtin_shufflevector((v4si)mv, (v4si)mv, 0, 0, 0, 0);
		v8hi *dst = mb->mvs_v + (i >> 4 << 2);
		int i4x4 = i & 15;
		v16qi mask = ctx->mvs_shuffle_v == (int8_t)i4x4;
		dst[0] = vector_select(__builtin_shufflevector(mask, mask, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3), mvs, dst[0]);
		dst[1] = vector_select(__builtin_shufflevector(mask, mask, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7), mvs, dst[1]);
		dst[2] = vector_select(__builtin_shufflevector(mask, mask, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11), mvs, dst[2]);
		dst[3] = vector_select(__builtin_shufflevector(mask, mask, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15), mvs, dst[3]);
		CALL(decode_inter, i4x4, ctx->part_sizes[i4x4 * 2], ctx->part_sizes[i4x4 * 2 + 1], mvs[0], mvs[1]);
	} while (ctx->mvd_flags &= ctx->mvd_flags - 1);
	
	CALL(parse_coded_block_pattern);
	
	if (mb->CodedBlockPatternLuma_s && ctx->transform_8x8_mode_flag) {
		mb->f.transform_size_8x8_flag = CALL(get_ae, 399 + ctx->inc.transform_size_8x8_flag);
		fprintf(stderr, "transform_size_8x8_flag: %x\n", mb->f.transform_size_8x8_flag);
	}
	
	// temporary fix until Pred modes are removed
	ctx->PredMode_v[0] = (mb->f.transform_size_8x8_flag) ?
		(v16qu){ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8, ADD_RESIDUAL_8x8} :
		(v16qu){ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4};
	if (ctx->ps.ChromaArrayType == 1)
		ctx->PredMode_v[1] = (v16qu){TRANSFORM_DC_2x2, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, TRANSFORM_DC_2x2, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4};
	else if (ctx->ps.ChromaArrayType == 2)
		ctx->PredMode_v[1] = (v16qu){TRANSFORM_DC_2x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, TRANSFORM_DC_2x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4, ADD_RESIDUAL_4x4};
	else if (ctx->ps.ChromaArrayType == 3)
		ctx->PredMode_v[1] = ctx->PredMode_v[2] = ctx->PredMode_v[0];
	JUMP(parse_NxN_residual);
}



/**
 * Parses intra_chroma_pred_mode (9.3.2.2 and 9.3.3.1.1.8).
 *
 * As with mb_qp_delta and coded_block_pattern, experience shows allowing
 * compilers to inline this function makes them produce slower&heavier code.
 */
static __attribute__((noinline)) void FUNC(parse_intra_chroma_pred_mode)
{
	// Do not optimise too hard to keep the code understandable here.
	CALL(check_ctx, INTRA_CHROMA_LABEL);
	int type = ctx->ps.ChromaArrayType;
	if (type == 1 || type == 2) {
		int ctxIdx = 64 + ctx->inc.intra_chroma_pred_mode_non_zero;
		int mode = 0;
		while (mode <3 && CALL(get_ae, ctxIdx))
			mode++, ctxIdx = 67;
		mb->f.intra_chroma_pred_mode_non_zero = (mode > 0);
		fprintf(stderr, "intra_chroma_pred_mode: %u\n", mode);
		
		// ac[0]==VERTICAL_4x4_BUFFERED reuses the buffering mode of Intra16x16
		static uint8_t dc[4] = {DC_CHROMA_8x8, HORIZONTAL_CHROMA_8x8, VERTICAL_CHROMA_8x8, PLANE_CHROMA_8x8};
		static uint8_t ac[4] = {VERTICAL_4x4_BUFFERED, HORIZONTAL_4x4_BUFFERED, VERTICAL_4x4_BUFFERED, PLANE_4x4_BUFFERED};
		int depth = (ctx->ps.BitDepth_C == 8) ? 0 : VERTICAL_4x4_16_BIT;
		int i = depth + ac[mode];
		int dc420 = depth + dc[mode] + (mode == 0 ? ctx->inc.unavailable & 3 : 0);
		ctx->PredMode_v[1] = (v16qu){i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i};
		if (type == 1)
			ctx->PredMode[16] = ctx->PredMode[20] = dc420;
		else
			ctx->PredMode[16] = ctx->PredMode[24] = dc420 + (DC_CHROMA_8x16 - DC_CHROMA_8x8);
	}
}



/**
 * Parses prev_intraNxN_pred_mode_flag and rem_intraNxN_pred_mode, and returns
 * the given intra_pred_mode (7.3.5.1, 7.4.5.1, 8.3.1.1 and table 9-34).
 */
static __attribute__((noinline)) int FUNC(parse_intraNxN_pred_mode, int luma4x4BlkIdx)
{
	// dcPredModePredictedFlag is enforced by putting -2
	int intraMxMPredModeA = *(mb->Intra4x4PredMode + ctx->Intra4x4PredMode_A[luma4x4BlkIdx]);
	int intraMxMPredModeB = *(mb->Intra4x4PredMode + ctx->Intra4x4PredMode_B[luma4x4BlkIdx]);
	int mode = abs(min(intraMxMPredModeA, intraMxMPredModeB));
	if (!CALL(get_ae, 68)) {
		int rem_intra_pred_mode = CALL(get_ae, 69);
		rem_intra_pred_mode += CALL(get_ae, 69) * 2;
		rem_intra_pred_mode += CALL(get_ae, 69) * 4;
		fprintf(stderr, "rem_intra_pred_mode: %u\n", rem_intra_pred_mode);
		mode = rem_intra_pred_mode + (rem_intra_pred_mode >= mode);
	} else {
		// for compatibility with reference decoder
		fprintf(stderr, "rem_intra_pred_mode: -1\n");
	}
	return mode;
}



/**
 * This function parses the syntax elements mb_type, transform_size_8x8_flag,
 * intraNxN_pred_mode (from function), intra_chroma_pred_mode (from function),
 * PCM stuff, and coded_block_pattern (from function) for the current Intra
 * macroblock. It proceeds to residual decoding through tail call.
 *
 * The prediction mode is stored twice in ctx:
 * _ Intra4x4PredMode, buffer for neighbouring values. The special value -2 is
 *   used by unavailable blocks and Inter blocks with constrained_intra_pred_flag,
 *   to account for dcPredModePredictedFlag.
 * _ PredMode, for the late decoding of samples. It has a wider range of values
 *   to account for unavailability of neighbouring blocks, Intra chroma modes
 *   and Inter prediction.
 */
static __attribute__((noinline)) void FUNC(parse_I_mb, int ctxIdx)
{
	static const Edge264_flags flags_PCM = {
		.CodedBlockPatternChromaDC = 1,
		.CodedBlockPatternChromaAC = 1,
		.coded_block_flags_16x16 = {1, 1, 1},
	};
	CALL(check_ctx, INTRA_MB_LABEL);
	
	// Intra-specific initialisations
	if (ctx->inc.unavailable & 1) {
		mb[-1].coded_block_flags_4x4_v[0] = mb[-1].coded_block_flags_4x4_v[1] =
			mb[-1].coded_block_flags_4x4_v[2] = mb[-1].coded_block_flags_8x8_v =
			(v16qi){1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		ctx->inc.coded_block_flags_16x16_s |= 0x01010101;
	}
	if (ctx->inc.unavailable & 2) {
		// Why add a new variable when the value is already in memory??
		Edge264_macroblock *mbB = (Edge264_macroblock *)((uint8_t *)mb + ctx->coded_block_flags_4x4_B[0] - 10);
		mbB->coded_block_flags_4x4_v[0] = mbB->coded_block_flags_4x4_v[1] =
			mbB->coded_block_flags_4x4_v[2] = mbB->coded_block_flags_8x8_v =
			(v16qi){1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		ctx->inc.coded_block_flags_16x16_s |= 0x02020202;
	}
	mb->f.mbIsInterFlag = 0;
	mb->refIdx_v = (v8qi){-1, -1, -1, -1, -1, -1, -1, -1};
	
	// I_NxN
	if (!CALL(get_ae, ctxIdx)) {
		mb->f.mb_type_I_NxN = 1;
		fprintf(stderr, (ctxIdx == 17) ? "mb_type: 5\n" : // in P slice
		                (ctxIdx == 32) ? "mb_type: 23\n" : // in B slice
		                                 "mb_type: 0\n"); // in I slice
		
		// 7.3.5, 7.4.5, 9.3.3.1.1.10 and table 9-34
		if (ctx->ps.transform_8x8_mode_flag) {
			mb->f.transform_size_8x8_flag = CALL(get_ae, 399 + ctx->inc.transform_size_8x8_flag);
			fprintf(stderr, "transform_size_8x8_flag: %x\n", mb->f.transform_size_8x8_flag);
		}
		
		if (mb->f.transform_size_8x8_flag) {
			for (int i = 0; i < 4; i++) {
				int mode = CALL(parse_intraNxN_pred_mode, i * 4);
				mb->Intra4x4PredMode[i * 4 + 1] = mb->Intra4x4PredMode[i * 4 + 2] = mb->Intra4x4PredMode[i * 4 + 3] = mode;
				ctx->PredMode[i * 4] = ctx->intra8x8_modes[mode][ctx->unavail[i * 5]];
			}
		} else {
			for (int i = 0; i < 16; i++) {
				mb->Intra4x4PredMode[i] = CALL(parse_intraNxN_pred_mode, i);
				ctx->PredMode[i] = ctx->intra4x4_modes[mb->Intra4x4PredMode[i]][ctx->unavail[i]];
			}
		}
		ctx->PredMode_v[1] = ctx->PredMode_v[2] = ctx->PredMode_v[0] + ctx->pred_offset_C;
		
		CALL(parse_intra_chroma_pred_mode);
		CALL(parse_coded_block_pattern);
		JUMP(parse_NxN_residual);
	
	// Intra_16x16
	} else if (!CALL(get_ae, 276)) {
		mb->CodedBlockPatternLuma_s = CALL(get_ae, max(ctxIdx + 1, 6)) ? 0x01010101 : 0;
		mb->f.CodedBlockPatternChromaDC = CALL(get_ae, max(ctxIdx + 2, 7));
		if (mb->f.CodedBlockPatternChromaDC)
			mb->f.CodedBlockPatternChromaAC = CALL(get_ae, max(ctxIdx + 2, 8));
		int mode = CALL(get_ae, max(ctxIdx + 3, 9)) << 1;
		mode += CALL(get_ae, max(ctxIdx + 3, 10));
		fprintf(stderr, "mb_type: %u\n", mb->CodedBlockPatternLuma[0] * 12 +
			(mb->f.CodedBlockPatternChromaDC + mb->f.CodedBlockPatternChromaAC) * 4 +
			mode + (ctxIdx == 17 ? 6 : ctxIdx == 32 ? 24 : 1));
		
		mb->Intra4x4PredMode_v = (v16qi){2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
		
		// prepare the decoding modes
		static uint8_t dc[4] = {VERTICAL_16x16, HORIZONTAL_16x16, DC_16x16, PLANE_16x16};
		int depth = ctx->intra4x4_modes[0][0] - VERTICAL_4x4;
		int p = depth + VERTICAL_4x4_BUFFERED + mode;
		ctx->PredMode_v[0] = (v16qu){p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p};
		ctx->PredMode[0] = depth + dc[mode] + (mode == 2 ? ctx->inc.unavailable & 3 : 0);
		ctx->PredMode_v[1] = ctx->PredMode_v[2] = ctx->PredMode_v[0] + ctx->pred_offset_C;
		
		CALL(parse_intra_chroma_pred_mode);
		JUMP(parse_Intra16x16_residual);
		
	// I_PCM
	} else {
		fprintf(stderr, "mb_type: 25\n");
		
		mb->f.v |= flags_PCM.v;
		mb->Intra4x4PredMode_v = (v16qi){2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
		mb->CodedBlockPatternLuma_s = 0x01010101;
		mb->coded_block_flags_8x8_v = mb->coded_block_flags_4x4_v[0] =
			mb->coded_block_flags_4x4_v[1] = mb->coded_block_flags_4x4_v[2] =
			(v16qi){1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		ctx->mb_qp_delta_non_zero = 0;
		
		// reclaim all but two bits from codIOffset, and skip pcm_alignment_zero_bit
		codIOffset >>= ctx->shift;
		ctx->shift = (ctx->shift - (SIZE_BIT - clz(codIRange) - 2) + 7) & -8;
		while (ctx->shift < 0) {
			ctx->shift += 8;
			ctx->RBSP[1] = lsd(ctx->RBSP[0], ctx->RBSP[1], SIZE_BIT - 8);
			ctx->RBSP[0] = lsd(codIOffset, ctx->RBSP[0], SIZE_BIT - 8);
			codIOffset >>= 8;
			int32_t i;
			memcpy(&i, ctx->CPB - 4, 4);
			ctx->CPB -= 1 + ((big_endian32(i) >> 8) == 3);
		}
		
		// PCM is so rare that it should be compact rather than fast
		uint8_t *p = ctx->frame + ctx->frame_offsets_x[0] + ctx->frame_offsets_y[0];
		for (int y = 16; y-- > 0; p += ctx->stride_Y) {
			for (int x = 0; x < 16; x++) {
				if (ctx->ps.BitDepth_Y == 8)
					p[x] = CALL(get_uv, 8);
				else
					((uint16_t *)p)[x] = CALL(get_uv, ctx->ps.BitDepth_Y);
			}
		}
		p = ctx->frame + ctx->frame_offsets_x[16] + ctx->frame_offsets_y[16];
		int MbWidthC = (ctx->ps.ChromaArrayType < 3) ? 8 : 16;
		static int8_t MbHeightC[4] = {0, 8, 16, 16};
		for (int y = MbHeightC[ctx->ps.ChromaArrayType]; y-- > 0; p += ctx->stride_C) {
			for (int x = 0; x < MbWidthC; x++) {
				if (ctx->ps.BitDepth_Y == 8)
					p[x] = CALL(get_uv, 8);
				else
					((uint16_t *)p)[x] = CALL(get_uv, ctx->ps.BitDepth_Y);
			}
		}
		p = ctx->frame + ctx->frame_offsets_x[32] + ctx->frame_offsets_y[32];
		for (int y = MbHeightC[ctx->ps.ChromaArrayType]; y-- > 0; p += ctx->stride_C) {
			for (int x = 0; x < MbWidthC; x++) {
				if (ctx->ps.BitDepth_Y == 8)
					p[x] = CALL(get_uv, 8);
				else
					((uint16_t *)p)[x] = CALL(get_uv, ctx->ps.BitDepth_Y);
			}
		}
		
		// reinitialize CABAC
		codIRange = (size_t)255 << (SIZE_BIT - 9);
		codIOffset = CALL(get_uv, SIZE_BIT - 1);
	}
}



/**
 * Parses ref_idx_lx (9.3.3.1.1.6).
 *
 * There are 4 code paths (8x8, 8x16, 16x8, 16x16) with different treatments
 * after parsing ref_idx, so we put it in a function to spare a branch after.
 */
static __attribute__((noinline)) void FUNC(parse_ref_idx)
{
	for (unsigned f = ctx->mvd_flags & ctx->ref_idx_mask; f != 0; f &= f - 1) {
		int i = __builtin_ctz(f) >> 2;
		int refIdxA = *(mb->refIdx + (i & 4) + ctx->refIdx_A[i & 3]);
		int refIdxB = *(mb->refIdx + (i & 4) + ctx->refIdx_B[i & 3]);
		int ctxIdxInc = (refIdxA > 0) + (refIdxB > 0) * 2;
		int num_ref_idx_active_minus1 = ctx->ps.num_ref_idx_active[i >> 2] - 1;
		int refIdx = 0;
		while (CALL(get_ae, 54 + ctxIdxInc) && refIdx < num_ref_idx_active_minus1) {
			ctxIdxInc = (ctxIdxInc >> 2) + 4; // cool trick from ffmpeg
			refIdx++;
		}
		mb->refIdx[i] = refIdx;
		fprintf(stderr, "ref_idx: %u\n", refIdx);
	}
}



/**
 * These functions parse the syntax elements mb_type, sub_mb_type and ref_idx
 * (from function) for the current macroblock in a P or B slice, and initialize
 * mvd_flags, mvs_shuffle, transform_8x8_mode_flag and refIdx4x4_eq, before
 * jumping to further decoding.
 *
 * mvd_flags is a 32bit mask indexed by LX*16+BlkIdx. Each set bit indicates a
 * mvd pair should be parsed for the corresponding block index. We obtain a
 * mask for parsing refIdx by masking every 4th bit.
 *
 * Each mvd pair may cover a 4x4, 4x8, 8x4, 8x8, 8x16, 16x8 or 16x16 block.
 * While we do not keep mb_type, we will duplicate each parsed mvd pair to all
 * covered 4x4 blocks. So we provide a shuffle mask (mvd_shuffle) for each 4x4
 * position, that indicates where it takes its value. It overwrites all mvs for
 * each mvd pair, but then the code is simple and branchless.
 *
 * transform_8x8_mode_flag is initialized with a copy of its value in ctx->ps,
 * then modified during mb_type parsing to account for the complex conditions
 * in 7.3.5.
 *
 * refIdx4x4_eq stores the result of comparing refIdx with refIdxA/B/C for each
 * 4x4 block: (refIdx==refIdxA) + (refIdx==refIdxB)*2 + (refIdx==refIdxC)*4.
 * They are used in parse_mvs to compute the origins of predicted mvs.
 * For partitions smaller than or equal to 8x8, they are computed in parallel
 * with vector code, first by loading refIdx neighbours in a single v16qi
 *  1  2  3  4
 *  5| 6  7|
 *  9|10 11|
 * then by shuffling it with variable masks to infer refIdxA/B/C.
 */
 __attribute__((noinline)) void FUNC(parse_P_mb)
{
	static const int32_t refIdx4x4_C_base[4] = {0x01010101, 0x02020202, 0x05050505, 0x06060606};
	static const v4qi refIdx4x4_C_inc[8] = {
		{1, 2, 5, 5}, {2, 0, 4, 0}, // B and C available
		{0, 2, 5, 5}, {2, 0, 4, 0}, // B unavailable, C available
		{1, 1, 5, 5}, {0, 0, 4, 0}, // B available, C unavailable
		{0, 1, 5, 5}, {0, 0, 4, 0}}; // B and C unavailable
	
	// Inter initializations
	ctx->transform_8x8_mode_flag = ctx->ps.transform_8x8_mode_flag;
	mb->f.mbIsInterFlag = 1;
	mb->refIdx_s[1] = -1;
	mb->Intra4x4PredMode_v = (v16qi){2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
	
	// shortcuts for P_Skip and Intra
	mb->f.mb_skip_flag = CALL(get_ae, 13 - ctx->inc.mb_skip_flag);
	fprintf(stderr, "mb_skip_flag: %x\n", mb->f.mb_skip_flag);
	if (mb->f.mb_skip_flag) {
		int refIdxA = *(mb->refIdx + ctx->refIdx_A[0]);
		int refIdxB = *(mb->refIdx + ctx->refIdx_B[0]);
		int mvA = *(mb->mvs_s + ctx->mvs_A[0]);
		int mvB = *(mb->mvs_s + ctx->mvs_B[0]);
		v8hi mv = {};
		if (!(ctx->inc.unavailable & 3) && (refIdxA | mvA) && (refIdxB | mvB)) {
			int refIdxC = *(mb->refIdx + (ctx->inc.unavailable & 4 ? ctx->refIdx_D : ctx->refIdx_C));
			int mvC = *(mb->mvs_s + (ctx->inc.unavailable & 4 ? ctx->mvs8x8_D[0] : ctx->mvs8x8_C[1]));
			int eq = (!refIdxA | (ctx->inc.unavailable==14)) + !refIdxB * 2 + !refIdxC * 4;
			if (__builtin_expect(0xe9 >> eq & 1, 1)) {
				mv = vector_median((v8hi)(v4si){mvA}, (v8hi)(v4si){mvB}, (v8hi)(v4si){mvC});
			} else {
				mv = (v8hi)(v4si){(eq == 1) ? mvA : (eq == 2) ? mvB : mvC};
			}
		}
		v8hi mvs = (v8hi)__builtin_shufflevector((v4si)mv, (v4si)mv, 0, 0, 0, 0);
		mb->mvs_v[0] = mb->mvs_v[1] = mb->mvs_v[2] = mb->mvs_v[3] = mvs;
		JUMP(decode_inter, 0, 16, 16, mvs[0], mvs[1]);
	} else if (CALL(get_ae, 14)) {
		JUMP(parse_I_mb, 17);
	}
	
	// Non-skip Inter initialisations
	if (ctx->inc.unavailable & 1) {
		mb[-1].coded_block_flags_4x4_v[0] = mb[-1].coded_block_flags_4x4_v[1] =
			mb[-1].coded_block_flags_4x4_v[2] = mb[-1].coded_block_flags_8x8_v = (v16qi){};
		ctx->inc.coded_block_flags_16x16_s &= 0x02020202;
	}
	if (ctx->inc.unavailable & 2) {
		// Why add a new variable when the value is already in memory??
		Edge264_macroblock *mbB = (Edge264_macroblock *)((uint8_t *)mb + ctx->coded_block_flags_4x4_B[0] - 10);
		mbB->coded_block_flags_4x4_v[0] = mbB->coded_block_flags_4x4_v[1] =
			mbB->coded_block_flags_4x4_v[2] = mbB->coded_block_flags_8x8_v = (v16qi){};
		ctx->inc.coded_block_flags_16x16_s &= 0x01010101;
	}
	
	// initializations and jumps for mb_type
	if (!CALL(get_ae, 15)) {
		if (!CALL(get_ae, 16)) { // 16x16
			ctx->mvd_flags = 0x0001;
			fprintf(stderr, "mb_type: 0\n");
			CALL(parse_ref_idx);
			int refIdx = mb->refIdx[0];
			int refIdxA = *(mb->refIdx + ctx->refIdx_A[0]);
			int refIdxB = *(mb->refIdx + ctx->refIdx_B[0]);
			int refIdxC = *(mb->refIdx + (ctx->inc.unavailable & 4 ? ctx->refIdx_D : ctx->refIdx_C));
			ctx->mvs_C[0] = (ctx->inc.unavailable & 4) ? ctx->mvs8x8_D[0] : ctx->mvs8x8_C[1];
			mb->refIdx_v = __builtin_shufflevector(mb->refIdx_v, mb->refIdx_v, 0, 0, 0, 0, 4, 4, 4, 4);
			ctx->refIdx4x4_eq[0] = ((refIdx==refIdxA) | (ctx->inc.unavailable==14)) + (refIdx==refIdxB) * 2 + (refIdx==refIdxC) * 4;
			ctx->mvs_shuffle_v = (v16qi){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			ctx->part_sizes_l[0] = (int64_t)(v8qi){16, 16};
			JUMP(parse_mvs);
		} // else 8x8
		
	} else if (!CALL(get_ae, 17)) { // 8x16
		ctx->mvd_flags = 0x0011;
		fprintf(stderr, "mb_type: 2\n");
		CALL(parse_ref_idx);
		int refIdx0 = mb->refIdx[0];
		int refIdx1 = mb->refIdx[1];
		int refIdxA0 = *(mb->refIdx + ctx->refIdx_A[0]);
		int refIdxB0 = *(mb->refIdx + ctx->refIdx_B[0]);
		int refIdxB1 = *(mb->refIdx + ctx->refIdx_B[1]);
		int refIdxC0 = (ctx->inc.unavailable & 2) ? *(mb->refIdx + ctx->refIdx_D) : refIdxB1;
		ctx->mvs_C[0] = (ctx->inc.unavailable & 2) ? ctx->mvs8x8_D[0] : ctx->mvs8x8_C[0];
		int refIdxC1 = (ctx->inc.unavailable & 4) ? refIdxB0 : *(mb->refIdx + ctx->refIdx_C);
		ctx->mvs_C[4] = (ctx->inc.unavailable & 4) ? ctx->mvs8x8_D[1] : ctx->mvs8x8_C[1];
		mb->refIdx_v = __builtin_shufflevector(mb->refIdx_v, mb->refIdx_v, 0, 1, 0, 1, 4, 5, 4, 5);
		ctx->refIdx4x4_eq[0] = (refIdx0==refIdxA0) ? 0x1 : (ctx->unavail[0]==14) + (refIdx0==refIdxB0) * 2 + (refIdx0==refIdxC0) * 4;
		ctx->refIdx4x4_eq[4] = (refIdx1==refIdxC1) ? 0x4 : ((refIdx1==refIdx0) | (ctx->unavail[5]==14)) + (refIdx1==refIdxB1) * 2;
		ctx->mvs_shuffle_v = (v16qi){0, 0, 0, 0, 4, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 4};
		ctx->part_sizes_l[0] = ctx->part_sizes_l[1] = (int64_t)(v8qi){8, 16};
		JUMP(parse_mvs);
		
	} else { // 16x8
		ctx->mvd_flags = 0x0101;
		fprintf(stderr, "mb_type: 1\n");
		CALL(parse_ref_idx);
		int refIdx0 = mb->refIdx[0];
		int refIdx2 = mb->refIdx[2];
		int refIdxA0 = *(mb->refIdx + ctx->refIdx_A[0]);
		int refIdxA2 = *(mb->refIdx + ctx->refIdx_A[2]);
		int refIdxB0 = *(mb->refIdx + ctx->refIdx_B[0]);
		int refIdxC0 = *(mb->refIdx + (ctx->inc.unavailable & 4 ? ctx->refIdx_D : ctx->refIdx_C));
		ctx->mvs_C[0] = (ctx->inc.unavailable & 4) ? ctx->mvs8x8_D[0] : ctx->mvs8x8_C[1];
		ctx->mvs_C[8] = ctx->mvs8x8_D[2];
		mb->refIdx_v = __builtin_shufflevector(mb->refIdx_v, mb->refIdx_v, 0, 0, 2, 2, 4, 4, 6, 6);
		ctx->refIdx4x4_eq[0] = (refIdx0==refIdxB0) ? 0x2 : ((refIdx0==refIdxA0) | (ctx->inc.unavailable==14)) + (refIdx0==refIdxC0) * 4;
		ctx->refIdx4x4_eq[8] = (refIdx2==refIdxA2) ? 0x1 : (refIdx2==refIdx0) * 2 + (refIdx2==refIdxA0) * 4;
		ctx->mvs_shuffle_v = (v16qi){0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8};
		ctx->part_sizes_l[0] = ctx->part_sizes_l[2] = (int64_t)(v8qi){16, 8};
		JUMP(parse_mvs);
	}
	fprintf(stderr, "mb_type: 3\n");
	
	// initializations and jumps for sub_mb_type
	unsigned flags = 0;
	for (int i8x8 = 0, f, s; i8x8 < 4; i8x8++) {
		int i4x4 = i8x8 * 4;
		int unavail1 = ctx->unavail[i4x4 + 1]; // always even, incremented by subpart modes to signal 8-sample width
		int64_t sizes;
		if (CALL(get_ae, 21)) { // 8x8
			ctx->mvs_C[i4x4] = (unavail1 & 4 ? ctx->mvs8x8_D : ctx->mvs8x8_C)[i8x8];
			ctx->unavail[i4x4] |= unavail1++;
			f = 1;
			s = 0;
			sizes = (int64_t)(v8qi){8, 8};
		} else if (ctx->transform_8x8_mode_flag = 0, !CALL(get_ae, 22)) { // 8x4
			ctx->mvs_C[i4x4] = (unavail1 & 4 ? ctx->mvs8x8_D : ctx->mvs8x8_C)[i8x8];
			ctx->mvs_C[i4x4 + 2] = ctx->mvs_A[i4x4];
			ctx->unavail[i4x4] |= unavail1++;
			f = 5;
			s = (int32_t)(v4qi){0, 0, 2, 2};
			sizes = (int64_t)(v8qi){8, 4, 0, 0, 8, 4, 0, 0};
		} else if (CALL(get_ae, 23)) { // 4x8
			ctx->mvs_C[i4x4] = (unavail1 & 2) ? ctx->mvs8x8_D[i8x8] : ctx->mvs_B[i4x4 + 1];
			ctx->mvs_C[i4x4 + 1] = (unavail1 & 4) ? ctx->mvs_B[i4x4] : ctx->mvs8x8_C[i8x8];
			f = 3;
			s = (int32_t)(v4qi){0, 1, 0, 1};
			sizes = (int64_t)(v8qi){4, 8, 4, 8};
		} else { // 4x4
			ctx->mvs_C[i4x4] = (unavail1 & 2) ? ctx->mvs8x8_D[i8x8] : ctx->mvs_B[i4x4 + 1];
			ctx->mvs_C[i4x4 + 1] = (unavail1 & 4) ? ctx->mvs_B[i4x4] : ctx->mvs8x8_C[i8x8];
			ctx->mvs_C[i4x4 + 2] = i4x4 + 1;
			ctx->mvs_C[i4x4 + 3] = i4x4;
			f = 15;
			s = (int32_t)(v4qi){0, 1, 2, 3};
			sizes = (int64_t)(v8qi){4, 4, 4, 4, 4, 4, 4, 4};
		}
		flags |= f << i4x4;
		ctx->refIdx4x4_C_s[i8x8] = refIdx4x4_C_base[i8x8] + (int32_t)refIdx4x4_C_inc[unavail1 & 7];
		ctx->mvs_shuffle_s[i8x8] = i4x4 * 0x01010101 + s;
		ctx->part_sizes_l[i8x8] = sizes;
		fprintf(stderr, "sub_mb_type: %c\n", (f == 1) ? '0' : (f == 5) ? '1' : (f == 3) ? '2' : '3');
	}
	ctx->mvd_flags = flags;
	CALL(parse_ref_idx);
	
	// load neighbouring refIdx values to compute mvpred directions
	v16qi neighbours = {0, *(mb->refIdx + ctx->refIdx_D), *(mb->refIdx + ctx->refIdx_B[0]),
		*(mb->refIdx + ctx->refIdx_B[1]), *(mb->refIdx + ctx->refIdx_C),
		*(mb->refIdx + ctx->refIdx_A[0]), mb->refIdx[0], mb->refIdx[1], 0,
		*(mb->refIdx + ctx->refIdx_A[2]), mb->refIdx[2], mb->refIdx[3]};
	v16qi refIdxA = byte_shuffle(neighbours, ctx->refIdx4x4_A_v);
	v16qi refIdxB = byte_shuffle(neighbours, ctx->refIdx4x4_B_v);
	v16qi refIdxC = byte_shuffle(neighbours, ctx->refIdx4x4_C_v);
	
	// compare them and store equality formula
	v16qi refIdx = __builtin_shufflevector(neighbours, neighbours, 6, 6, 6, 6, 7, 7, 7, 7, 10, 10, 10, 10, 11, 11, 11, 11);
	v16qi sum = (refIdx==refIdxA) + ((refIdx==refIdxB) << 1) + ((refIdx==refIdxC) << 2);
	ctx->refIdx4x4_eq_v[0] = -(sum | (ctx->unavail_v==14)); // if B and C are unavailable, then sum is 0 or 1
	JUMP(parse_mvs);
}

static __attribute__((noinline)) void FUNC(parse_B_mb)
{
	static const uint32_t B2flags[26] = {0x00010001, 0x00000101, 0x00000011, 0x01010000,
		0x00110000, 0x01000001, 0x00100001, 0x00010100, 0x00000001, 0x00010000,
		0, 0, 0, 0, 0x00010010, 0, 0x01000101, 0x00100011, 0x01010100, 0x00110010,
		0x00010101, 0x00010011, 0x01010001, 0x00110001, 0x01010101, 0x00110011};
	static const uint32_t b2flags[13] = {0x10001, 0x00005, 0x00003, 0x50000, 0x00001,
		0x10000, 0xf0000, 0xf000f, 0x30000, 0x50005, 0x30003, 0x0000f, 0};
	static const uint8_t B2mb_type[26] = {3, 4, 5, 6, 7, 8, 9, 10, 1, 2, 0, 0,
		0, 0, 11, 22, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
	static const uint8_t b2sub_mb_type[13] = {3, 4, 5, 6, 1, 2, 11, 12, 7, 8, 9, 10, 0};
	
	mb->f.mbIsInterFlag = 1;
	mb->Intra4x4PredMode_v = (v16qi){2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
	
	mb->f.mb_skip_flag = CALL(get_ae, 26 - ctx->inc.mb_skip_flag);
	fprintf(stderr, "mb_skip_flag: %x\n", mb->f.mb_skip_flag);
	if (mb->f.mb_skip_flag) {
		mb->CodedBlockPatternLuma_s = 0;
		mb->f.mb_type_B_Direct = 1;
		JUMP(parse_NxN_residual);
		
	// B_Direct_16x16
	} else if (!CALL(get_ae, 29 - ctx->inc.mb_type_B_Direct)) {
		fprintf(stderr, "mb_type: 0\n");
		CALL(parse_coded_block_pattern);
		if (mb->CodedBlockPatternLuma_s && ctx->ps.transform_8x8_mode_flag && ctx->ps.direct_8x8_inference_flag) {
			mb->f.transform_size_8x8_flag = CALL(get_ae, 399 + ctx->inc.transform_size_8x8_flag);
			fprintf(stderr, "transform_size_8x8_flag: %x\n", mb->f.transform_size_8x8_flag);
		}
		mb->f.mb_type_B_Direct = 1;
		JUMP(parse_NxN_residual);
	}
	
	// Most important here is the minimal number of conditional branches.
	int str = 4;
	if (!CALL(get_ae, 30) || (str = CALL(get_ae, 31),
		str += str + CALL(get_ae, 32),
		str += str + CALL(get_ae, 32),
		str += str + CALL(get_ae, 32), str - 8 < 5u))
	{
		str += str + CALL(get_ae, 32);
	}
	if (str == 13) {
		memset(mb->mvs_v, 0, sizeof(mb->mvs_v));
		JUMP(parse_I_mb, 32);
	}
	fprintf(stderr, "mb_type: %u\n", B2mb_type[str]);
	unsigned flags = B2flags[str];
	
	// Parsing for sub_mb_type in B slices.
	for (int i = 0; str == 15 && i < 16; i += 4) {
		int sub = 12;
		if (!CALL(get_ae, 36)) {
			
		} else {
			sub = 2;
			if (!CALL(get_ae, 37) || (sub = CALL(get_ae, 38),
				sub += sub + CALL(get_ae, 39),
				sub += sub + CALL(get_ae, 39), sub - 4 < 2u))
			{
				sub += sub + CALL(get_ae, 39);
			}
			flags |= b2flags[sub] << i;
		}
		fprintf(stderr, "sub_mb_type: %u\n", b2sub_mb_type[sub]);
	}
	ctx->mvd_flags = flags;
	return;
}



/**
 * Initialise the reference indices and motion vectors of an entire macroblock
 * with direct prediction (8.4.1.2).
 */
static void FUNC(init_direct_spatial_prediction)
{
	// load refIdxCol and mvCol
	Edge264_macroblock *mbCol = ctx->mbCol;
	int refCol0 = mbCol->refIdx[0];
	int refCol1 = mbCol->refIdx[1];
	int refCol2 = mbCol->refIdx[2];
	int refCol3 = mbCol->refIdx[3];
	v8hi mvCol0 = mbCol->mvs_v[0];
	v8hi mvCol1 = mbCol->mvs_v[1];
	v8hi mvCol2 = mbCol->mvs_v[2];
	v8hi mvCol3 = mbCol->mvs_v[3];
	
	// Both GCC and Clang are INCREDIBLY dumb for any attempt to use ?: here.
	if (refCol0 < 0)
		refCol0 = mbCol->refIdx[4], mvCol0 = mbCol->mvs_v[4];
	if (refCol1 < 0)
		refCol1 = mbCol->refIdx[5], mvCol1 = mbCol->mvs_v[5];
	if (refCol2 < 0)
		refCol2 = mbCol->refIdx[6], mvCol2 = mbCol->mvs_v[6];
	if (refCol3 < 0)
		refCol3 = mbCol->refIdx[7], mvCol3 = mbCol->mvs_v[7];
	
	// initialize colZeroFlags
	v8hi colZero0 = {}, colZero1 = {}, colZero2 = {}, colZero3 = {};
	if (__builtin_expect(refCol0 == ctx->zero_if_col_short_term, 1))
		colZero0 = mv_near_zero(mvCol0);
	if (__builtin_expect(refCol1 == ctx->zero_if_col_short_term, 1))
		colZero1 = mv_near_zero(mvCol1);
	if (__builtin_expect(refCol2 == ctx->zero_if_col_short_term, 1))
		colZero2 = mv_near_zero(mvCol2);
	if (__builtin_expect(refCol3 == ctx->zero_if_col_short_term, 1))
		colZero3 = mv_near_zero(mvCol3);
	
	// load all refIdxN and mvN in vector registers
	v8hi mvA = (v8hi)(v4si){*(mb->mvs_s + ctx->mvs_A[0]), *(mb->mvs_s + ctx->mvs_A[0] + 16)};
	v8hi mvB = (v8hi)(v4si){*(mb->mvs_s + ctx->mvs_B[0]), *(mb->mvs_s + ctx->mvs_B[0] + 16)};
	v8hi mvC = (v8hi)(v4si){*(mb->mvs_s + ctx->mvs8x8_C[1]), *(mb->mvs_s + ctx->mvs8x8_C[1] + 16)};
	v16qi shuf = {0, 0, 0, 0, 1, 1, 1, 1};
	v16qi refIdxA = byte_shuffle((v16qi){*(mb->refIdx + ctx->refIdx_A[0]), *(mb->refIdx + ctx->refIdx_A[0] + 4)}, shuf);
	v16qi refIdxB = byte_shuffle((v16qi){*(mb->refIdx + ctx->refIdx_B[0]), *(mb->refIdx + ctx->refIdx_B[0] + 4)}, shuf);
	v16qi refIdxC = byte_shuffle((v16qi){*(mb->refIdx + ctx->refIdx_C), *(mb->refIdx + ctx->refIdx_C + 4)}, shuf);
	if (__builtin_expect(ctx->inc.unavailable & 4, 0)) {
		mvC = (v8hi)(v4si){*(mb->mvs_s + ctx->mvs8x8_D[0]), *(mb->mvs_s + ctx->mvs8x8_D[0] + 16)};
		refIdxC = byte_shuffle((v16qi){*(mb->refIdx + ctx->refIdx_D), *(mb->refIdx + ctx->refIdx_D + 4)}, shuf);
	}
	
	// initialize mv along refIdx since it will equal one of refIdxA/B/C
	v16qu cmp_AB = (v16qu)refIdxA < (v16qu)refIdxB; // unsigned comparisons
	v16qi refIdxm = vector_select(cmp_AB, refIdxA, refIdxB); // umin(refIdxA, refIdxB)
	v16qi refIdxM = vector_select(cmp_AB, refIdxB, refIdxA); // umax(refIdxA, refIdxB)
	v8hi mvm = vector_select(cmp_AB, mvA, mvB);
	v16qu cmp_mC = (v16qu)refIdxm < (v16qu)refIdxC;
	v16qi refIdx = vector_select(cmp_mC, refIdxm, refIdxC); // umin(refIdxm, refIdxC)
	v8hi mvmm = vector_select(cmp_mC, mvm, mvC);
	
	// select median if refIdx equals one more of refIdxA/B/C
	v16qi cmp_med = (refIdxm == refIdxC) | (refIdx == refIdxM);
	v8hi mv01 = vector_select(cmp_med, vector_median(mvA, mvB, mvC), mvmm);
	v8hi mvs0 = (v8hi)__builtin_shufflevector((v4si)mv01, (v4si)mv01, 0, 0, 0, 0);
	v8hi mvs1 = (v8hi)__builtin_shufflevector((v4si)mv01, (v4si)mv01, 1, 1, 1, 1);
	
	// direct zero prediction applies only to refIdx (mvLX are zero already)
	refIdx &= (v16qi)((v2li)refIdx != -1);
	mb->refIdx_l = ((v2li)refIdx)[0];
	
	// mask mvs with colZeroFlags and store them
	if (refIdx[0] == 0) {
		mb->mvs_v[0] = mvs0 & ~colZero0;
		mb->mvs_v[1] = mvs0 & ~colZero1;
		mb->mvs_v[2] = mvs0 & ~colZero2;
		mb->mvs_v[3] = mvs0 & ~colZero3;
	} else {
		mb->mvs_v[0] = mb->mvs_v[1] = mb->mvs_v[2] = mb->mvs_v[3] = mvs0;
	}
	if (refIdx[4] == 0) {
		mb->mvs_v[4] = mvs1 & ~colZero0;
		mb->mvs_v[5] = mvs1 & ~colZero1;
		mb->mvs_v[6] = mvs1 & ~colZero2;
		mb->mvs_v[7] = mvs1 & ~colZero3;
	} else {
		mb->mvs_v[4] = mb->mvs_v[5] = mb->mvs_v[6] = mb->mvs_v[7] = mvs1;
	}
}

static void FUNC(init_direct_temporal_prediction)
{
	// load refIdxCol and mvCol
	Edge264_macroblock *mbCol = ctx->mbCol;
	int refCol0 = mbCol->refIdx[0];
	int refCol1 = mbCol->refIdx[1];
	int refCol2 = mbCol->refIdx[2];
	int refCol3 = mbCol->refIdx[3];
	v8hi mvCol0 = mbCol->mvs_v[0];
	v8hi mvCol1 = mbCol->mvs_v[1];
	v8hi mvCol2 = mbCol->mvs_v[2];
	v8hi mvCol3 = mbCol->mvs_v[3];
	
	// Both GCC and Clang are INCREDIBLY dumb for any attempt to use ?: here.
	if (refCol0 < 0)
		refCol0 = mbCol->refIdx[4], mvCol0 = mbCol->mvs_v[4];
	if (refCol1 < 0)
		refCol1 = mbCol->refIdx[5], mvCol1 = mbCol->mvs_v[5];
	if (refCol2 < 0)
		refCol2 = mbCol->refIdx[6], mvCol2 = mbCol->mvs_v[6];
	if (refCol3 < 0)
		refCol3 = mbCol->refIdx[7], mvCol3 = mbCol->mvs_v[7];
	
	// with precomputed constants the rest is straightforward
	mb->refIdx[0] = ctx->MapColToList0[1 + refCol0];
	mb->refIdx[1] = ctx->MapColToList0[1 + refCol1];
	mb->refIdx[2] = ctx->MapColToList0[1 + refCol2];
	mb->refIdx[3] = ctx->MapColToList0[1 + refCol3];
	mb->refIdx_s[1] = 0;
	mb->mvs_v[0] = temporal_scale(mvCol0, ctx->DistScaleFactor[mb->refIdx[0]]);
	mb->mvs_v[1] = temporal_scale(mvCol1, ctx->DistScaleFactor[mb->refIdx[1]]);
	mb->mvs_v[2] = temporal_scale(mvCol2, ctx->DistScaleFactor[mb->refIdx[2]]);
	mb->mvs_v[3] = temporal_scale(mvCol3, ctx->DistScaleFactor[mb->refIdx[3]]);
	mb->mvs_v[4] = mb->mvs_v[0] - mvCol0;
	mb->mvs_v[5] = mb->mvs_v[1] - mvCol1;
	mb->mvs_v[6] = mb->mvs_v[2] - mvCol2;
	mb->mvs_v[7] = mb->mvs_v[3] - mvCol3;
}



/**
 * This function loops through the macroblocks of a slice, initialising their
 * data and calling parse_inter/intra_mb for each one.
 */
__attribute__((noinline)) void FUNC(parse_slice_data)
{
	static const v16qi block_unavailability[4] = {
		{ 0,  0,  0,  4,  0,  0,  0,  4,  0,  0,  0,  4,  0,  4,  0,  4},
		{ 1,  0,  9,  4,  0,  0,  0,  4,  9,  0,  9,  4,  0,  4,  0,  4},
		{ 6, 14,  0,  4, 14, 10,  0,  4,  0,  0,  0,  4,  0,  4,  0,  4},
		{ 7, 14,  9,  4, 14, 10,  0,  4,  9,  0,  9,  4,  0,  4,  0,  4},
	};
	
	ctx->mb_qp_delta_non_zero = 0;
	while (1) {
		fprintf(stderr, "********** %u **********\n", ctx->CurrMbAddr);
		Edge264_macroblock *mbB = mb - (ctx->ps.width >> 4) - 1;
		v16qi flagsA = mb[-1].f.v;
		v16qi flagsB = mbB->f.v;
		ctx->inc.v = flagsA + flagsB + (flagsB & flags_twice.v);
		memset(mb, 0, sizeof(*mb));
		mb->f.mb_field_decoding_flag = ctx->field_pic_flag;
		CALL(check_ctx, LOOP_START_LABEL);
		
		// prepare block unavailability information (6.4.11.4)
		ctx->unavail_v = block_unavailability[ctx->inc.unavailable];
		if (mbB[1].f.unavailable) {
			ctx->inc.unavailable += 4;
			ctx->unavail[5] += 4;
		}
		if (mbB[-1].f.unavailable) {
			ctx->inc.unavailable += 8;
			ctx->unavail[0] += 8;
		}
		
		// Would it actually help to push this test outside the loop?
		if (ctx->slice_type == 0) {
			CALL(parse_P_mb);
		} else if (ctx->slice_type == 1) {
			CALL(parse_B_mb);
		} else {
			CALL(parse_I_mb, 5 - ctx->inc.mb_type_I_NxN);
		}
		
		// break on end_of_slice_flag
		int end_of_slice_flag = CALL(get_ae, 276);
		fprintf(stderr, "end_of_slice_flag: %x\n\n", end_of_slice_flag);
		if (end_of_slice_flag)
			break;
		
		// point to the next macroblock
		mb++;
		ctx->CurrMbAddr++;
		int xY = (ctx->frame_offsets_x[4] - ctx->frame_offsets_x[0]) << 1;
		int xC = (ctx->frame_offsets_x[20] - ctx->frame_offsets_x[16]) << 1;
		int end_of_line = (ctx->frame_offsets_x[0] + xY >= ctx->stride_Y);
		ctx->frame_offsets_x_v[0] = ctx->frame_offsets_x_v[1] +=
			(v8hu){xY, xY, xY, xY, xY, xY, xY, xY};
		ctx->frame_offsets_x_v[2] = ctx->frame_offsets_x_v[3] = ctx->frame_offsets_x_v[4] = ctx->frame_offsets_x_v[5] +=
			(v8hu){xC, xC, xC, xC, xC, xC, xC, xC};
		if (!end_of_line)
			continue;
		
		// reaching the end of a line
		if (ctx->frame_offsets_y[10] + ctx->stride_Y * 4 >= ctx->plane_size_Y)
			break;
		mb++; // skip the empty macroblock at the edge
		ctx->frame_offsets_x_v[0] = ctx->frame_offsets_x_v[1] -=
			(v8hu){ctx->stride_Y, ctx->stride_Y, ctx->stride_Y, ctx->stride_Y, ctx->stride_Y, ctx->stride_Y, ctx->stride_Y, ctx->stride_Y};
		ctx->frame_offsets_x_v[2] = ctx->frame_offsets_x_v[3] = ctx->frame_offsets_x_v[4] = ctx->frame_offsets_x_v[5] -=
			(v8hu){ctx->stride_C, ctx->stride_C, ctx->stride_C, ctx->stride_C, ctx->stride_C, ctx->stride_C, ctx->stride_C, ctx->stride_C};
		v4si YY = (v4si){ctx->stride_Y, ctx->stride_Y, ctx->stride_Y, ctx->stride_Y} << 4;
		int yC = (ctx->frame_offsets_y[24] - ctx->frame_offsets_y[16]) << 1;
		v4si YC = (v4si){yC, yC, yC, yC};
		ctx->frame_offsets_y_v[0] = ctx->frame_offsets_y_v[1] += YY;
		ctx->frame_offsets_y_v[2] = ctx->frame_offsets_y_v[3] += YY;
		ctx->frame_offsets_y_v[4] = ctx->frame_offsets_y_v[5] += YC;
		ctx->frame_offsets_y_v[6] = ctx->frame_offsets_y_v[7] += YC;
		ctx->frame_offsets_y_v[8] = ctx->frame_offsets_y_v[9] += YC;
		ctx->frame_offsets_y_v[10] = ctx->frame_offsets_y_v[11] += YC;
	}
}
