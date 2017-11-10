// TODO: Add testing of borders from ctx
// TODO: Optimise _mm_set_epi64?
// TODO: Add 1px unused line atop the first picture to avoid testing forbidden reads
// TODO: uninline loads?
// TODO: Make 4x4 two-pass too, and gather all _mm_setzero_si128()
// TODO: Decrement p before all!
// TODO: Compare execution times as inline vs noinline
// TODO: Reorder enums to separate hot&cold paths
// TODO: Reorder instructions to put load8_up0_8bit last whenever possible
// TODO: Fix _mm_movpi64_epi64 with GCC
// TODO: load8 function calls force many stack spills!
// TODO: Review functions for a last optimisation pass against HADD
// TODO: Try to replace loaded constants with computable ones
// TODO: Change convention to left-to-top-right arguments
/**
 * Intra decoding involves so many shuffling tricks that it is better expressed
 * as native intrinsics, where each architecture can give its best.
 *
 * Choosing between the different possibilities of a same function is tricky,
 * in general I favor in order:
 * _ the fastest code, obviously (http://www.agner.org/optimize/#manual_instr_tab),
 * _ a short dependency chain (instructions are pipelined in parallel),
 * _ smaller code+data (avoid excessive use of pshufb),
 * _ readable code (helped by Intel's astounding instrinsics naming...).
 */

#include "edge264_common.h"

int decode_Residual4x4(__m128i, __m128i);
int decode_Residual8x8(__m128i, __m128i, __m128i, __m128i, __m128i, __m128i, __m128i, __m128i);
int decode_Residual8x8_8bit(__m128i, __m128i, __m128i, __m128i, __m128i, __m128i, __m128i, __m128i);
int decode_ResidualDC4_8bit();

static inline void print_v8hi(__m128i x) {
	for (int i = 0; i < 8; i++)
		printf("%3d ", ((v8hi)x)[i]);
	printf("\n");
}

static inline __m128i lowpass(__m128i left, __m128i mid, __m128i right) {
	return _mm_avg_epu16(_mm_srli_epi16(_mm_add_epi16(left, right), 1), mid);
}

// returns Intra8x8 filtered samples p'[-1,0] to p'[-1,7]
static __attribute__((noinline)) __m128i filter8_left_8bit(size_t stride, ssize_t nstride, uint8_t *p, __m128i zero, ssize_t lt) {
	uint8_t *q = p + stride * 7;
	__m64 m0 = _mm_unpackhi_pi8(*(__m64 *)(p + nstride     - 8), *(__m64 *)(p               - 8));
	__m64 m1 = _mm_unpackhi_pi8(*(__m64 *)(p +  stride     - 8), *(__m64 *)(p +  stride * 2 - 8));
	__m64 m2 = _mm_unpackhi_pi8(*(__m64 *)(q + nstride * 4 - 8), *(__m64 *)(p +  stride * 4 - 8));
	__m64 m3 = _mm_unpackhi_pi8(*(__m64 *)(q + nstride * 2 - 8), *(__m64 *)(q + nstride     - 8));
	__m64 m4 = _mm_unpackhi_pi32(_mm_unpackhi_pi16(m0, m1), _mm_unpackhi_pi16(m2, m3));
	__m64 m5 = _mm_alignr_pi8(m4, *(__m64 *)(p + lt - 8), 7);
	__m128i x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(m4), zero);
	__m128i x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(m5), zero);
	__m128i x2 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
	return lowpass(x1, x0, x2);
}
static __attribute__((noinline)) __m128i filter8_left_16bit(size_t stride, ssize_t nstride, uint8_t *p, ssize_t lt) {
	uint8_t *q = p + stride * 7;
	__m128i x0 = _mm_unpackhi_epi16(*(__m128i *)(p + nstride     - 16), *(__m128i *)(p               - 16));
	__m128i x1 = _mm_unpackhi_epi16(*(__m128i *)(p +  stride     - 16), *(__m128i *)(p +  stride * 2 - 16));
	__m128i x2 = _mm_unpackhi_epi16(*(__m128i *)(q + nstride * 4 - 16), *(__m128i *)(p +  stride * 4 - 16));
	__m128i x3 = _mm_unpackhi_epi16(*(__m128i *)(q + nstride * 2 - 16), *(__m128i *)(q + nstride     - 16));
	__m128i x4 = _mm_unpackhi_epi64(_mm_unpackhi_epi32(x0, x1), _mm_unpackhi_epi32(x2, x3));
	__m128i x5 = _mm_alignr_epi8(x4, *(__m128i *)(p + lt - 16), 14);
	__m128i x6 = _mm_shufflehi_epi16(_mm_srli_si128(x4, 2), _MM_SHUFFLE(2, 2, 1, 0));
	return lowpass(x5, x4, x6);
}

// filters Intra8x8 samples p'[-1,7] to p'[-1,-1] to PredBuffer, and returns p'[0,-1] to p'[7,-1]
static __attribute__((noinline)) __m128i filter8_top_left_8bit(size_t stride, ssize_t nstride, uint8_t *p, __m128i zero, ssize_t lt, __m128i top) {
	uint8_t *q = p + stride * 7;
	__m64 m0 = _mm_unpackhi_pi8(*(__m64 *)(p + nstride     - 8), *(__m64 *)(p +      lt     - 8));
	__m64 m1 = _mm_unpackhi_pi8(*(__m64 *)(p +  stride     - 8), *(__m64 *)(p +             - 8));
	__m64 m2 = _mm_unpackhi_pi8(*(__m64 *)(q + nstride * 4 - 8), *(__m64 *)(p +  stride * 2 - 8));
	__m64 m3 = _mm_unpackhi_pi8(*(__m64 *)(q + nstride * 2 - 8), *(__m64 *)(p +  stride * 4 - 8));
	__m64 m4 = _mm_unpackhi_pi32(_mm_unpackhi_pi16(m3, m2), _mm_unpackhi_pi16(m1, m0));
	__m128i x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(q + nstride     - 8)), zero);
	__m128i x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(m4), zero);
	__m128i x2 = _mm_alignr_epi8(x1, x0, 14);
	__m128i x3 = _mm_alignr_epi8(x2, x0, 14);
	__m128i x4 = _mm_unpacklo_epi8(top, zero);
	__m128i x5 = _mm_unpacklo_epi8(_mm_srli_si128(top, 1), zero);
	__m128i x6 = _mm_unpacklo_epi8(_mm_srli_si128(top, 2), zero);
	ctx->pred_buffer[0] = (v8hi)lowpass(x1, x2, x3);
	((uint16_t *)ctx->pred_buffer)[8] = (p[nstride - 1] + p[nstride * 2 - 1] * 2 + p[nstride * 2] + 2) >> 2;
	return lowpass(x4, x5, x6);
}
static __attribute__((noinline)) __m128i filter8_top_left_16bit(size_t stride, ssize_t nstride, uint8_t *p, __m128i zero, ssize_t lt, __m128i tr, __m128i tl) {
	uint8_t *q = p + stride * 7;
	__m128i x0 = _mm_unpackhi_epi16(*(__m128i *)(p + nstride     - 16), *(__m128i *)(p +      lt     - 16));
	__m128i x1 = _mm_unpackhi_epi16(*(__m128i *)(p +  stride     - 16), *(__m128i *)(p +             - 16));
	__m128i x2 = _mm_unpackhi_epi16(*(__m128i *)(q + nstride * 4 - 16), *(__m128i *)(p +  stride * 2 - 16));
	__m128i x3 = _mm_unpackhi_epi16(*(__m128i *)(q + nstride * 2 - 16), *(__m128i *)(p +  stride * 4 - 16));
	__m128i x4 = _mm_unpackhi_epi64(_mm_unpackhi_epi32(x3, x2), _mm_unpackhi_epi32(x1, x0));
	__m128i x5 = _mm_alignr_epi8(x4, *(__m128i *)(q + nstride     - 16), 14);
	__m128i x6 = _mm_alignr_epi8(x5, *(__m128i *)(q + nstride     - 16), 14);
	ctx->pred_buffer[0] = (v8hi)lowpass(x4, x5, x6);
	((uint16_t *)ctx->pred_buffer)[8] = (*(uint16_t *)(p + nstride - 2) + *(uint16_t *)(p + nstride * 2 - 2) * 2 + *(uint16_t *)(p + nstride * 2) + 2) >> 2;
	return lowpass(tl, *(__m128i *)(p + nstride * 2), tr);
}

// returns packed samples p[-1,0] to p[-1,15]
static __attribute__((noinline)) __m128i load16_left_8bit(size_t stride, ssize_t nstride, uint8_t *p) {
	uint8_t *q = p + stride * 7;
	uint8_t *r = q + stride * 7;
	__m128i x0 = _mm_unpackhi_epi8(*(__m128i *)(p + nstride     - 16), *(__m128i *)(p               - 8));
	__m128i x1 = _mm_unpackhi_epi8(*(__m128i *)(p +  stride     - 16), *(__m128i *)(p +  stride * 2 - 8));
	__m128i x2 = _mm_unpackhi_epi8(*(__m128i *)(q + nstride * 4 - 16), *(__m128i *)(p +  stride * 4 - 8));
	__m128i x3 = _mm_unpackhi_epi8(*(__m128i *)(q + nstride * 2 - 16), *(__m128i *)(q + nstride     - 8));
	__m128i x4 = _mm_unpackhi_epi8(*(__m128i *)(q +             - 16), *(__m128i *)(q +  stride     - 8));
	__m128i x5 = _mm_unpackhi_epi8(*(__m128i *)(q +  stride * 2 - 16), *(__m128i *)(r + nstride * 4 - 8));
	__m128i x6 = _mm_unpackhi_epi8(*(__m128i *)(q +  stride * 4 - 16), *(__m128i *)(r + nstride * 2 - 8));
	__m128i x7 = _mm_unpackhi_epi8(*(__m128i *)(r + nstride     - 16), *(__m128i *)(r               - 8));
	__m128i x8 = _mm_unpackhi_epi32(_mm_unpackhi_epi16(x0, x1), _mm_unpackhi_epi16(x2, x3));
	__m128i x9 = _mm_unpackhi_epi32(_mm_unpackhi_epi16(x4, x5), _mm_unpackhi_epi16(x6, x7));
	return _mm_unpackhi_epi64(x8, x9);
}
static __attribute__((noinline)) __m128i load16_left_16bit(size_t stride, ssize_t nstride, uint8_t *p) {
	uint8_t *q = p + stride * 7;
	uint8_t *r = q + stride * 7;
	__m128i x0 = _mm_unpackhi_epi16(*(__m128i *)(p + nstride     - 16), *(__m128i *)(p               - 8));
	__m128i x1 = _mm_unpackhi_epi16(*(__m128i *)(p +  stride     - 16), *(__m128i *)(p +  stride * 2 - 8));
	__m128i x2 = _mm_unpackhi_epi16(*(__m128i *)(q + nstride * 4 - 16), *(__m128i *)(p +  stride * 4 - 8));
	__m128i x3 = _mm_unpackhi_epi16(*(__m128i *)(q + nstride * 2 - 16), *(__m128i *)(q + nstride     - 8));
	__m128i x4 = _mm_unpackhi_epi16(*(__m128i *)(q +             - 16), *(__m128i *)(q +  stride     - 8));
	__m128i x5 = _mm_unpackhi_epi16(*(__m128i *)(q +  stride * 2 - 16), *(__m128i *)(r + nstride * 4 - 8));
	__m128i x6 = _mm_unpackhi_epi16(*(__m128i *)(q +  stride * 4 - 16), *(__m128i *)(r + nstride * 2 - 8));
	__m128i x7 = _mm_unpackhi_epi16(*(__m128i *)(r + nstride     - 16), *(__m128i *)(r               - 8));
	ctx->pred_buffer[0] = (v8hi)_mm_unpackhi_epi64(_mm_unpackhi_epi32(x4, x5), _mm_unpackhi_epi32(x6, x7));
	return _mm_unpackhi_epi64(_mm_unpackhi_epi32(x0, x1), _mm_unpackhi_epi32(x2, x3));
}



/**
 * Intra4x4 modes must execute extremely fast.
 * 8/16bit paths are merged when possible.
 */
static int decode_Horizontal4x4_8bit(size_t stride, ssize_t nstride, uint8_t *p) {
	static const v16qi shuf = {3, -1, 3, -1, 3, -1, 3, -1, 11, -1, 11, -1, 11, -1, 11, -1};
	__m128i x0 = _mm_set_epi64(*(__m64 *)(p +             - 4), *(__m64 *)(p + nstride     - 4));
	__m128i x1 = _mm_set_epi64(*(__m64 *)(p +  stride * 2 - 4), *(__m64 *)(p +  stride     - 4));
	__m128i x2 = _mm_shuffle_epi8(x0, (__m128i)shuf);
	__m128i x3 = _mm_shuffle_epi8(x1, (__m128i)shuf);
	return decode_Residual4x4(x2, x3);
}

static int decode_Horizontal4x4_16bit(size_t stride, ssize_t nstride, uint8_t *p) {
	__m128i x0 = _mm_set_epi64(*(__m64 *)(p +             - 8), *(__m64 *)(p + nstride     - 8));
	__m128i x1 = _mm_set_epi64(*(__m64 *)(p +  stride * 2 - 8), *(__m64 *)(p +  stride     - 8));
	__m128i x2 = _mm_shufflelo_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3));
	__m128i x3 = _mm_shufflelo_epi16(x1, _MM_SHUFFLE(3, 3, 3, 3));
	__m128i x4 = _mm_shufflehi_epi16(x2, _MM_SHUFFLE(3, 3, 3, 3));
	__m128i x5 = _mm_shufflehi_epi16(x3, _MM_SHUFFLE(3, 3, 3, 3));
	return decode_Residual4x4(x4, x5);
}

static int decode_DC4_8bit_8bit(__m128i zero, __m128i x0) {
	__m128i x1 = _mm_avg_epu16(_mm_srli_epi16(_mm_sad_epu8(x0, zero), 2), zero);
	__m128i DC = _mm_broadcastw_epi16(x1);
	return decode_Residual4x4(DC, DC);
}

static int decode_DiagonalDownLeft4x4(__m128i top) {
	__m128i x0 = _mm_srli_si128(top, 2);
	__m128i x1 = _mm_shufflehi_epi16(_mm_shuffle_epi32(top, _MM_SHUFFLE(3, 3, 2, 1)), _MM_SHUFFLE(1, 1, 1, 0));
	__m128i x2 = lowpass(top, x0, x1);
	__m128i x3 = _mm_srli_si128(x2, 2);
	__m128i x4 = (__m128i)_mm_shuffle_ps((__m128)x2, (__m128)x3, _MM_SHUFFLE(1, 0, 1, 0));
	__m128i x5 = (__m128i)_mm_shuffle_ps((__m128)x2, (__m128)x3, _MM_SHUFFLE(2, 1, 2, 1));
	return decode_Residual4x4(x4, x5);
}

static int decode_DiagonalDownRight4x4(__m128i lt, __m128i bot) {
	__m128i x0 = _mm_slli_si128(lt, 2);
	__m128i x1 = _mm_alignr_epi8(lt, bot, 12);
	__m128i x2 = lowpass(lt, x0, x1);
	__m128i x3 = _mm_slli_si128(x2, 2);
	__m128i x4 = (__m128i)_mm_shuffle_ps((__m128)x2, (__m128)x3, _MM_SHUFFLE(3, 2, 3, 2));
	__m128i x5 = (__m128i)_mm_shuffle_ps((__m128)x2, (__m128)x3, _MM_SHUFFLE(2, 1, 2, 1));
	return decode_Residual4x4(x4, x5);
}

static int decode_VerticalRight4x4(__m128i lt) {
	__m128i x0 = _mm_slli_si128(lt, 2);
	__m128i x1 = _mm_shuffle_epi32(lt, _MM_SHUFFLE(2, 1, 0, 0));
	__m128i x2 = _mm_avg_epu16(lt, x0);
	__m128i x3 = lowpass(lt, x0, x1);
	__m128i x4 = (__m128i)_mm_shuffle_ps((__m128)x3, (__m128)x2, _MM_SHUFFLE(3, 2, 1, 0));
	__m128i x5 = _mm_shufflelo_epi16(x3, _MM_SHUFFLE(2, 0, 0, 0));
	__m128i x6 = _mm_unpackhi_epi64(x2, x3);
	__m128i x7 = _mm_unpackhi_epi64(_mm_slli_si128(x4, 2), _mm_slli_si128(x5, 2));
	return decode_Residual4x4(x6, x7);
}

static int decode_HorizontalDown4x4(__m128i lt) {
	__m128i x0 = _mm_srli_si128(lt, 2);
	__m128i x1 = _mm_shuffle_epi32(lt, _MM_SHUFFLE(3, 3, 2, 1));
	__m128i x2 = _mm_avg_epu16(lt, x0);
	__m128i x3 = lowpass(lt, x0, x1);
	__m128i x4 = _mm_unpacklo_epi16(x2, x3);
	__m128i x5 = _mm_shuffle_epi32(_mm_unpackhi_epi64(x3, x4), _MM_SHUFFLE(3, 2, 0, 3));
	__m128i x6 = _mm_shuffle_epi32(x4, _MM_SHUFFLE(1, 0, 2, 1));
	return decode_Residual4x4(x5, x6);
}

static int decode_VerticalLeft4x4(__m128i top) {
	__m128i x0 = _mm_srli_si128(top, 2);
	__m128i x1 = _mm_shufflehi_epi16(_mm_shuffle_epi32(top, _MM_SHUFFLE(3, 3, 2, 1)), _MM_SHUFFLE(1, 1, 1, 0));
	__m128i x2 = _mm_avg_epu16(top, x0);
	__m128i x3 = lowpass(top, x0, x1);
	__m128i x4 = _mm_unpacklo_epi64(x2, x3);
	__m128i x5 = _mm_unpacklo_epi64(_mm_srli_si128(x2, 2), _mm_srli_si128(x3, 2));
	return decode_Residual4x4(x4, x5);
}

static int decode_HorizontalUp4x4_8bit(size_t stride, ssize_t nstride, uint8_t *p, __m128i zero) {
	__m64 m0 = _mm_unpacklo_pi8(*(__m64 *)(p + nstride     - 4), *(__m64 *)(p +             - 4));
	__m64 m1 = _mm_unpacklo_pi8(*(__m64 *)(p +  stride     - 4), *(__m64 *)(p +  stride * 2 - 4));
	__m64 m2 = _mm_unpackhi_pi16(m0, m1);
	__m128i x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(m2), zero);
	__m128i x1 = _mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 2, 1));
	__m128i x2 = _mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 2));
	__m128i x3 = _mm_avg_epu16(x0, x1);
	__m128i x4 = lowpass(x0, x1, x2);
	__m128i x5 = _mm_unpackhi_epi16(x3, x4);
	__m128i x6 = _mm_shuffle_epi32(x5, _MM_SHUFFLE(2, 1, 1, 0));
	__m128i x7 = _mm_shuffle_epi32(x5, _MM_SHUFFLE(3, 3, 3, 2));
	return decode_Residual4x4(x6, x7);
}

static int decode_HorizontalUp4x4_16bit(size_t stride, ssize_t nstride, uint8_t *p) {
   __m64 m0 = _mm_shuffle_pi16(*(__m64 *)(p +  stride * 2 - 8), _MM_SHUFFLE(3, 3, 3, 3));
   __m64 m1 = _mm_alignr_pi8(m0, *(__m64 *)(p +  stride     - 8), 6);
   __m64 m2 = _mm_alignr_pi8(m1, *(__m64 *)(p               - 8), 6);
   __m64 m3 = _mm_alignr_pi8(m2, *(__m64 *)(p + nstride     - 8), 6);
   __m64 m4 = _mm_avg_pu16(m2, m3);
	__m64 m5 =  _mm_avg_pu16(_mm_srli_pi16(_mm_add_pi16(m1, m3), 1), m2);
   __m128i x0 = _mm_unpacklo_epi16(_mm_movpi64_epi64(m4), _mm_movpi64_epi64(m5));
   __m128i x1 = _mm_shuffle_epi32(x0, _MM_SHUFFLE(2, 1, 1, 0));
   __m128i x2 = _mm_shuffle_epi32(x0, _MM_SHUFFLE(3, 3, 3, 2));
   return decode_Residual4x4(x1, x2);
}



/**
 * Intra8x8 has wide functions and many variations of the same prediction
 * modes, so we care a lot about sharing code.
 */
static int decode_Vertical8x8(__m128i topr, __m128i topm, __m128i topl) {
	__m128i x0 = lowpass(topr, topm, topl);
	return decode_Residual8x8(x0, x0, x0, x0, x0, x0, x0, x0);
}

static int decode_Horizontal8x8(__m128i left) {
	__m128i x0 = _mm_unpacklo_epi16(left, left);
	__m128i x1 = _mm_unpackhi_epi16(left, left);
	__m128i x2 = _mm_shuffle_epi32(x0, _MM_SHUFFLE(0, 0, 0, 0));
	__m128i x3 = _mm_shuffle_epi32(x0, _MM_SHUFFLE(1, 1, 1, 1));
	__m128i x4 = _mm_shuffle_epi32(x0, _MM_SHUFFLE(2, 2, 2, 2));
	__m128i x5 = _mm_shuffle_epi32(x0, _MM_SHUFFLE(3, 3, 3, 3));
	__m128i x6 = _mm_shuffle_epi32(x1, _MM_SHUFFLE(0, 0, 0, 0));
	__m128i x7 = _mm_shuffle_epi32(x1, _MM_SHUFFLE(1, 1, 1, 1));
	__m128i x8 = _mm_shuffle_epi32(x1, _MM_SHUFFLE(2, 2, 2, 2));
	__m128i x9 = _mm_shuffle_epi32(x1, _MM_SHUFFLE(3, 3, 3, 3));
	return decode_Residual8x8(x2, x3, x4, x5, x6, x7, x8, x9);
}

static int decode_DC8x8_8bit(__m128i top, __m128i left) {
	__m128i zero = _mm_setzero_si128();
	__m128i x0 = _mm_sad_epu8(_mm_packus_epi16(top, left), zero);
	__m128i x1 = _mm_add_epi16(x0, _mm_shuffle_epi32(x0, _MM_SHUFFLE(3, 2, 3, 2)));
	__m128i DC = _mm_broadcastw_epi16(_mm_avg_epu16(_mm_srli_epi16(x1, 3), zero));
	return decode_Residual8x8(DC, DC, DC, DC, DC, DC, DC, DC);
}

static int decode_DC8x8_16bit(__m128i top, __m128i left) {
	__m128i zero = _mm_setzero_si128();
	__m128i x0 = _mm_add_epi16(top, left);
	__m128i x1 = _mm_add_epi32(_mm_unpacklo_epi16(x0, zero), _mm_unpackhi_epi16(x0, zero));
	__m128i x2 = _mm_add_epi32(x1, _mm_shuffle_epi32(x1, _MM_SHUFFLE(1, 0, 3, 2)));
	__m128i x3 = _mm_add_epi32(x2, _mm_shuffle_epi32(x2, _MM_SHUFFLE(2, 3, 0, 1)));
	__m128i x4 = _mm_srli_epi32(x3, 3);
	__m128i DC = _mm_avg_epu16(_mm_packs_epi32(x4, x4), zero);
	return decode_Residual8x8(DC, DC, DC, DC, DC, DC, DC, DC);
}

static int decode_DiagonalDownLeft8x8(__m128i right, __m128i top, __m128i topl) {
	__m128i topr = _mm_alignr_epi8(right, top, 2);
	__m128i rightl = _mm_alignr_epi8(right, top, 14);
	__m128i rightr = _mm_shufflehi_epi16(_mm_srli_si128(right, 2), _MM_SHUFFLE(2, 2, 1, 0));
	__m128i x0 = lowpass(topl, top, topr);
	__m128i x1 = lowpass(rightl, right, rightr);
	__m128i x2 = _mm_alignr_epi8(x1, x0, 2);
	__m128i x3 = _mm_alignr_epi8(x1, x0, 4);
	__m128i x4 = _mm_srli_si128(x1, 2);
	__m128i x5 = _mm_shufflehi_epi16(_mm_shuffle_epi32(x1, _MM_SHUFFLE(3, 3, 2, 1)), _MM_SHUFFLE(1, 1, 1, 0));
	__m128i x6 = lowpass(x0, x2, x3);
	__m128i x7 = lowpass(x1, x4, x5);
	__m128i x8 = _mm_alignr_epi8(x7, x6, 2);
	__m128i x9 = _mm_alignr_epi8(x7, x6, 4);
	__m128i xA = _mm_alignr_epi8(x7, x6, 6);
	__m128i xB = _mm_alignr_epi8(x7, x6, 8);
	__m128i xC = _mm_alignr_epi8(x7, x6, 10);
	__m128i xD = _mm_alignr_epi8(x7, x6, 12);
	__m128i xE = _mm_alignr_epi8(x7, x6, 14);
	return decode_Residual8x8(x6, x8, x9, xA, xB, xC, xD, xE);
}

static int decode_DiagonalDownRight8x8(__m128i top) {
	__m128i left = (__m128i)ctx->pred_buffer[0];
	__m128i lt = _mm_lddqu_si128((__m128i *)((uint16_t *)ctx->pred_buffer + 1));
	__m128i lb = _mm_slli_si128(left, 2);
	__m128i tl = _mm_alignr_epi8(top, lt, 14);
	__m128i tll = _mm_alignr_epi8(top, lt, 12);
	__m128i x0 = lowpass(lb, left, lt);
	__m128i x1 = lowpass(tll, tl, top);
	__m128i x2 = _mm_alignr_epi8(x1, x0, 14);
	__m128i x3 = _mm_alignr_epi8(x1, x0, 12);
	__m128i x4 = _mm_alignr_epi8(x1, x0, 10);
	__m128i x5 = _mm_alignr_epi8(x1, x0, 8);
	__m128i x6 = _mm_alignr_epi8(x1, x0, 6);
	__m128i x7 = _mm_alignr_epi8(x1, x0, 4);
	__m128i x8 = _mm_alignr_epi8(x1, x0, 2);
	return decode_Residual8x8(x1, x2, x3, x4, x5, x6, x7, x8);
}

static int decode_VerticalRight8x8(__m128i top) {
	__m128i lt = _mm_lddqu_si128((__m128i *)((uint16_t *)ctx->pred_buffer + 1));
	__m128i x0 = _mm_slli_si128(lt, 2);
	__m128i x1 = _mm_shuffle_epi32(lt, _MM_SHUFFLE(2, 1, 0, 0));
	__m128i x2 = _mm_alignr_epi8(top, lt, 14);
	__m128i x3 = _mm_alignr_epi8(top, lt, 12);
	__m128i x4 = _mm_avg_epu16(top, x2);
	__m128i x5 = lowpass(lt, x0, x1);
	__m128i x6 = lowpass(top, x2, x3);
	__m128i x7 = _mm_alignr_epi8(x4, x5, 14);
	__m128i x8 = _mm_alignr_epi8(x6, x5 = _mm_slli_si128(x5, 2), 14);
	__m128i x9 = _mm_alignr_epi8(x7, x5 = _mm_slli_si128(x5, 2), 14);
	__m128i xA = _mm_alignr_epi8(x8, x5 = _mm_slli_si128(x5, 2), 14);
	__m128i xB = _mm_alignr_epi8(x9, x5 = _mm_slli_si128(x5, 2), 14);
	__m128i xC = _mm_alignr_epi8(xA, _mm_slli_si128(x5, 2), 14);
	return decode_Residual8x8(x4, x6, x7, x8, x9, xA, xB, xC);
}

static int decode_HorizontalDown8x8(__m128i top) {
	__m128i left = (__m128i)ctx->pred_buffer[0];
	__m128i topl = _mm_alignr_epi8(top, _mm_lddqu_si128((__m128i *)((uint16_t *)ctx->pred_buffer + 1)), 14);
	__m128i x0 = _mm_alignr_epi8(topl, left, 2);
	__m128i x1 = _mm_alignr_epi8(topl, left, 4);
	__m128i x2 = _mm_srli_si128(topl, 2);
	__m128i x3 = _mm_shuffle_epi32(topl, _MM_SHUFFLE(3, 3, 2, 1));
	__m128i x4 = _mm_avg_epu16(left, x0);
	__m128i x5 = lowpass(left, x0, x1);
	__m128i x6 = lowpass(topl, x2, x3);
	__m128i x7 = _mm_unpackhi_epi16(x4, x5);
	__m128i x8 = _mm_unpacklo_epi16(x4, x5);
	__m128i x9 = _mm_alignr_epi8(x6, x7, 12);
	__m128i xA = _mm_alignr_epi8(x6, x7, 8);
	__m128i xB = _mm_alignr_epi8(x6, x7, 4);
	__m128i xC = _mm_alignr_epi8(x7, x8, 12);
	__m128i xD = _mm_alignr_epi8(x7, x8, 8);
	__m128i xE = _mm_alignr_epi8(x7, x8, 4);
	return decode_Residual8x8(x9, xA, xB, x7, xC, xD, xE, x8);
}

static int decode_VerticalLeft8x8(__m128i right, __m128i top, __m128i topl) {
	__m128i topr = _mm_alignr_epi8(right, top, 2);
	__m128i rightl = _mm_alignr_epi8(right, top, 14);
	__m128i rightr = _mm_shufflehi_epi16(_mm_srli_si128(right, 2), _MM_SHUFFLE(2, 2, 1, 0));
	__m128i x0 = lowpass(topl, top, topr);
	__m128i x1 = lowpass(rightl, right, rightr);
	__m128i x2 = _mm_alignr_epi8(x1, x0, 2);
	__m128i x3 = _mm_alignr_epi8(x1, x0, 4);
	__m128i x4 = _mm_srli_si128(x1, 2);
	__m128i x5 = _mm_shuffle_epi32(x1, _MM_SHUFFLE(3, 3, 2, 1));
	__m128i x6 = _mm_avg_epu16(x0, x2);
	__m128i x7 = _mm_avg_epu16(x1, x4);
	__m128i x8 = lowpass(x0, x2, x3);
	__m128i x9 = lowpass(x1, x4, x5);
	__m128i xA = _mm_alignr_epi8(x7, x6, 2);
	__m128i xB = _mm_alignr_epi8(x9, x8, 2);
	__m128i xC = _mm_alignr_epi8(x7, x6, 4);
	__m128i xD = _mm_alignr_epi8(x9, x8, 4);
	__m128i xE = _mm_alignr_epi8(x7, x6, 6);
	__m128i xF = _mm_alignr_epi8(x9, x8, 6);
	return decode_Residual8x8(x6, x8, xA, xB, xC, xD, xE, xF);
}

static int decode_HorizontalUp8x8(__m128i left) {
	__m128i x0 = _mm_shufflehi_epi16(_mm_srli_si128(left, 2), _MM_SHUFFLE(2, 2, 1, 0));
	__m128i x1 = _mm_shufflehi_epi16(_mm_shuffle_epi32(left, _MM_SHUFFLE(3, 3, 2, 1)), _MM_SHUFFLE(1, 1, 1, 0));
	__m128i x2 = _mm_avg_epu16(left, x0);
	__m128i x3 = lowpass(left, x0, x1);
	__m128i x4 = _mm_unpacklo_epi16(x2, x3);
	__m128i x5 = _mm_unpackhi_epi16(x2, x3);
	__m128i x6 = _mm_alignr_epi8(x5, x4, 4);
	__m128i x7 = _mm_alignr_epi8(x5, x4, 8);
	__m128i x8 = _mm_alignr_epi8(x5, x4, 12);
	__m128i x9 = _mm_shuffle_epi32(x5, _MM_SHUFFLE(3, 3, 2, 1));
	__m128i xA = _mm_shuffle_epi32(x5, _MM_SHUFFLE(3, 3, 3, 2));
	__m128i xB = _mm_shuffle_epi32(x5, _MM_SHUFFLE(3, 3, 3, 3));
	return decode_Residual8x8(x4, x6, x7, x8, x5, x9, xA, xB);
}



/**
 * Intra16x16 and Chroma modes
 */
static int decode_Vertical16x16(__m128i topr, __m128i topl) {
	__m128i x0 = _mm_unpacklo_epi64(topl, topl);
	__m128i x1 = _mm_unpackhi_epi64(topl, topl);
	__m128i x2 = _mm_unpacklo_epi64(topr, topr);
	__m128i x3 = _mm_unpackhi_epi64(topr, topr);
	ctx->pred_buffer[0] = ctx->pred_buffer[2] = ctx->pred_buffer[8] = ctx->pred_buffer[10] = (v8hi)x0;
	ctx->pred_buffer[1] = ctx->pred_buffer[3] = ctx->pred_buffer[9] = ctx->pred_buffer[11] = (v8hi)x1;
	ctx->pred_buffer[4] = ctx->pred_buffer[6] = ctx->pred_buffer[12] = ctx->pred_buffer[14] = (v8hi)x2;
	ctx->pred_buffer[5] = ctx->pred_buffer[7] = ctx->pred_buffer[13] = ctx->pred_buffer[15] = (v8hi)x3;
	return 0;
}

static int decode_Horizontal16x16(__m128i leftt, __m128i leftb) {
	__m128i x0 = _mm_unpacklo_epi16(leftt, leftt);
	__m128i x1 = _mm_unpackhi_epi16(leftt, leftt);
	__m128i x2 = _mm_unpacklo_epi16(leftb, leftb);
	__m128i x3 = _mm_unpackhi_epi16(leftb, leftb);
	ctx->pred_buffer[0] = ctx->pred_buffer[1] = ctx->pred_buffer[4] = ctx->pred_buffer[5] = (v8hi)x0;
	ctx->pred_buffer[2] = ctx->pred_buffer[3] = ctx->pred_buffer[6] = ctx->pred_buffer[7] = (v8hi)x1;
	ctx->pred_buffer[8] = ctx->pred_buffer[9] = ctx->pred_buffer[12] = ctx->pred_buffer[13] = (v8hi)x2;
	ctx->pred_buffer[10] = ctx->pred_buffer[11] = ctx->pred_buffer[14] = ctx->pred_buffer[15] = (v8hi)x3;
	return 0;
}

static int decode_DC16x16_8bit(__m128i top, __m128i left) {
	__m128i zero = _mm_setzero_si128();
	__m128i x0 = _mm_add_epi16(_mm_sad_epu8(top, zero), _mm_sad_epu8(left, zero));
	__m128i x1 = _mm_add_epi16(x0, _mm_shuffle_epi32(x0, _MM_SHUFFLE(3, 2, 3, 2)));
	__m128i DC = _mm_broadcastw_epi16(_mm_avg_epu16(_mm_srli_epi16(x1, 4), zero));
	ctx->pred_buffer[0] = (v8hi)DC;
	return 0;
}

static int decode_DC16x16_16bit(__m128i topr, __m128i topl, __m128i leftt, __m128i leftb) {
	__m128i zero = _mm_setzero_si128();
	__m128i x0 = _mm_adds_epu16(_mm_add_epi16(topr, topl), _mm_add_epi16(leftt, leftb));
	__m128i x1 = _mm_add_epi32(_mm_unpacklo_epi16(x0, zero), _mm_unpackhi_epi16(x0, zero));
	__m128i x2 = _mm_add_epi32(x1, _mm_shuffle_epi32(x1, _MM_SHUFFLE(1, 0, 3, 2)));
	__m128i x3 = _mm_add_epi32(x2, _mm_shuffle_epi32(x2, _MM_SHUFFLE(2, 3, 0, 1)));
	__m128i x4 = _mm_srli_epi32(x3, 4);
	__m128i DC = _mm_avg_epu16(_mm_packs_epi32(x4, x4), zero);
	ctx->pred_buffer[0] = (v8hi)DC;
	return 0;
}

static int decode_Plane16x16_8bit(__m128i top, __m128i tl, __m128i left) {
	// Sum the samples and compute a, b, c (with care for overflow)
	__m128i mul = (__m128i)(v16qi){-7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8};
	__m128i zero = _mm_setzero_si128();
	__m128i x0 = _mm_maddubs_epi16(top, mul);
	__m128i x1 = _mm_maddubs_epi16(left, mul);
	__m128i x2 = _mm_sub_epi16(_mm_hadd_epi16(x0, x1), _mm_slli_epi16(tl, 3));
	__m128i x3 = _mm_add_epi16(x2, _mm_shuffle_epi32(x2, _MM_SHUFFLE(2, 3, 0, 1)));
	__m128i HV = _mm_hadd_epi16(x3, x3); // HHVVHHVV, 15 significant bits
	__m128i x4 = _mm_add_epi16(HV, _mm_srai_epi16(HV, 2)); // (5 * HV) >> 2
	__m128i x5 = _mm_avg_epu16(_mm_srai_epi16(x4, 3), zero); // (5 * HV + 32) >> 6
	__m128i x6 = _mm_add_epi16(_mm_unpackhi_epi8(top, zero), _mm_unpackhi_epi8(left, zero));
	__m128i a = _mm_srai_epi16(_mm_sub_epi16(_mm_broadcastw_epi16(x6), _mm_set1_epi16(-1)), 4);
	__m128i b = _mm_shuffle_epi32(x5, _MM_SHUFFLE(0, 0, 0, 0));
	__m128i c = _mm_shuffle_epi32(x5, _MM_SHUFFLE(1, 1, 1, 1));
	
	// compute the first row of prediction vectors
	__m128i c1 = _mm_slli_epi16(c, 1);
	__m128i c2 = _mm_slli_epi16(c, 2);
	((__m128i *)ctx->pred_buffer)[16] = c1;
	__m128i x7 = _mm_sub_epi16(_mm_sub_epi16(a, c), _mm_add_epi16(c1, c2)); // a - c * 7 + 16
	__m128i x8 = _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(mul, zero), b), x7);
	__m128i x9 = _mm_add_epi16(_mm_slli_epi16(b, 3), x8);
	__m128i xA = _mm_add_epi16(x8, c);
	__m128i xB = _mm_add_epi16(x9, c);
	__m128i p0 = _mm_unpacklo_epi64(x8, xA);
	__m128i p1 = _mm_unpackhi_epi64(x8, xA);
	__m128i p2 = _mm_unpacklo_epi64(x9, xB);
	__m128i p3 = _mm_unpackhi_epi64(x9, xB);
	
	// store them
	((__m128i *)ctx->pred_buffer)[0] = p0;
	((__m128i *)ctx->pred_buffer)[1] = p1;
	((__m128i *)ctx->pred_buffer)[4] = p2;
	((__m128i *)ctx->pred_buffer)[5] = p3;
	((__m128i *)ctx->pred_buffer)[2] = p0 = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[3] = p1 = _mm_add_epi16(p1, c2);
	((__m128i *)ctx->pred_buffer)[6] = p2 = _mm_add_epi16(p2, c2);
	((__m128i *)ctx->pred_buffer)[7] = p3 = _mm_add_epi16(p3, c2);
	((__m128i *)ctx->pred_buffer)[8] = p0 = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[9] = p1 = _mm_add_epi16(p1, c2);
	((__m128i *)ctx->pred_buffer)[12] = p2 = _mm_add_epi16(p2, c2);
	((__m128i *)ctx->pred_buffer)[13] = p3 = _mm_add_epi16(p3, c2);
	((__m128i *)ctx->pred_buffer)[10] = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[11] = _mm_add_epi16(p1, c2);
	((__m128i *)ctx->pred_buffer)[14] = _mm_add_epi16(p2, c2);
	((__m128i *)ctx->pred_buffer)[15] = _mm_add_epi16(p3, c2);
	return 0;
}



static int predict_Plane16x16_16bit(uint8_t *p, size_t stride)
{
	static const v16qi inv = {14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1};
	static const v8hi mul0 = {40, 35, 30, 25, 20, 15, 10, 5};
	static const v4si mul1 = {-7, -6, -5, -4};
	static const v4si mul2 = {-3, -2, -1, 0};
	static const v4si mul3 = {1, 2, 3, 4};
	static const v4si mul4 = {5, 6, 7, 8};
	static const v4si s32 = {32, 32, 32, 32};
	
	// load all neighbouring samples
	// TODO: Adopt p/q/r/s convention
	int p15 = ((int16_t *)p)[15];
	__m128i t0 = _mm_lddqu_si128((__m128i *)(p - 2));
	__m128i t1 = _mm_shuffle_epi8(*(__m128i *)(p + 16), (__m128i)inv);
	__m128i l0 = _mm_alignr_epi8(t0, *(__m128i *)(p += stride - 16), 14);
	__m128i l8 = _mm_srli_si128(*(__m128i *)(p + stride * 8), 14);
	__m128i l1 = _mm_alignr_epi8(l0, *(__m128i *)(p += stride), 14);
	__m128i l9 = _mm_alignr_epi8(l8, *(__m128i *)(p + stride * 8), 14);
	__m128i l2 = _mm_alignr_epi8(l1, *(__m128i *)(p += stride), 14);
	__m128i lA = _mm_alignr_epi8(l9, *(__m128i *)(p + stride * 8), 14);
	__m128i l3 = _mm_alignr_epi8(l2, *(__m128i *)(p += stride), 14);
	__m128i lB = _mm_alignr_epi8(lA, *(__m128i *)(p + stride * 8), 14);
	__m128i l4 = _mm_alignr_epi8(l3, *(__m128i *)(p += stride), 14);
	__m128i lC = _mm_alignr_epi8(lB, *(__m128i *)(p + stride * 8), 14);
	__m128i l5 = _mm_alignr_epi8(l4, *(__m128i *)(p += stride), 14);
	__m128i lD = _mm_alignr_epi8(lC, *(__m128i *)(p + stride * 8), 14);
	__m128i l6 = _mm_alignr_epi8(l5, *(__m128i *)(p += stride), 14);
	__m128i lE = _mm_alignr_epi8(lD, *(__m128i *)(p += stride * 8), 14);
	__m128i l7 = _mm_shuffle_epi8(l6, (__m128i)inv);
	__m128i lF = _mm_alignr_epi8(lE, *(__m128i *)(p += stride), 14);
	
	// sum them and compute a, b, c
	__m128i x0 = _mm_madd_epi16(_mm_sub_epi16(t1, t0), (__m128i)mul0);
	__m128i x1 = _mm_madd_epi16(_mm_sub_epi16(lF, l7), (__m128i)mul0);
	__m128i x2 = _mm_hadd_epi32(x0, x1);
	__m128i HV = _mm_add_epi32(x2, _mm_shuffle_epi32(x2, _MM_SHUFFLE(2, 3, 0, 1))); // HHVV
	__m128i x3 = _mm_srai_epi32(_mm_add_epi32(HV, (__m128i)s32), 6);
	__m128i a = _mm_set1_epi32((p15 + ((int16_t *)p)[7] + 1) * 16);
	__m128i b = _mm_shuffle_epi32(x3, _MM_SHUFFLE(1, 0, 1, 0));
	__m128i c = _mm_shuffle_epi32(x3, _MM_SHUFFLE(3, 2, 3, 2));
	
	// compute the first row of prediction vectors
	((__m128i *)ctx->pred_buffer)[16] = c;
	__m128i x4 = _mm_sub_epi32(_mm_add_epi32(a, c), _mm_slli_epi32(c, 3)); // a - c * 7 + 16
	__m128i p0 = _mm_add_epi32(_mm_mullo_epi32(b, (__m128i)mul1), x4);
	__m128i p1 = _mm_add_epi32(_mm_mullo_epi32(b, (__m128i)mul2), x4);
	__m128i p2 = _mm_add_epi32(_mm_mullo_epi32(b, (__m128i)mul3), x4);
	__m128i p3 = _mm_add_epi32(_mm_mullo_epi32(b, (__m128i)mul4), x4);
	
	// store them
	__m128i c2 = _mm_slli_epi32(c, 2);
	((__m128i *)ctx->pred_buffer)[0] = p0;
	((__m128i *)ctx->pred_buffer)[1] = p1;
	((__m128i *)ctx->pred_buffer)[4] = p2;
	((__m128i *)ctx->pred_buffer)[5] = p3;
	((__m128i *)ctx->pred_buffer)[2] = p0 = _mm_add_epi32(p0, c2);
	((__m128i *)ctx->pred_buffer)[3] = p1 = _mm_add_epi32(p1, c2);
	((__m128i *)ctx->pred_buffer)[6] = p2 = _mm_add_epi32(p2, c2);
	((__m128i *)ctx->pred_buffer)[7] = p3 = _mm_add_epi32(p3, c2);
	((__m128i *)ctx->pred_buffer)[8] = p0 = _mm_add_epi32(p0, c2);
	((__m128i *)ctx->pred_buffer)[9] = p1 = _mm_add_epi32(p1, c2);
	((__m128i *)ctx->pred_buffer)[12] = p2 = _mm_add_epi32(p2, c2);
	((__m128i *)ctx->pred_buffer)[13] = p3 = _mm_add_epi32(p3, c2);
	((__m128i *)ctx->pred_buffer)[10] = _mm_add_epi32(p0, c2);
	((__m128i *)ctx->pred_buffer)[11] = _mm_add_epi32(p1, c2);
	((__m128i *)ctx->pred_buffer)[14] = _mm_add_epi32(p2, c2);
	((__m128i *)ctx->pred_buffer)[15] = _mm_add_epi32(p3, c2);
	return 0;
}



static int predict_Plane8x16_8bit(uint8_t *p, size_t stride)
{
	static const v16qi mul0 = {-1, -2, -3, -4, -5, -6, -7, -8, -4, -3, -2, -1, 0, 0, 0, 0};
	static const v16qi mul1 = {8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 0, 0, 0, 0};
	static const v8hi mul2 = {-3, -2, -1, 0, 1, 2, 3, 4};
	
	// load all neighbouring samples
	size_t stride3 = stride * 3;
	uint8_t *q = p + stride * 4 - 8;
	uint8_t *r = p + stride * 8 - 8;
	uint8_t *s = r + stride * 4;
	__m64 l0 = _mm_alignr_pi8(*(__m64 *)(p - 1), *(__m64 *)(p + stride     - 8), 7);
	__m64 l1 = _mm_alignr_pi8(*(__m64 *)(r + stride), *(__m64 *)(r + stride * 2), 7);
	__m64 l2 = _mm_alignr_pi8(l0, *(__m64 *)(p +  stride * 2 - 8), 7);
	__m64 l3 = _mm_alignr_pi8(l1, *(__m64 *)(r + stride3), 7);
	__m64 l4 = _mm_alignr_pi8(l2, *(__m64 *)(p + stride3 - 8), 7);
	__m64 l5 = _mm_alignr_pi8(l3, *(__m64 *)s, 7);
	__m64 l6 = _mm_alignr_pi8(l4, *(__m64 *)q, 7);
	__m64 l7 = _mm_alignr_pi8(l5, *(__m64 *)(s + stride), 7);
	__m64 l8 = _mm_alignr_pi8(l6, *(__m64 *)(q + stride), 7);
	__m64 l9 = _mm_alignr_pi8(l7, *(__m64 *)(s + stride * 2), 7);
	__m64 lA = _mm_alignr_pi8(l8, *(__m64 *)(q + stride * 2), 7);
	__m64 lB = _mm_alignr_pi8(l9, *(__m64 *)(s + stride3), 7);
	__m64 lC = _mm_alignr_pi8(lA, *(__m64 *)(q + stride3), 7);
	__m64 lD = _mm_alignr_pi8(lB, *(__m64 *)(s + stride * 4), 7);
	__m128i lt0 = _mm_set_epi64(*(__m64 *)(p - 1), lC);
	__m128i lt1 = _mm_set_epi64(*(__m64 *)(p + 4), lD);
	
	// sum them and compute a, b, c (with care for overflow)
	__m128i x0 = _mm_maddubs_epi16(lt0, (__m128i)mul0);
	__m128i x1 = _mm_maddubs_epi16(lt1, (__m128i)mul1);
	__m128i x2 = _mm_add_epi16(x0, x1);
	__m128i x3 = _mm_add_epi16(x2, _mm_shuffle_epi32(x2, _MM_SHUFFLE(2, 3, 0, 1)));
	__m128i HV = _mm_hadd_epi16(x3, x3); // VVHHVVHH
	__m128i H = _mm_shuffle_epi32(HV, _MM_SHUFFLE(1, 1, 1, 1));
	__m128i V = _mm_shuffle_epi32(HV, _MM_SHUFFLE(0, 0, 0, 0));
	__m128i x4 = _mm_add_epi16(H, _mm_srai_epi16(HV, 4)); // (17 * H) >> 4
	__m128i x5 = _mm_add_epi16(V, _mm_srai_epi16(HV, 2)); // (5 * V) >> 2
	__m128i a = _mm_set1_epi16((p[15] + s[stride * 4 + 7] + 1) * 16); // FIXME
	__m128i b = _mm_srai_epi16(_mm_sub_epi16(x4, _mm_set1_epi16(-1)), 2);
	__m128i c = _mm_srai_epi16(_mm_add_epi16(x5, _mm_set1_epi16(8)), 4);
	
	// compute the first row of prediction vectors
	__m128i c1 = _mm_slli_epi16(c, 1);
	__m128i c2 = _mm_slli_epi16(c, 2);
	((__m128i *)ctx->pred_buffer)[16] = c1;
	__m128i x6 = _mm_sub_epi16(_mm_sub_epi16(a, c), _mm_add_epi16(c1, c2)); // a - c * 7 + 16
	__m128i x7 = _mm_add_epi16(_mm_mullo_epi16(b, (__m128i)mul2), x6);
	__m128i x8 = _mm_add_epi16(x7, c);
	__m128i p0 = _mm_unpacklo_epi64(x7, x8);
	__m128i p1 = _mm_unpackhi_epi64(x7, x8);
	
	// store them
	((__m128i *)ctx->pred_buffer)[0] = p0;
	((__m128i *)ctx->pred_buffer)[1] = p1;
	((__m128i *)ctx->pred_buffer)[2] = p0 = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[3] = p1 = _mm_add_epi16(p1, c2);
	((__m128i *)ctx->pred_buffer)[4] = p0 = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[5] = p1 = _mm_add_epi16(p1, c2);
	((__m128i *)ctx->pred_buffer)[6] = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[7] = _mm_add_epi16(p1, c2);
	return 0;
}



static int predict_Plane8x16_16bit(uint8_t *p, size_t stride)
{
	static const v16qi shuf = {0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1};
	static const v16qi inv = {14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1};
	static const v8hi mul0 = {-136, -102, -68, -34, 34, 68, 102, 136};
	static const v8hi mul1 = {40, 35, 30, 25, 20, 15, 10, 5};
	static const v4si mul2 = {-3, -2, -1, 0};
	static const v4si mul3 = {1, 2, 3, 4};
	static const v4si s32 = {32, 32, 32, 32};
	
	// load all neighbouring samples
	int p7 = ((int16_t *)p)[7];
	__m128i t0 = _mm_shuffle_epi8(*(__m128i *)p, (__m128i)shuf);
	__m128i t1 = _mm_alignr_epi8(t0, *(__m128i *)(p -= 16), 14);
	__m128i l0 = _mm_alignr_epi8(t1, *(__m128i *)(p += stride), 14);
	__m128i l8 = _mm_srli_si128(*(__m128i *)(p + stride * 8), 14);
	__m128i l1 = _mm_alignr_epi8(l0, *(__m128i *)(p += stride), 14);
	__m128i l9 = _mm_alignr_epi8(l8, *(__m128i *)(p + stride * 8), 14);
	__m128i l2 = _mm_alignr_epi8(l1, *(__m128i *)(p += stride), 14);
	__m128i lA = _mm_alignr_epi8(l9, *(__m128i *)(p + stride * 8), 14);
	__m128i l3 = _mm_alignr_epi8(l2, *(__m128i *)(p += stride), 14);
	__m128i lB = _mm_alignr_epi8(lA, *(__m128i *)(p + stride * 8), 14);
	__m128i l4 = _mm_alignr_epi8(l3, *(__m128i *)(p += stride), 14);
	__m128i lC = _mm_alignr_epi8(lB, *(__m128i *)(p + stride * 8), 14);
	__m128i l5 = _mm_alignr_epi8(l4, *(__m128i *)(p += stride), 14);
	__m128i lD = _mm_alignr_epi8(lC, *(__m128i *)(p + stride * 8), 14);
	__m128i l6 = _mm_alignr_epi8(l5, *(__m128i *)(p += stride), 14);
	__m128i lE = _mm_alignr_epi8(lD, *(__m128i *)(p += stride * 8), 14);
	__m128i l7 = _mm_shuffle_epi8(l6, (__m128i)inv);
	__m128i lF = _mm_alignr_epi8(lE, *(__m128i *)(p += stride), 14);
	
	// sum them and compute a, b, c
	__m128i x0 = _mm_madd_epi16(t1, (__m128i)mul0);
	__m128i x1 = _mm_madd_epi16(_mm_sub_epi16(lF, l7), (__m128i)mul1);
	__m128i x2 = _mm_add_epi32(x0, _mm_shuffle_epi32(x0, _MM_SHUFFLE(1, 0, 3, 2)));
	__m128i x3 = _mm_add_epi32(x1, _mm_shuffle_epi32(x1, _MM_SHUFFLE(1, 0, 3, 2)));
	__m128i H = _mm_add_epi32(x2, _mm_shuffle_epi32(x2, _MM_SHUFFLE(2, 3, 0, 1)));
	__m128i V = _mm_add_epi32(x3, _mm_shuffle_epi32(x3, _MM_SHUFFLE(2, 3, 0, 1)));
	__m128i a = _mm_set1_epi32((p7 + ((int16_t *)p)[7] + 1) * 16);
	__m128i b = _mm_srai_epi32(_mm_add_epi32(H, (__m128i)s32), 6);
	__m128i c = _mm_srai_epi32(_mm_add_epi32(V, (__m128i)s32), 6);
	
	// compute the first row of prediction vectors
	((__m128i *)ctx->pred_buffer)[16] = c;
	__m128i x4 = _mm_sub_epi32(_mm_add_epi32(a, c), _mm_slli_epi32(c, 3)); // a - c * 7 + 16
	__m128i p0 = _mm_add_epi32(_mm_mullo_epi32(b, (__m128i)mul2), x4);
	__m128i p1 = _mm_add_epi32(_mm_mullo_epi32(b, (__m128i)mul3), x4);
	
	// store them
	__m128i c2 = _mm_slli_epi32(c, 2);
	((__m128i *)ctx->pred_buffer)[0] = p0;
	((__m128i *)ctx->pred_buffer)[1] = p1;
	((__m128i *)ctx->pred_buffer)[2] = p0 = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[3] = p1 = _mm_add_epi16(p1, c2);
	((__m128i *)ctx->pred_buffer)[4] = p0 = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[5] = p1 = _mm_add_epi16(p1, c2);
	((__m128i *)ctx->pred_buffer)[6] = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[7] = _mm_add_epi16(p1, c2);
	return 0;
}



static int predict_Plane8x8_8bit(uint8_t *p, size_t stride)
{
	static const v16qi mul0 = {-1, -2, -3, -4, -4, -3, -2, -1, 1, 2, 3, 4, 4, 3, 2, 1};
	static const v8hi mul1 = {-3, -2, -1, 0, 1, 2, 3, 4};
	
	// load all neighbouring samples
	size_t stride3 = stride * 3;
	uint8_t *q = (p -= 8) + stride * 4;
	__m64 t0 = *(__m64 *)(p + 7);
	__m64 t1 = *(__m64 *)(p + 12);
	__m64 l0 = _mm_alignr_pi8(t0, *(__m64 *)p, 7);
	__m64 l1 = _mm_alignr_pi8(t1, *(__m64 *)(q + stride), 7);
	__m64 l2 = _mm_alignr_pi8(l0, *(__m64 *)(p + stride), 7);
	__m64 l3 = _mm_alignr_pi8(l1, *(__m64 *)(q + stride * 2), 7);
	__m64 l4 = _mm_alignr_pi8(l2, *(__m64 *)(p + stride * 2), 7);
	__m64 l5 = _mm_alignr_pi8(l3, *(__m64 *)(q + stride3), 7);
	__m64 l6 = _mm_alignr_pi8(l4, *(__m64 *)(p + stride3), 7);
	__m64 l7 = _mm_alignr_pi8(l5, *(__m64 *)(q + stride * 4), 7);
	
	// sum them and compute a, b, c (with care for overflow)
	__m128i x0 = _mm_maddubs_epi16(_mm_set_epi64(l6, l7), (__m128i)mul0);
	__m128i x1 = _mm_add_epi16(x0, _mm_shuffle_epi32(x0, _MM_SHUFFLE(1, 0, 3, 2)));
	__m128i HV = _mm_add_epi16(x1, _mm_shufflelo_epi16(x1, _MM_SHUFFLE(2, 3, 0, 1)));
	__m128i x2 = _mm_add_epi16(HV, _mm_srai_epi16(HV, 4)); // (17 * HV) >> 4
	__m128i x3 = _mm_srai_epi16(_mm_sub_epi16(x2, _mm_set1_epi16(-1)), 1); // (17 * HV + 16) >> 5
	__m128i a = _mm_set1_epi16((p[15] + q[stride * 4 + 7] + 1) * 16);
	__m128i b = _mm_shuffle_epi32(x3, _MM_SHUFFLE(1, 1, 1, 1));
	__m128i c = _mm_shuffle_epi32(x3, _MM_SHUFFLE(0, 0, 0, 0));
	
	// compute the first row of prediction vectors
	__m128i c1 = _mm_slli_epi16(c, 1);
	((__m128i *)ctx->pred_buffer)[16] = c1;
	__m128i x4 = _mm_sub_epi16(_mm_sub_epi16(a, c), c1); // a - c * 3 + 16
	__m128i x5 = _mm_add_epi16(_mm_mullo_epi16(b, (__m128i)mul1), x4);
	__m128i x6 = _mm_add_epi16(x5, c);
	__m128i p0 = _mm_unpacklo_epi64(x5, x6);
	__m128i p1 = _mm_unpackhi_epi64(x5, x6);
	
	// store them
	__m128i c2 = _mm_slli_epi16(c, 2);
	((__m128i *)ctx->pred_buffer)[0] = p0;
	((__m128i *)ctx->pred_buffer)[1] = p1;
	((__m128i *)ctx->pred_buffer)[2] = _mm_add_epi16(p0, c2);
	((__m128i *)ctx->pred_buffer)[3] = _mm_add_epi16(p1, c2);
	return 0;
}



static int predict_Plane8x8_16bit(uint8_t *p, size_t stride)
{
	static const v16qi shuf = {6, 7, 4, 5, 2, 3, 0, 1, 12, 13, 10, 11, 8, 9, 6, 7};
	static const v8hi mul0 = {68, 51, 34, 17, 17, 34, 51, 68};
	static const v4si mul1 = {-3, -2, -1, 0};
	
	// load all neighbouring samples
	size_t stride3 = stride * 3;
	uint8_t *q = p + stride * 4 - 16;
	__m128i t0 = _mm_movpi64_epi64(*(__m64 *)(p - 1));
	__m128i t1 = _mm_movpi64_epi64(*(__m64 *)(p + 8));
	__m128i l0 = _mm_alignr_epi8(t1, *(__m128i *)(q + stride), 14);
	__m128i l1 = _mm_alignr_epi8(t0, *(__m128i *)(p + stride - 16), 14);
	__m128i l2 = _mm_alignr_epi8(l0, *(__m128i *)(q + stride * 2), 14);
	__m128i l3 = _mm_alignr_epi8(l1, *(__m128i *)(p +  stride * 2 - 16), 14);
	__m128i l4 = _mm_alignr_epi8(l2, *(__m128i *)(q + stride3), 14);
	__m128i l5 = _mm_alignr_epi8(l3, *(__m128i *)(p + stride3 - 16), 14);
	__m128i l6 = _mm_alignr_epi8(l4, *(__m128i *)(q + stride * 4), 14);
	
	// sum them and compute a, b, c
	__m128i x0 = _mm_sub_epi16(l6, _mm_shuffle_epi8(l5, (__m128i)shuf));
	__m128i x1 = _mm_madd_epi16(x0, (__m128i)mul0);
	__m128i HV = _mm_add_epi32(x1, _mm_shuffle_epi32(x1, _MM_SHUFFLE(2, 3, 0, 1))); // VVHH
	__m128i x2 = _mm_srai_epi32(_mm_add_epi32(HV, _mm_set1_epi32(16)), 5);
	__m128i a = _mm_set1_epi32((((int16_t *)p)[7] + ((int16_t *)q)[7] + 1) * 16);
	__m128i b = _mm_unpackhi_epi64(x2, x2);
	__m128i c = _mm_unpacklo_epi64(x2, x2);
	
	// compute the first row of prediction vectors
	((__m128i *)ctx->pred_buffer)[16] = c;
	__m128i c2 = _mm_slli_epi32(c, 2);
	__m128i x3 = _mm_sub_epi32(_mm_add_epi32(a, c), c2); // a - c * 3 + 16
	__m128i p0 = _mm_add_epi32(_mm_mullo_epi32(b, (__m128i)mul1), x3);
	__m128i p1 = _mm_add_epi32(_mm_slli_epi32(b, 2), p0);
	
	// store them
	((__m128i *)ctx->pred_buffer)[0] = p0;
	((__m128i *)ctx->pred_buffer)[1] = p1;
	((__m128i *)ctx->pred_buffer)[2] = _mm_add_epi32(p0, c2);
	((__m128i *)ctx->pred_buffer)[3] = _mm_add_epi32(p1, c2);
	return 0;
}



/**
 * This function has been redesigned many times because of many constraints.
 * The ideal architecture here is a tree where a switch starts to the leaves,
 * and we jump down the tree to share code among subsections, finishing with
 * residual decoding at the root.
 *
 * The problems are:
 * _ The prologue must be minimal for the performance of Intra4x4 modes, so
 *   code common to other modes has to be duplicated or put in functions.
 * _ clang does not support collapsing nested switches into a single one,
 *   leaving only the possibility to implement the tree with functions.
 * _ Functions incur some overhead, even with tail calls, because of stack
 *   management and the impossibility to do near/short jumps.
 * _ Readability prevents the main function from growing too large.
 */
int decode_switch(size_t stride, ssize_t nstride, uint8_t *p, __m128i zero) {
	static const v16qi C8_8bit = {7, 8, 9, 10, 11, 12, 13, 14, 15, 15, -1, -1, -1, -1, -1, -1};
	static const v16qi D8_8bit = {0, 0, 1, 2, 3, 4, 5, 6, 7, 8, -1, -1, -1, -1, -1, -1};
	static const v16qi CD8_8bit = {0, 0, 1, 2, 3, 4, 5, 6, 7, 7, -1, -1, -1, -1, -1, -1};
	__m64 m0, m1, m2;
	__m128i x0, x1, x2, x3, x4, x5, x6;
	switch (ctx->PredMode[ctx->BlkIdx]) {
	
	// Intra4x4 modes -> critical performance
	case VERTICAL_4x4:
		x0 = _mm_unpacklo_epi8(_mm_set1_epi32(*(int32_t *)(p + nstride * 2)), zero);
		return decode_Residual4x4(x0, x0);
	case HORIZONTAL_4x4:
		return decode_Horizontal4x4_8bit(stride, nstride, p);
	case DC_4x4:
		m0 = _mm_unpacklo_pi8(*(__m64 *)(p + nstride     - 4), *(__m64 *)(p               - 4));
		m1 = _mm_unpacklo_pi8(*(__m64 *)(p +  stride     - 4), *(__m64 *)(p +  stride * 2 - 4));
		m2 = _mm_unpackhi_pi32(_mm_unpackhi_pi16(m0, m1), *(__m64 *)(p + nstride * 2 - 4));
		return decode_DC4_8bit_8bit(zero, _mm_movpi64_epi64(m2));
	case DC_4x4_A:
		return decode_DC4_8bit_8bit(zero, _mm_set1_epi32(*(int32_t *)(p + nstride * 2)));
	case DC_4x4_B:
		m0 = _mm_unpacklo_pi8(*(__m64 *)(p + nstride     - 4), *(__m64 *)(p               - 4));
		m1 = _mm_unpacklo_pi8(*(__m64 *)(p +  stride     - 4), *(__m64 *)(p +  stride * 2 - 4));
		m2 = _mm_shuffle_pi16(_mm_unpackhi_pi16(m0, m1), _MM_SHUFFLE(3, 2, 3, 2));
		return decode_DC4_8bit_8bit(zero, _mm_movpi64_epi64(m2));
	case DC_4x4_AB:
	case DC_4x4_AB_16_BIT:
		x0 = _mm_avg_epu16((__m128i)ctx->clip, zero);
		return decode_Residual4x4(x0, x0);
	case DIAGONAL_DOWN_LEFT_4x4:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		return decode_DiagonalDownLeft4x4(x0);
	case DIAGONAL_DOWN_LEFT_4x4_C:
		x0 = _mm_unpacklo_epi8(_mm_set1_epi32(*(int32_t *)(p + nstride * 2)), zero);
		return decode_DiagonalDownLeft4x4(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)));
	case DIAGONAL_DOWN_RIGHT_4x4:
		m0 = _mm_alignr_pi8(*(__m64 *)(p + nstride * 2 - 1), *(__m64 *)(p + nstride     - 8), 7);
		m1 = _mm_unpackhi_pi8(*(__m64 *)(p +  stride     - 8), *(__m64 *)(p               - 8));
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(_mm_alignr_pi8(m0, m1, 6)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p +  stride * 2 - 8)), zero);
		return decode_DiagonalDownRight4x4(x0, x1);
	case VERTICAL_RIGHT_4x4:
		m0 = _mm_alignr_pi8(*(__m64 *)(p + nstride * 2 - 1), *(__m64 *)(p + nstride     - 8), 7);
		m1 = _mm_unpackhi_pi8(*(__m64 *)(p +  stride     - 8), *(__m64 *)(p               - 8));
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(_mm_alignr_pi8(m0, m1, 6)), zero);
		return decode_VerticalRight4x4(x0);
	case HORIZONTAL_DOWN_4x4:
		m0 = _mm_unpacklo_pi8(*(__m64 *)(p +  stride * 2 - 4), *(__m64 *)(p +  stride     - 4));
		m1 = _mm_unpacklo_pi8(*(__m64 *)(p               - 4), *(__m64 *)(p + nstride     - 4));
		m2 = _mm_unpackhi_pi32(_mm_unpackhi_pi16(m0, m1), *(__m64 *)(p + nstride * 2 - 5));
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(m2), zero);
		return decode_HorizontalDown4x4(x0);
	case VERTICAL_LEFT_4x4:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		return decode_VerticalLeft4x4(x0);
	case VERTICAL_LEFT_4x4_C:
		x0 = _mm_unpacklo_epi8(_mm_set1_epi32(*(int32_t *)(p + nstride * 2)), zero);
		return decode_VerticalLeft4x4(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)));
	case HORIZONTAL_UP_4x4:
		return decode_HorizontalUp4x4_8bit(stride, nstride, p, zero);
	
	
	// Intra8x8 modes
	case VERTICAL_8x8:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 1)), zero);
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		return decode_Vertical8x8(x1, x0, x2);
	case VERTICAL_8x8_C:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		return decode_Vertical8x8(x1, x0, x2);
	case VERTICAL_8x8_D:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 1)), zero);
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_Vertical8x8(x1, x0, x2);
	case VERTICAL_8x8_CD:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_Vertical8x8(x1, x0, x2);
	case HORIZONTAL_8x8:
		return decode_Horizontal8x8(filter8_left_8bit(stride, nstride, p, zero, nstride * 2));
	case HORIZONTAL_8x8_D:
		return decode_Horizontal8x8(filter8_left_8bit(stride, nstride, p, zero, nstride));
	case DC_8x8:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 1));
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride * 2, x0);
		return decode_DC8x8_8bit(x1, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_C:
		x0 = _mm_shuffle_epi8(_mm_lddqu_si128((__m128i *)(p + nstride * 2 - 8)), (__m128i)C8_8bit);
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride * 2, x0);
		return decode_DC8x8_8bit(x1, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_D:
		x0 = _mm_shuffle_epi8(_mm_lddqu_si128((__m128i *)(p + nstride * 2)), (__m128i)D8_8bit);
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride, x0);
		return decode_DC8x8_8bit(x1, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_CD:
		x0 = _mm_shuffle_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), (__m128i)CD8_8bit);
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride, x0);
		return decode_DC8x8_8bit(x1, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_A:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 1)), zero);
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_8bit(x3, x3);
	case DC_8x8_AC:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_8bit(x3, x3);
	case DC_8x8_AD:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 1)), zero);
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_8bit(x3, x3);
	case DC_8x8_ACD:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_8bit(x3, x3);
	case DC_8x8_B:
		x0 = filter8_left_8bit(stride, nstride, p, zero, nstride * 2);
		return decode_DC8x8_8bit(x0, x0);
	case DC_8x8_BD:
		x0 = filter8_left_8bit(stride, nstride, p, zero, nstride);
		return decode_DC8x8_8bit(x0, x0);
	case DC_8x8_AB:
	case DC_8x8_AB_16_BIT:
		x0 = _mm_avg_epu16(zero, (__m128i)ctx->clip);
		return decode_Residual8x8(x0, x0, x0, x0, x0, x0, x0, x0);
	case DIAGONAL_DOWN_LEFT_8x8:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 8)), zero);
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_LEFT_8x8_C:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_LEFT_8x8_D:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 8)), zero);
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_LEFT_8x8_CD:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_RIGHT_8x8:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 1));
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride * 2, x0);
		return decode_DiagonalDownRight8x8(x1);
	case DIAGONAL_DOWN_RIGHT_8x8_C:
		x0 = _mm_shuffle_epi8(_mm_lddqu_si128((__m128i *)(p + nstride * 2 - 8)), (__m128i)C8_8bit);
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride * 2, x0);
		return decode_DiagonalDownRight8x8(x1);
	case VERTICAL_RIGHT_8x8:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 1));
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride * 2, x0);
		return decode_VerticalRight8x8(x1);
	case VERTICAL_RIGHT_8x8_C:
		x0 = _mm_shuffle_epi8(_mm_lddqu_si128((__m128i *)(p + nstride * 2 - 8)), (__m128i)C8_8bit);
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride * 2, x0);
		return decode_VerticalRight8x8(x1);
	case HORIZONTAL_DOWN_8x8:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 1));
		x1 = filter8_top_left_8bit(stride, nstride, p, zero, nstride * 2, x0);
		return decode_HorizontalDown8x8(x1);
	case VERTICAL_LEFT_8x8:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 8)), zero);
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		return decode_VerticalLeft8x8(x1, x0, x2);
	case VERTICAL_LEFT_8x8_C:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 - 1)), zero);
		return decode_VerticalLeft8x8(x1, x0, x2);
	case VERTICAL_LEFT_8x8_D:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2 + 8)), zero);
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_VerticalLeft8x8(x1, x0, x2);
	case VERTICAL_LEFT_8x8_CD:
		x0 = _mm_unpacklo_epi8(_mm_movpi64_epi64(*(__m64 *)(p + nstride * 2)), zero);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_VerticalLeft8x8(x1, x0, x2);
	case HORIZONTAL_UP_8x8:
		return decode_HorizontalUp8x8(filter8_left_8bit(stride, nstride, p, zero, nstride * 2));
	case HORIZONTAL_UP_8x8_D:
		return decode_HorizontalUp8x8(filter8_left_8bit(stride, nstride, p, zero, nstride));
	
	
	// Intra16x16 and Chroma modes
	case VERTICAL_16x16:
		x0 = _mm_unpackhi_epi8(*(__m128i *)(p + nstride * 2), zero);
		x1 = _mm_unpacklo_epi8(*(__m128i *)(p + nstride * 2), zero);
		return decode_Vertical16x16(x0, x1);
	case HORIZONTAL_16x16:
		x0 = load16_left_8bit(stride, nstride, p);
		return decode_Horizontal16x16(_mm_unpacklo_epi8(x0, zero), _mm_unpackhi_epi8(x0, zero));
	case DC_16x16:
		x0 = *(__m128i *)(p + nstride * 2);
		return decode_DC16x16_8bit(x0, load16_left_8bit(stride, nstride, p));
	case DC_16x16_A:
		x0 = *(__m128i *)(p + nstride * 2);
		return decode_DC16x16_8bit(x0, x0);
	case DC_16x16_B:
		x0 = load16_left_8bit(stride, nstride, p);
		return decode_DC16x16_8bit(x0, x0);
	case PLANE_16x16:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_set1_epi16(p[nstride * 2 - 1]);
		return decode_Plane16x16_8bit(x0, x1, load16_left_8bit(stride, nstride, p));
	case VERTICAL_4x4_BUFFERED:
	case HORIZONTAL_4x4_BUFFERED:
		x0 = (__m128i)ctx->pred_buffer[ctx->BlkIdx];
		return decode_Residual4x4(x0, x0);
	case DC_4x4_BUFFERED:
		x0 = (__m128i)ctx->pred_buffer[0];
		return decode_Residual4x4(x0, x0);
	case PLANE_4x4_BUFFERED:
		x0 = (__m128i)ctx->pred_buffer[ctx->BlkIdx];
		x1 = _mm_add_epi16(x0, (__m128i)ctx->pred_buffer[16]);
		x2 = _mm_packus_epi16(_mm_srai_epi16(x0, 5), _mm_srai_epi16(x1, 5));
		x3 = _mm_unpacklo_epi8(x2, zero);
		x4 = _mm_unpackhi_epi8(x2, zero);
		return decode_Residual4x4(x3, x4);
	
	
	// 16bit Intra4x4 modes
	case VERTICAL_4x4_16_BIT:
		x0 = _mm_set1_epi64(*(__m64 *)(p + nstride * 2));
		return decode_Residual4x4(x0, x0);
	case HORIZONTAL_4x4_16_BIT:
		return decode_Horizontal4x4_16bit(stride, nstride, p);
	case DC_4x4_16_BIT:
		x0 = _mm_set1_epi16((*(int16_t *)(p + nstride * 2) + *(int16_t *)(p + nstride * 2 + 2) +
			*(int16_t *)(p + nstride * 2 + 4) + *(int16_t *)(p + nstride * 2 + 6) +
			*(int16_t *)(p + nstride     - 2) + *(int16_t *)(p               - 2) +
			*(int16_t *)(p +  stride     - 2) + *(int16_t *)(p +  stride * 2 - 2) + 4) >> 3);
		return decode_Residual4x4(x0, x0);
	case DC_4x4_A_16_BIT:
		x0 = _mm_set1_epi16((*(int16_t *)(p + nstride * 2) + *(int16_t *)(p + nstride * 2 + 2) +
			*(int16_t *)(p + nstride * 2 + 4) + *(int16_t *)(p + nstride * 2 + 6) + 2) >> 2);
		return decode_Residual4x4(x0, x0);
	case DC_4x4_B_16_BIT:
		x0 = _mm_set1_epi16((*(int16_t *)(p + nstride     - 2) + *(int16_t *)(p               - 2) +
			*(int16_t *)(p +  stride     - 2) + *(int16_t *)(p +  stride * 2 - 2) + 2) >> 2);
		return decode_Residual4x4(x0, x0);
	case DIAGONAL_DOWN_LEFT_4x4_16_BIT:
		return decode_DiagonalDownLeft4x4(_mm_lddqu_si128((__m128i *)(p + nstride * 2)));
	case DIAGONAL_DOWN_LEFT_4x4_C_16_BIT:
		x0 = _mm_shufflehi_epi16(_mm_set1_epi64(*(__m64 *)(p + nstride * 2)), _MM_SHUFFLE(3, 3, 3, 3));
		return decode_DiagonalDownLeft4x4(x0);
	case DIAGONAL_DOWN_RIGHT_4x4_16_BIT:
		m0 = _mm_unpackhi_pi16(*(__m64 *)(p +  stride     - 8), *(__m64 *)(p               - 8));
		m1 = _mm_unpackhi_pi16(*(__m64 *)(p + nstride     - 8), *(__m64 *)(p + nstride * 2 - 8));
		x0 = _mm_set_epi64(*(__m64 *)(p + nstride * 2), _mm_unpackhi_pi32(m0, m1));
		x1 = _mm_set1_epi64(*(__m64 *)(p +  stride * 2 - 8));
		return decode_DiagonalDownRight4x4(x0, x1);
	case VERTICAL_RIGHT_4x4_16_BIT:
		m0 = _mm_unpackhi_pi16(*(__m64 *)(p +  stride     - 8), *(__m64 *)(p               - 8));
		m1 = _mm_unpackhi_pi16(*(__m64 *)(p + nstride     - 8), *(__m64 *)(p + nstride * 2 - 8));
		x0 = _mm_set_epi64(*(__m64 *)(p + nstride * 2), _mm_unpackhi_pi32(m0, m1));
		return decode_VerticalRight4x4(x0);
	case HORIZONTAL_DOWN_4x4_16_BIT:
		m0 = _mm_unpackhi_pi16(*(__m64 *)(p +  stride * 2 - 8), *(__m64 *)(p +  stride     - 8));
		m1 = _mm_unpackhi_pi16(*(__m64 *)(p +             - 8), *(__m64 *)(p + nstride     - 8));
		x0 = _mm_set_epi64(*(__m64 *)(p + nstride * 2 - 2), _mm_unpackhi_pi32(m0, m1));
		return decode_HorizontalDown4x4(x0);
	case VERTICAL_LEFT_4x4_16_BIT:
		return decode_VerticalLeft4x4(_mm_lddqu_si128((__m128i *)(p + nstride * 2)));
	case VERTICAL_LEFT_4x4_C_16_BIT:
		x0 = _mm_shufflehi_epi16(_mm_set1_epi64(*(__m64 *)(p + nstride * 2)), _MM_SHUFFLE(3, 3, 3, 3));
		return decode_VerticalLeft4x4(x0);
	case HORIZONTAL_UP_4x4_16_BIT:
		return decode_HorizontalUp4x4_16bit(stride, nstride, p);
	
	
	// 16bit Intra8x8 modes
	case VERTICAL_8x8_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		return decode_Vertical8x8(x1, x0, x2);
	case VERTICAL_8x8_C_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		return decode_Vertical8x8(x1, x0, x2);
	case VERTICAL_8x8_D_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_Vertical8x8(x1, x0, x2);
	case VERTICAL_8x8_CD_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_Vertical8x8(x1, x0, x2);
	case HORIZONTAL_8x8_16_BIT:
		return decode_Horizontal8x8(filter8_left_16bit(stride, nstride, p, nstride * 2));
	case HORIZONTAL_8x8_D_16_BIT:
		return decode_Horizontal8x8(filter8_left_16bit(stride, nstride, p, nstride));
	case DC_8x8_16_BIT:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride * 2, x0, x1);
		return decode_DC8x8_16bit(x2, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_C_16_BIT:
		x0 = _mm_shufflehi_epi16(_mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2)), _MM_SHUFFLE(2, 2, 1, 0));
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride * 2, x0, x1);
		return decode_DC8x8_16bit(x2, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_D_16_BIT:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x1 = _mm_shufflelo_epi16(_mm_slli_si128(*(__m128i *)(p + nstride * 2), 2), _MM_SHUFFLE(3, 2, 1, 1));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride, x0, x1);
		return decode_DC8x8_16bit(x2, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_CD_16_BIT:
		x0 = _mm_shufflehi_epi16(_mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2)), _MM_SHUFFLE(2, 2, 1, 0));
		x1 = _mm_shufflelo_epi16(_mm_slli_si128(*(__m128i *)(p + nstride * 2), 2), _MM_SHUFFLE(3, 2, 1, 1));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride, x0, x1);
		return decode_DC8x8_16bit(x2, (__m128i)ctx->pred_buffer[0]);
	case DC_8x8_A_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_16bit(x3, x3);
	case DC_8x8_AC_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_16bit(x3, x3);
	case DC_8x8_AD_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_16bit(x3, x3);
	case DC_8x8_ACD_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shufflehi_epi16(_mm_srli_si128(x0, 2), _MM_SHUFFLE(2, 2, 1, 0));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		x3 = lowpass(x1, x0, x2);
		return decode_DC8x8_16bit(x3, x3);
	case DC_8x8_B_16_BIT:
		x0 = filter8_left_16bit(stride, nstride, p, nstride * 2);
		return decode_DC8x8_16bit(x0, x0);
	case DC_8x8_BD_16_BIT:
		x0 = filter8_left_16bit(stride, nstride, p, nstride);
		return decode_DC8x8_16bit(x0, x0);
	case DIAGONAL_DOWN_LEFT_8x8_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = *(__m128i *)(p + nstride * 2 + 16);
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_LEFT_8x8_C_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_LEFT_8x8_D_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = *(__m128i *)(p + nstride * 2 + 16);
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_LEFT_8x8_CD_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_DiagonalDownLeft8x8(x1, x0, x2);
	case DIAGONAL_DOWN_RIGHT_8x8_16_BIT:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride * 2, x0, x1);
		return decode_DiagonalDownRight8x8(x2);
	case DIAGONAL_DOWN_RIGHT_8x8_C_16_BIT:
		x0 = _mm_shufflehi_epi16(_mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2)), _MM_SHUFFLE(2, 2, 1, 0));
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride * 2, x0, x1);
		return decode_DiagonalDownRight8x8(x2);
	case VERTICAL_RIGHT_8x8_16_BIT:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride * 2, x0, x1);
		return decode_VerticalRight8x8(x2);
	case VERTICAL_RIGHT_8x8_C_16_BIT:
		x0 = _mm_shufflehi_epi16(_mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2)), _MM_SHUFFLE(2, 2, 1, 0));
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride * 2, x0, x1);
		return decode_VerticalRight8x8(x2);
	case HORIZONTAL_DOWN_8x8_16_BIT:
		x0 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 + 2));
		x1 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		x2 = filter8_top_left_16bit(stride, nstride, p, zero, nstride * 2, x0, x1);
		return decode_HorizontalDown8x8(x2);
	case VERTICAL_LEFT_8x8_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = *(__m128i *)(p + nstride * 2 + 16);
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		return decode_VerticalLeft8x8(x1, x0, x2);
	case VERTICAL_LEFT_8x8_C_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_lddqu_si128((__m128i *)(p + nstride * 2 - 2));
		return decode_VerticalLeft8x8(x1, x0, x2);
	case VERTICAL_LEFT_8x8_D_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = *(__m128i *)(p + nstride * 2 + 16);
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_VerticalLeft8x8(x1, x0, x2);
	case VERTICAL_LEFT_8x8_CD_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2);
		x1 = _mm_shuffle_epi32(_mm_shufflehi_epi16(x0, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		x2 = _mm_shufflelo_epi16(_mm_slli_si128(x0, 2), _MM_SHUFFLE(3, 2, 1, 1));
		return decode_VerticalLeft8x8(x1, x0, x2);
	case HORIZONTAL_UP_8x8_16_BIT:
		return decode_HorizontalUp8x8(filter8_left_16bit(stride, nstride, p, nstride * 2));
	case HORIZONTAL_UP_8x8_D_16_BIT:
		return decode_HorizontalUp8x8(filter8_left_16bit(stride, nstride, p, nstride));
	
	
	// 16bit Intra16x16 and Chroma modes
	case VERTICAL_16x16_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2 + 16);
		return decode_Vertical16x16(x0, *(__m128i *)(p + nstride * 2));
	case HORIZONTAL_16x16_16_BIT:
		x0 = load16_left_16bit(stride, nstride, p);
		return decode_Horizontal16x16(x0, (__m128i)ctx->pred_buffer[0]);
	case DC_16x16_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2 + 16);
		x1 = *(__m128i *)(p + nstride * 2);
		x2 = load16_left_16bit(stride, nstride, p);
		return decode_DC16x16_16bit(x0, x1, x2, (__m128i)ctx->pred_buffer[0]);
	case DC_16x16_A_16_BIT:
		x0 = *(__m128i *)(p + nstride * 2 + 16);
		x1 = *(__m128i *)(p + nstride * 2);
		return decode_DC16x16_16bit(x0, x1, x0, x1);
	case DC_16x16_B_16_BIT:
		x0 = load16_left_16bit(stride, nstride, p);
		x1 = (__m128i)ctx->pred_buffer[0];
		return decode_DC16x16_16bit(x0, x1, x0, x1);
	case PLANE_4x4_BUFFERED_16_BIT:
		x0 = (__m128i)ctx->pred_buffer[16];
		x1 = (__m128i)ctx->pred_buffer[ctx->BlkIdx];
		x2 = _mm_add_epi32(x1, x0);
		x3 = _mm_add_epi32(x2, x0);
		x4 = _mm_add_epi32(x3, x0);
		x5 = _mm_min_epi16(_mm_packus_epi32(_mm_srai_epi32(x1, 5), _mm_srai_epi32(x2, 5)), (__m128i)ctx->clip);
		x6 = _mm_min_epi16(_mm_packus_epi32(_mm_srai_epi32(x3, 5), _mm_srai_epi32(x4, 5)), (__m128i)ctx->clip);
		return decode_Residual4x4(x5, x6);
	
	default:
		__builtin_unreachable();
	}
}



int decode_samples() {
	size_t stride = ctx->stride;
	uint8_t *p = ctx->plane + ctx->plane_offsets[ctx->BlkIdx] + stride;
	return decode_switch(stride, -stride, p, _mm_setzero_si128());
}