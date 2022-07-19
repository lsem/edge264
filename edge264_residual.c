#include "edge264_common.h"


static const v16qi normAdjust4x4[6] = {
	{10, 13, 10, 13, 13, 16, 13, 16, 10, 13, 10, 13, 13, 16, 13, 16},
	{11, 14, 11, 14, 14, 18, 14, 18, 11, 14, 11, 14, 14, 18, 14, 18},
	{13, 16, 13, 16, 16, 20, 16, 20, 13, 16, 13, 16, 16, 20, 16, 20},
	{14, 18, 14, 18, 18, 23, 18, 23, 14, 18, 14, 18, 18, 23, 18, 23},
	{16, 20, 16, 20, 20, 25, 20, 25, 16, 20, 16, 20, 20, 25, 20, 25},
	{18, 23, 18, 23, 23, 29, 23, 29, 18, 23, 18, 23, 23, 29, 23, 29},
};
static const v16qi normAdjust8x8[24] = {
	{20, 19, 25, 19, 20, 19, 25, 19, 19, 18, 24, 18, 19, 18, 24, 18},
	{25, 24, 32, 24, 25, 24, 32, 24, 19, 18, 24, 18, 19, 18, 24, 18},
	{20, 19, 25, 19, 20, 19, 25, 19, 19, 18, 24, 18, 19, 18, 24, 18},
	{25, 24, 32, 24, 25, 24, 32, 24, 19, 18, 24, 18, 19, 18, 24, 18},
	{22, 21, 28, 21, 22, 21, 28, 21, 21, 19, 26, 19, 21, 19, 26, 19},
	{28, 26, 35, 26, 28, 26, 35, 26, 21, 19, 26, 19, 21, 19, 26, 19},
	{22, 21, 28, 21, 22, 21, 28, 21, 21, 19, 26, 19, 21, 19, 26, 19},
	{28, 26, 35, 26, 28, 26, 35, 26, 21, 19, 26, 19, 21, 19, 26, 19},
	{26, 24, 33, 24, 26, 24, 33, 24, 24, 23, 31, 23, 24, 23, 31, 23},
	{33, 31, 42, 31, 33, 31, 42, 31, 24, 23, 31, 23, 24, 23, 31, 23},
	{26, 24, 33, 24, 26, 24, 33, 24, 24, 23, 31, 23, 24, 23, 31, 23},
	{33, 31, 42, 31, 33, 31, 42, 31, 24, 23, 31, 23, 24, 23, 31, 23},
	{28, 26, 35, 26, 28, 26, 35, 26, 26, 25, 33, 25, 26, 25, 33, 25},
	{35, 33, 45, 33, 35, 33, 45, 33, 26, 25, 33, 25, 26, 25, 33, 25},
	{28, 26, 35, 26, 28, 26, 35, 26, 26, 25, 33, 25, 26, 25, 33, 25},
	{35, 33, 45, 33, 35, 33, 45, 33, 26, 25, 33, 25, 26, 25, 33, 25},
	{32, 30, 40, 30, 32, 30, 40, 30, 30, 28, 38, 28, 30, 28, 38, 28},
	{40, 38, 51, 38, 40, 38, 51, 38, 30, 28, 38, 28, 30, 28, 38, 28},
	{32, 30, 40, 30, 32, 30, 40, 30, 30, 28, 38, 28, 30, 28, 38, 28},
	{40, 38, 51, 38, 40, 38, 51, 38, 30, 28, 38, 28, 30, 28, 38, 28},
	{36, 34, 46, 34, 36, 34, 46, 34, 34, 32, 43, 32, 34, 32, 43, 32},
	{46, 43, 58, 43, 46, 43, 58, 43, 34, 32, 43, 32, 34, 32, 43, 32},
	{36, 34, 46, 34, 36, 34, 46, 34, 34, 32, 43, 32, 34, 32, 43, 32},
	{46, 43, 58, 43, 46, 43, 58, 43, 34, 32, 43, 32, 34, 32, 43, 32},
};



/**
 * Inverse 4x4 transform
 *
 * Here we try to stay close to the spec's pseudocode, avoiding minor
 * optimisations that would make the code hard to understand.
 */
void FUNC(add_idct4x4, int iYCbCr, int qP, v16qu wS, int DCidx, uint8_t *samples)
{
	// loading and scaling
	__m128i zero = _mm_setzero_si128();
	__m128i sh = _mm_cvtsi32_si128(qP / 6);
	__m128i nA = (__m128i)normAdjust4x4[qP % 6];
	__m128i LS0 = _mm_mullo_epi16(_mm_unpacklo_epi8((__m128i)wS, zero), _mm_unpacklo_epi8(nA, zero));
	__m128i LS1 = _mm_mullo_epi16(_mm_unpackhi_epi8((__m128i)wS, zero), _mm_unpackhi_epi8(nA, zero));
	__m128i mul0 = _mm_sll_epi32(_mm_unpacklo_epi16(LS0, zero), sh);
	__m128i mul1 = _mm_sll_epi32(_mm_unpackhi_epi16(LS0, zero), sh);
	__m128i mul2 = _mm_sll_epi32(_mm_unpacklo_epi16(LS1, zero), sh);
	__m128i mul3 = _mm_sll_epi32(_mm_unpackhi_epi16(LS1, zero), sh);
	__m128i s8 = _mm_set1_epi32(8);
	v4si d0 = (v4si)_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(mul0, (__m128i)ctx->c_v[0]), s8), 4);
	__m128i d1 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(mul1, (__m128i)ctx->c_v[1]), s8), 4);
	__m128i d2 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(mul2, (__m128i)ctx->c_v[2]), s8), 4);
	__m128i d3 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(mul3, (__m128i)ctx->c_v[3]), s8), 4);
	if (DCidx >= 0)
		d0[0] = ctx->c[16 + DCidx];
	
	// horizontal 1D transform
	__m128i e0 = _mm_add_epi32((__m128i)d0, d2);
	__m128i e1 = _mm_sub_epi32((__m128i)d0, d2);
	__m128i e2 = _mm_sub_epi32(_mm_srai_epi32(d1, 1), d3);
	__m128i e3 = _mm_add_epi32(_mm_srai_epi32(d3, 1), d1);
	__m128i f0 = _mm_add_epi32(e0, e3);
	__m128i f1 = _mm_add_epi32(e1, e2);
	__m128i f2 = _mm_sub_epi32(e1, e2);
	__m128i f3 = _mm_sub_epi32(e0, e3);
	
	// matrix transposition
	__m128i x0 = _mm_unpacklo_epi32(f0, f1);
	__m128i x1 = _mm_unpacklo_epi32(f2, f3);
	__m128i x2 = _mm_unpackhi_epi32(f0, f1);
	__m128i x3 = _mm_unpackhi_epi32(f2, f3);
	__m128i s32 = _mm_set1_epi32(32);
	f0 = _mm_add_epi32(_mm_unpacklo_epi64(x0, x1), s32);
	f1 = _mm_unpackhi_epi64(x0, x1);
	f2 = _mm_unpacklo_epi64(x2, x3);
	f3 = _mm_unpackhi_epi64(x2, x3);
	
	// vertical 1D transform
	__m128i g0 = _mm_add_epi32(f0, f2);
	__m128i g1 = _mm_sub_epi32(f0, f2);
	__m128i g2 = _mm_sub_epi32(_mm_srai_epi32(f1, 1), f3);
	__m128i g3 = _mm_add_epi32(_mm_srai_epi32(f3, 1), f1);
	__m128i h0 = _mm_add_epi32(g0, g3);
	__m128i h1 = _mm_add_epi32(g1, g2);
	__m128i h2 = _mm_sub_epi32(g1, g2);
	__m128i h3 = _mm_sub_epi32(g0, g3);
	
	// final residual values
	__m128i r0 = _mm_packs_epi32(_mm_srai_epi32(h0, 6), _mm_srai_epi32(h1, 6));
	__m128i r1 = _mm_packs_epi32(_mm_srai_epi32(h2, 6), _mm_srai_epi32(h3, 6));
	
	// addition to values in place, clipping and storage
	size_t stride = ctx->stride[iYCbCr];
	if (ctx->clip[iYCbCr] == 255) {
		__m128i p0 = load4x2_8bit(samples             , samples + stride    , zero);
		__m128i p1 = load4x2_8bit(samples + stride * 2, samples + stride * 3, zero);
		v4si u = (v4si)_mm_packus_epi16(_mm_adds_epi16(p0, r0), _mm_adds_epi16(p1, r1));
		*(int32_t *)(samples             ) = u[0];
		*(int32_t *)(samples + stride    ) = u[1];
		*(int32_t *)(samples + stride * 2) = u[2];
		*(int32_t *)(samples + stride * 3) = u[3];
	} else {
		__m128i p0 = _mm_setr_epi64(*(__m64 *)(samples             ), *(__m64 *)(samples + stride    ));
		__m128i p1 = _mm_setr_epi64(*(__m64 *)(samples + stride * 2), *(__m64 *)(samples + stride * 3));
		__m128i clip = _mm_set1_epi16(ctx->clip[iYCbCr]);
		v2li u0 = (v2li)_mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p0, r0), zero), clip);
		v2li u1 = (v2li)_mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p1, r1), zero), clip);
		*(int64_t *)(samples             ) = u0[0];
		*(int64_t *)(samples + stride    ) = u0[1];
		*(int64_t *)(samples + stride * 2) = u1[0];
		*(int64_t *)(samples + stride * 3) = u1[1];
	}
}

void FUNC(add_dc4x4, int iYCbCr, int DCidx, uint8_t *samples) {
	__m128i x = _mm_srai_epi32(_mm_add_epi32(_mm_set1_epi32(ctx->c[16 + DCidx]), _mm_set1_epi32(32)), 6);
	__m128i r = _mm_packs_epi32(x, x);
	__m128i zero = _mm_setzero_si128();
	size_t stride = ctx->stride[iYCbCr];
	if (ctx->clip[iYCbCr] == 255) {
		__m128i p0 = load4x2_8bit(samples             , samples + stride    , zero);
		__m128i p1 = load4x2_8bit(samples + stride * 2, samples + stride * 3, zero);
		v4si u = (v4si)_mm_packus_epi16(_mm_adds_epi16(p0, r), _mm_adds_epi16(p1, r));
		*(int32_t *)(samples             ) = u[0];
		*(int32_t *)(samples + stride    ) = u[1];
		*(int32_t *)(samples + stride * 2) = u[2];
		*(int32_t *)(samples + stride * 3) = u[3];
	} else {
		__m128i p0 = _mm_setr_epi64(*(__m64 *)(samples             ), *(__m64 *)(samples + stride    ));
		__m128i p1 = _mm_setr_epi64(*(__m64 *)(samples + stride * 2), *(__m64 *)(samples + stride * 3));
		__m128i clip = _mm_set1_epi16(ctx->clip[iYCbCr]);
		v2li u0 = (v2li)_mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p0, r), zero), clip);
		v2li u1 = (v2li)_mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p1, r), zero), clip);
		*(int64_t *)(samples             ) = u0[0];
		*(int64_t *)(samples + stride    ) = u0[1];
		*(int64_t *)(samples + stride * 2) = u1[0];
		*(int64_t *)(samples + stride * 3) = u1[1];
	}
}



/**
 * Inverse 8x8 transform
 *
 * For 16bit, an AVX2 version is maintained that brings a decent speedup.
 */
static noinline void FUNC(add_idct8x8)
{
	if (ctx->clip[0] == 255) { // FIXME 4:4:4
		// loading and scaling
		__m128i s32 = _mm_set1_epi32(32);
		__m128i *c = (__m128i *)ctx->c_v;
		__m128i *s = NULL;//(__m128i *)ctx->LevelScale_v;
		__m128i d0 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[0], s[0]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[1], s[1]), s32), 6));
		__m128i d1 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[2], s[2]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[3], s[3]), s32), 6));
		__m128i d2 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[4], s[4]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[5], s[5]), s32), 6));
		__m128i d3 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[6], s[6]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[7], s[7]), s32), 6));
		__m128i d4 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[8], s[8]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[9], s[9]), s32), 6));
		__m128i d5 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[10], s[10]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[11], s[11]), s32), 6));
		__m128i d6 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[12], s[12]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[13], s[13]), s32), 6));
		__m128i d7 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[14], s[14]), s32), 6), _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(c[15], s[15]), s32), 6));
		
		for (int i = 2;;) {
			// 1D transform
			__m128i e0 = _mm_add_epi16(d0, d4);
			__m128i e1 = _mm_sub_epi16(_mm_sub_epi16(d5, d3), _mm_add_epi16(_mm_srai_epi16(d7, 1), d7));
			__m128i e2 = _mm_sub_epi16(d0, d4);
			__m128i e3 = _mm_sub_epi16(_mm_add_epi16(d1, d7), _mm_add_epi16(_mm_srai_epi16(d3, 1), d3));
			__m128i e4 = _mm_sub_epi16(_mm_srai_epi16(d2, 1), d6);
			__m128i e5 = _mm_add_epi16(_mm_sub_epi16(d7, d1), _mm_add_epi16(_mm_srai_epi16(d5, 1), d5));
			__m128i e6 = _mm_add_epi16(_mm_srai_epi16(d6, 1), d2);
			__m128i e7 = _mm_add_epi16(_mm_add_epi16(d3, d5), _mm_add_epi16(_mm_srai_epi16(d1, 1), d1));
			__m128i f0 = _mm_add_epi16(e0, e6);
			__m128i f1 = _mm_add_epi16(_mm_srai_epi16(e7, 2), e1);
			__m128i f2 = _mm_add_epi16(e2, e4);
			__m128i f3 = _mm_add_epi16(_mm_srai_epi16(e5, 2), e3);
			__m128i f4 = _mm_sub_epi16(e2, e4);
			__m128i f5 = _mm_sub_epi16(_mm_srai_epi16(e3, 2), e5);
			__m128i f6 = _mm_sub_epi16(e0, e6);
			__m128i f7 = _mm_sub_epi16(e7, _mm_srai_epi16(e1, 2));
			
			// Compilers freak out whenever output uses other registers.
			d0 = _mm_add_epi16(f0, f7);
			d1 = _mm_add_epi16(f2, f5);
			d2 = _mm_add_epi16(f4, f3);
			d3 = _mm_add_epi16(f6, f1);
			d4 = _mm_sub_epi16(f6, f1);
			d5 = _mm_sub_epi16(f4, f3);
			d6 = _mm_sub_epi16(f2, f5);
			d7 = _mm_sub_epi16(f0, f7);
			if (--i == 0)
				break;
			
			// matrix transposition
			__m128i x0 = _mm_unpacklo_epi16(d0, d1);
			__m128i x1 = _mm_unpacklo_epi16(d2, d3);
			__m128i x2 = _mm_unpacklo_epi16(d4, d5);
			__m128i x3 = _mm_unpacklo_epi16(d6, d7);
			__m128i x4 = _mm_unpackhi_epi16(d0, d1);
			__m128i x5 = _mm_unpackhi_epi16(d2, d3);
			__m128i x6 = _mm_unpackhi_epi16(d4, d5);
			__m128i x7 = _mm_unpackhi_epi16(d6, d7);
			__m128i x8 = _mm_unpacklo_epi32(x0, x1);
			__m128i x9 = _mm_unpacklo_epi32(x2, x3);
			__m128i xA = _mm_unpacklo_epi32(x4, x5);
			__m128i xB = _mm_unpacklo_epi32(x6, x7);
			__m128i xC = _mm_unpackhi_epi32(x0, x1);
			__m128i xD = _mm_unpackhi_epi32(x2, x3);
			__m128i xE = _mm_unpackhi_epi32(x4, x5);
			__m128i xF = _mm_unpackhi_epi32(x6, x7);
			d0 = _mm_add_epi16(_mm_unpacklo_epi64(x8, x9), _mm_set1_epi16(32));
			d1 = _mm_unpackhi_epi64(x8, x9);
			d2 = _mm_unpacklo_epi64(xA, xB);
			d3 = _mm_unpackhi_epi64(xA, xB);
			d4 = _mm_unpacklo_epi64(xC, xD);
			d5 = _mm_unpackhi_epi64(xC, xD);
			d6 = _mm_unpacklo_epi64(xE, xF);
			d7 = _mm_unpackhi_epi64(xE, xF);
		}
		
		// final residual values
		__m128i r0 = _mm_srai_epi16(d0, 6);
		__m128i r1 = _mm_srai_epi16(d1, 6);
		__m128i r2 = _mm_srai_epi16(d2, 6);
		__m128i r3 = _mm_srai_epi16(d3, 6);
		__m128i r4 = _mm_srai_epi16(d4, 6);
		__m128i r5 = _mm_srai_epi16(d5, 6);
		__m128i r6 = _mm_srai_epi16(d6, 6);
		__m128i r7 = _mm_srai_epi16(d7, 6);
		
		// addition to values in place, clipping and storage
		size_t stride = ctx->stride[0]; // FIXME 4:4:4
		size_t stride3 = stride * 3;
		uint8_t *p = NULL; //ctx->frame + ctx->frame_offsets_x[0/*BlkIdx2i4x4[BlkIdx]*/] + ctx->frame_offsets_y[0/*BlkIdx2i4x4[BlkIdx]*/];
		uint8_t *q = p + stride * 4;
		__m128i zero = _mm_setzero_si128();
		__m128i p0 = load8x1_8bit(p             , zero);
		__m128i p1 = load8x1_8bit(p + stride    , zero);
		__m128i p2 = load8x1_8bit(p + stride * 2, zero);
		__m128i p3 = load8x1_8bit(p + stride3   , zero);
		__m128i p4 = load8x1_8bit(q             , zero);
		__m128i p5 = load8x1_8bit(q + stride    , zero);
		__m128i p6 = load8x1_8bit(q + stride * 2, zero);
		__m128i p7 = load8x1_8bit(q + stride3   , zero);
		v2li u0 = (v2li)_mm_packus_epi16(_mm_adds_epi16(p0, r0), _mm_adds_epi16(p1, r1));
		v2li u1 = (v2li)_mm_packus_epi16(_mm_adds_epi16(p2, r2), _mm_adds_epi16(p3, r3));
		v2li u2 = (v2li)_mm_packus_epi16(_mm_adds_epi16(p4, r4), _mm_adds_epi16(p5, r5));
		v2li u3 = (v2li)_mm_packus_epi16(_mm_adds_epi16(p6, r6), _mm_adds_epi16(p7, r7));
		*(int64_t *)(p             ) = u0[0];
		*(int64_t *)(p + stride    ) = u0[1];
		*(int64_t *)(p + stride * 2) = u1[0];
		*(int64_t *)(p + stride3   ) = u1[1];
		*(int64_t *)(q             ) = u2[0];
		*(int64_t *)(q + stride    ) = u2[1];
		*(int64_t *)(q + stride * 2) = u3[0];
		*(int64_t *)(q + stride3   ) = u3[1];
	} else {
		
#ifdef __AVX2__
		// loading
		// FIXME scaling
		__m256i d0 = (__m256i)ctx->c_V[0];
		__m256i d1 = (__m256i)ctx->c_V[1];
		__m256i d2 = (__m256i)ctx->c_V[2];
		__m256i d3 = (__m256i)ctx->c_V[3];
		__m256i d4 = (__m256i)ctx->c_V[4];
		__m256i d5 = (__m256i)ctx->c_V[5];
		__m256i d6 = (__m256i)ctx->c_V[6];
		__m256i d7 = (__m256i)ctx->c_V[7];
		
		for (int i = 2;;) {
			// 1D transform
			__m256i e0 = _mm256_add_epi32(d0, d4);
			__m256i e1 = _mm256_sub_epi32(_mm256_sub_epi32(d5, d3), _mm256_add_epi32(_mm256_srai_epi32(d7, 1), d7));
			__m256i e2 = _mm256_sub_epi32(d0, d4);
			__m256i e3 = _mm256_sub_epi32(_mm256_add_epi32(d1, d7), _mm256_add_epi32(_mm256_srai_epi32(d3, 1), d3));
			__m256i e4 = _mm256_sub_epi32(_mm256_srai_epi32(d2, 1), d6);
			__m256i e5 = _mm256_add_epi32(_mm256_sub_epi32(d7, d1), _mm256_add_epi32(_mm256_srai_epi32(d5, 1), d5));
			__m256i e6 = _mm256_add_epi32(_mm256_srai_epi32(d6, 1), d2);
			__m256i e7 = _mm256_add_epi32(_mm256_add_epi32(d3, d5), _mm256_add_epi32(_mm256_srai_epi32(d1, 1), d1));
			__m256i f0 = _mm256_add_epi32(e0, e6);
			__m256i f1 = _mm256_add_epi32(_mm256_srai_epi32(e7, 2), e1);
			__m256i f2 = _mm256_add_epi32(e2, e4);
			__m256i f3 = _mm256_add_epi32(_mm256_srai_epi32(e5, 2), e3);
			__m256i f4 = _mm256_sub_epi32(e2, e4);
			__m256i f5 = _mm256_sub_epi32(_mm256_srai_epi32(e3, 2), e5);
			__m256i f6 = _mm256_sub_epi32(e0, e6);
			__m256i f7 = _mm256_sub_epi32(e7, _mm256_srai_epi32(e1, 2));
			
			// Compilers freak out whenever output uses other registers.
			d0 = _mm256_add_epi32(f0, f7);
			d1 = _mm256_add_epi32(f2, f5);
			d2 = _mm256_add_epi32(f4, f3);
			d3 = _mm256_add_epi32(f6, f1);
			d4 = _mm256_sub_epi32(f6, f1);
			d5 = _mm256_sub_epi32(f4, f3);
			d6 = _mm256_sub_epi32(f2, f5);
			d7 = _mm256_sub_epi32(f0, f7);
			if (--i == 0)
				break;
			
			// matrix transposition
			__m256i y0 = _mm256_unpacklo_epi32(d0, d1);
			__m256i y1 = _mm256_unpacklo_epi32(d2, d3);
			__m256i y2 = _mm256_unpacklo_epi32(d4, d5);
			__m256i y3 = _mm256_unpacklo_epi32(d6, d7);
			__m256i y4 = _mm256_unpackhi_epi32(d0, d1);
			__m256i y5 = _mm256_unpackhi_epi32(d2, d3);
			__m256i y6 = _mm256_unpackhi_epi32(d4, d5);
			__m256i y7 = _mm256_unpackhi_epi32(d6, d7);
			__m256i y8 = _mm256_unpacklo_epi64(y0, y1);
			__m256i y9 = _mm256_unpacklo_epi64(y2, y3);
			__m256i yA = _mm256_unpacklo_epi64(y4, y5);
			__m256i yB = _mm256_unpacklo_epi64(y6, y7);
			__m256i yC = _mm256_unpackhi_epi64(y0, y1);
			__m256i yD = _mm256_unpackhi_epi64(y2, y3);
			__m256i yE = _mm256_unpackhi_epi64(y4, y5);
			__m256i yF = _mm256_unpackhi_epi64(y6, y7);
			d0 = _mm256_add_epi32(_mm256_permute2x128_si256(y8, y9, _MM_SHUFFLE(0, 2, 0, 0)), _mm256_set1_epi32(32));
			d1 = _mm256_permute2x128_si256(yC, yD, _MM_SHUFFLE(0, 2, 0, 0));
			d2 = _mm256_permute2x128_si256(yA, yB, _MM_SHUFFLE(0, 2, 0, 0));
			d3 = _mm256_permute2x128_si256(yE, yF, _MM_SHUFFLE(0, 2, 0, 0));
			d4 = _mm256_permute2x128_si256(y8, y9, _MM_SHUFFLE(0, 3, 0, 1));
			d5 = _mm256_permute2x128_si256(yC, yD, _MM_SHUFFLE(0, 3, 0, 1));
			d6 = _mm256_permute2x128_si256(yA, yB, _MM_SHUFFLE(0, 3, 0, 1));
			d7 = _mm256_permute2x128_si256(yE, yF, _MM_SHUFFLE(0, 3, 0, 1));
		}
		
		// final residual values
		__m256i yG = _mm256_packs_epi32(_mm256_srai_epi32(d0, 6), _mm256_srai_epi32(d1, 6));
		__m256i yH = _mm256_packs_epi32(_mm256_srai_epi32(d2, 6), _mm256_srai_epi32(d3, 6));
		__m256i yI = _mm256_packs_epi32(_mm256_srai_epi32(d4, 6), _mm256_srai_epi32(d5, 6));
		__m256i yJ = _mm256_packs_epi32(_mm256_srai_epi32(d6, 6), _mm256_srai_epi32(d7, 6));
		__m256i r0 = _mm256_permute4x64_epi64(yG, _MM_SHUFFLE(3, 1, 2, 0));
		__m256i r1 = _mm256_permute4x64_epi64(yH, _MM_SHUFFLE(3, 1, 2, 0));
		__m256i r2 = _mm256_permute4x64_epi64(yI, _MM_SHUFFLE(3, 1, 2, 0));
		__m256i r3 = _mm256_permute4x64_epi64(yJ, _MM_SHUFFLE(3, 1, 2, 0));
		
		// addition to values in place, clipping and storage
		size_t stride = ctx->stride[0]; // FIXME 4:4:4
		size_t stride3 = stride * 3;
		uint8_t *p = NULL; //ctx->frame + ctx->frame_offsets_x[0/*BlkIdx2i4x4[BlkIdx]*/] + ctx->frame_offsets_y[0/*BlkIdx2i4x4[BlkIdx]*/];
		uint8_t *q = p + stride * 4;
		__m256i zero = _mm256_setzero_si256();
		__m256i clip = _mm256_set1_epi16(ctx->clip[0]); // FIXME 4:4:4
		__m256i p0 = _mm256_setr_m128i(*(__m128i *)(p             ), *(__m128i *)(p + stride ));
		__m256i p1 = _mm256_setr_m128i(*(__m128i *)(p + stride * 2), *(__m128i *)(p + stride3));
		__m256i p2 = _mm256_setr_m128i(*(__m128i *)(q             ), *(__m128i *)(q + stride ));
		__m256i p3 = _mm256_setr_m128i(*(__m128i *)(q + stride * 2), *(__m128i *)(q + stride3));
		__m256i u0 = _mm256_min_epi16(_mm256_max_epi16(_mm256_adds_epi16(p0, r0), zero), clip);
		__m256i u1 = _mm256_min_epi16(_mm256_max_epi16(_mm256_adds_epi16(p1, r1), zero), clip);
		__m256i u2 = _mm256_min_epi16(_mm256_max_epi16(_mm256_adds_epi16(p2, r2), zero), clip);
		__m256i u3 = _mm256_min_epi16(_mm256_max_epi16(_mm256_adds_epi16(p3, r3), zero), clip);
		*(__m128i *)(p             ) = _mm256_extracti128_si256(u0, 0);
		*(__m128i *)(p + stride    ) = _mm256_extracti128_si256(u0, 1);
		*(__m128i *)(p + stride * 2) = _mm256_extracti128_si256(u1, 0);
		*(__m128i *)(p + stride3   ) = _mm256_extracti128_si256(u1, 1);
		*(__m128i *)(q             ) = _mm256_extracti128_si256(u2, 0);
		*(__m128i *)(q + stride    ) = _mm256_extracti128_si256(u2, 1);
		*(__m128i *)(q + stride * 2) = _mm256_extracti128_si256(u3, 0);
		*(__m128i *)(q + stride3   ) = _mm256_extracti128_si256(u3, 1);

#else // !defined(__AVX2__)
		// load half of samples
		// FIXME scaline
		__m128i d0 = (__m128i)ctx->c_v[0];
		__m128i d1 = (__m128i)ctx->c_v[2];
		__m128i d2 = (__m128i)ctx->c_v[4];
		__m128i d3 = (__m128i)ctx->c_v[6];
		__m128i d4 = (__m128i)ctx->c_v[8];
		__m128i d5 = (__m128i)ctx->c_v[10];
		__m128i d6 = (__m128i)ctx->c_v[12];
		__m128i d7 = (__m128i)ctx->c_v[14];
		
		// This (crappy) version uses a nested loop trick to reduce code size
		for (int i = 2;; ) {
			for (int j = 2;; ) {
				// 1D transform
				__m128i e0 = _mm_add_epi32(d0, d4);
				__m128i e1 = _mm_sub_epi32(_mm_sub_epi32(d5, d3), _mm_add_epi32(_mm_srai_epi32(d7, 1), d7));
				__m128i e2 = _mm_sub_epi32(d0, d4);
				__m128i e3 = _mm_sub_epi32(_mm_add_epi32(d1, d7), _mm_add_epi32(_mm_srai_epi32(d3, 1), d3));
				__m128i e4 = _mm_sub_epi32(_mm_srai_epi32(d2, 1), d6);
				__m128i e5 = _mm_add_epi32(_mm_sub_epi32(d7, d1), _mm_add_epi32(_mm_srai_epi32(d5, 1), d5));
				__m128i e6 = _mm_add_epi32(_mm_srai_epi32(d6, 1), d2);
				__m128i e7 = _mm_add_epi32(_mm_add_epi32(d3, d5), _mm_add_epi32(_mm_srai_epi32(d1, 1), d1));
				__m128i f0 = _mm_add_epi32(e0, e6);
				__m128i f1 = _mm_add_epi32(_mm_srai_epi32(e7, 2), e1);
				__m128i f2 = _mm_add_epi32(e2, e4);
				__m128i f3 = _mm_add_epi32(_mm_srai_epi32(e5, 2), e3);
				__m128i f4 = _mm_sub_epi32(e2, e4);
				__m128i f5 = _mm_sub_epi32(_mm_srai_epi32(e3, 2), e5);
				__m128i f6 = _mm_sub_epi32(e0, e6);
				__m128i f7 = _mm_sub_epi32(e7, _mm_srai_epi32(e1, 2));
				d0 = _mm_add_epi32(f0, f7);
				d1 = _mm_add_epi32(f2, f5);
				d2 = _mm_add_epi32(f4, f3);
				d3 = _mm_add_epi32(f6, f1);
				d4 = _mm_sub_epi32(f6, f1);
				d5 = _mm_sub_epi32(f4, f3);
				d6 = _mm_sub_epi32(f2, f5);
				d7 = _mm_sub_epi32(f0, f7);
				if (--j == 0)
					break;
				
				// load other half of samples
				ctx->c_v[0] = (v4si)d0;
				d0 = (__m128i)ctx->c_v[1];
				ctx->c_v[2] = (v4si)d1;
				d1 = (__m128i)ctx->c_v[3];
				ctx->c_v[4] = (v4si)d2;
				d2 = (__m128i)ctx->c_v[5];
				ctx->c_v[6] = (v4si)d3;
				d3 = (__m128i)ctx->c_v[7];
				ctx->c_v[8] = (v4si)d4;
				d4 = (__m128i)ctx->c_v[9];
				ctx->c_v[10] = (v4si)d5;
				d5 = (__m128i)ctx->c_v[11];
				ctx->c_v[12] = (v4si)d6;
				d6 = (__m128i)ctx->c_v[13];
				ctx->c_v[14] = (v4si)d7;
				d7 = (__m128i)ctx->c_v[15];
			}
			if (--i == 0)
				break;
			
			// transpose the half matrix going to memory
			__m128i x0 = _mm_unpacklo_epi32((__m128i)ctx->c_v[8], (__m128i)ctx->c_v[10]);
			__m128i x1 = _mm_unpacklo_epi32((__m128i)ctx->c_v[12], (__m128i)ctx->c_v[14]);
			__m128i x2 = _mm_unpackhi_epi32((__m128i)ctx->c_v[8], (__m128i)ctx->c_v[10]);
			__m128i x3 = _mm_unpackhi_epi32((__m128i)ctx->c_v[12], (__m128i)ctx->c_v[14]);
			__m128i x4 = _mm_unpacklo_epi32(d4, d5);
			__m128i x5 = _mm_unpacklo_epi32(d6, d7);
			__m128i x6 = _mm_unpackhi_epi32(d4, d5);
			__m128i x7 = _mm_unpackhi_epi32(d6, d7);
			ctx->c_v[1] = (v4si)_mm_add_epi32(_mm_unpacklo_epi64(x0, x1), _mm_set1_epi32(32));
			ctx->c_v[3] = (v4si)_mm_unpackhi_epi64(x0, x1);
			ctx->c_v[5] = (v4si)_mm_unpacklo_epi64(x2, x3);
			ctx->c_v[7] = (v4si)_mm_unpackhi_epi64(x2, x3);
			ctx->c_v[9] = (v4si)_mm_unpacklo_epi64(x4, x5);
			ctx->c_v[11] = (v4si)_mm_unpackhi_epi64(x4, x5);
			ctx->c_v[13] = (v4si)_mm_unpacklo_epi64(x6, x7);
			ctx->c_v[15] = (v4si)_mm_unpackhi_epi64(x6, x7);
			
			// transpose the half matrix staying in registers
			__m128i x8 = _mm_unpacklo_epi32((__m128i)ctx->c_v[0], (__m128i)ctx->c_v[2]);
			__m128i x9 = _mm_unpacklo_epi32((__m128i)ctx->c_v[4], (__m128i)ctx->c_v[6]);
			__m128i xA = _mm_unpackhi_epi32((__m128i)ctx->c_v[0], (__m128i)ctx->c_v[2]);
			__m128i xB = _mm_unpackhi_epi32((__m128i)ctx->c_v[4], (__m128i)ctx->c_v[6]);
			__m128i xC = _mm_unpacklo_epi32(d0, d1);
			__m128i xD = _mm_unpacklo_epi32(d2, d3);
			__m128i xE = _mm_unpackhi_epi32(d0, d1);
			__m128i xF = _mm_unpackhi_epi32(d2, d3);
			d0 = _mm_add_epi32(_mm_unpacklo_epi64(x8, x9), _mm_set1_epi32(32));
			d1 = _mm_unpackhi_epi64(x8, x9);
			d2 = _mm_unpacklo_epi64(xA, xB);
			d3 = _mm_unpackhi_epi64(xA, xB);
			d4 = _mm_unpacklo_epi64(xC, xD);
			d5 = _mm_unpackhi_epi64(xC, xD);
			d6 = _mm_unpacklo_epi64(xE, xF);
			d7 = _mm_unpackhi_epi64(xE, xF);
		}
		
		// final residual values
		__m128i r0 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[0], 6), _mm_srai_epi32(d0, 6));
		__m128i r1 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[2], 6), _mm_srai_epi32(d1, 6));
		__m128i r2 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[4], 6), _mm_srai_epi32(d2, 6));
		__m128i r3 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[6], 6), _mm_srai_epi32(d3, 6));
		__m128i r4 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[8], 6), _mm_srai_epi32(d4, 6));
		__m128i r5 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[10], 6), _mm_srai_epi32(d5, 6));
		__m128i r6 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[12], 6), _mm_srai_epi32(d6, 6));
		__m128i r7 = _mm_packs_epi32(_mm_srai_epi32((__m128i)ctx->c_v[14], 6), _mm_srai_epi32(d7, 6));
		
		// addition to values in place, clipping and storage
		size_t stride = ctx->stride[0]; // FIXME 4:4:4
		size_t stride3 = stride * 3;
		uint8_t *p = NULL; //ctx->frame + ctx->frame_offsets_x[0/*BlkIdx2i4x4[BlkIdx]*/] + ctx->frame_offsets_y[0/*BlkIdx2i4x4[BlkIdx]*/];
		uint8_t *q = p + stride * 4;
		__m128i zero = _mm_setzero_si128();
		__m128i clip = _mm_set1_epi16(ctx->clip[0]); // FIXME 4:4:4
		__m128i p0 = *(__m128i *)(p             );
		__m128i p1 = *(__m128i *)(p + stride    );
		__m128i p2 = *(__m128i *)(p + stride * 2);
		__m128i p3 = *(__m128i *)(p + stride3   );
		__m128i p4 = *(__m128i *)(q             );
		__m128i p5 = *(__m128i *)(q + stride    );
		__m128i p6 = *(__m128i *)(q + stride * 2);
		__m128i p7 = *(__m128i *)(q + stride3   );
		__m128i u0 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p0, r0), zero), clip);
		__m128i u1 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p1, r1), zero), clip);
		__m128i u2 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p2, r2), zero), clip);
		__m128i u3 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p3, r3), zero), clip);
		__m128i u4 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p4, r4), zero), clip);
		__m128i u5 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p5, r5), zero), clip);
		__m128i u6 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p6, r6), zero), clip);
		__m128i u7 = _mm_min_epi16(_mm_max_epi16(_mm_adds_epi16(p7, r7), zero), clip);
		*(__m128i *)(p             ) = u0;
		*(__m128i *)(p + stride    ) = u1;
		*(__m128i *)(p + stride * 2) = u2;
		*(__m128i *)(p + stride3   ) = u3;
		*(__m128i *)(q             ) = u4;
		*(__m128i *)(q + stride    ) = u5;
		*(__m128i *)(q + stride * 2) = u6;
		*(__m128i *)(q + stride3   ) = u7;
#endif // __AVX2__
	}
}

// legacy function
static noinline void FUNC(decode_Residual8x8, __m128i p0,
	__m128i p1, __m128i p2, __m128i p3, __m128i p4, __m128i p5, __m128i p6,
	__m128i p7)
{
	size_t stride = ctx->stride[0]; // FIXME 4:4:4
	size_t stride3 = stride * 3;
	uint8_t *p = NULL; //ctx->frame + ctx->frame_offsets_x[0/*BlkIdx2i4x4[BlkIdx]*/] + ctx->frame_offsets_y[0/*BlkIdx2i4x4[BlkIdx]*/];
	uint8_t *q = p + stride * 4;
	if (__builtin_expect(ctx->clip[0] == 255, 1)) { // FIXME 4:4:4
		v2li u0 = (v2li)_mm_packus_epi16(p0, p1);
		v2li u1 = (v2li)_mm_packus_epi16(p2, p3);
		v2li u2 = (v2li)_mm_packus_epi16(p4, p5);
		v2li u3 = (v2li)_mm_packus_epi16(p6, p7);
		*(int64_t *)(p             ) = u0[0];
		*(int64_t *)(p + stride    ) = u0[1];
		*(int64_t *)(p + stride * 2) = u1[0];
		*(int64_t *)(p + stride3   ) = u1[1];
		*(int64_t *)(q             ) = u2[0];
		*(int64_t *)(q + stride    ) = u2[1];
		*(int64_t *)(q + stride * 2) = u3[0];
		*(int64_t *)(q + stride3   ) = u3[1];
	} else {
		*(__m128i *)(p             ) = p0;
		*(__m128i *)(p + stride    ) = p1;
		*(__m128i *)(p + stride * 2) = p2;
		*(__m128i *)(p + stride3   ) = p3;
		*(__m128i *)(q             ) = p4;
		*(__m128i *)(q + stride    ) = p5;
		*(__m128i *)(q + stride * 2) = p6;
		*(__m128i *)(q + stride3   ) = p7;
	}
	JUMP(add_idct8x8);
}
#define decode_Residual8x8_8bit decode_Residual8x8



/**
 * DC transforms
 * 
 * These functions do not gain enough from 8bit to justify distinct versions.
 */
void FUNC(transform_dc4x4, int iYCbCr)
{
	// load matrix in column order and multiply right
	__m128i c0 = (__m128i)ctx->c_v[0];
	__m128i c1 = (__m128i)ctx->c_v[1];
	__m128i c2 = (__m128i)ctx->c_v[2];
	__m128i c3 = (__m128i)ctx->c_v[3];
	__m128i x0 = _mm_add_epi32(c0, c1);
	__m128i x1 = _mm_add_epi32(c2, c3);
	__m128i x2 = _mm_sub_epi32(c0, c1);
	__m128i x3 = _mm_sub_epi32(c2, c3);
	__m128i x4 = _mm_add_epi32(x0, x1);
	__m128i x5 = _mm_sub_epi32(x0, x1);
	__m128i x6 = _mm_sub_epi32(x2, x3);
	__m128i x7 = _mm_add_epi32(x2, x3);
	
	// transpose
	__m128i x8 = _mm_unpacklo_epi32(x4, x5);
	__m128i x9 = _mm_unpacklo_epi32(x6, x7);
	__m128i xA = _mm_unpackhi_epi32(x4, x5);
	__m128i xB = _mm_unpackhi_epi32(x6, x7);
	__m128i xC = _mm_unpacklo_epi64(x8, x9);
	__m128i xD = _mm_unpackhi_epi64(x8, x9);
	__m128i xE = _mm_unpacklo_epi64(xA, xB);
	__m128i xF = _mm_unpackhi_epi64(xA, xB);
	
	// multiply left
	__m128i xG = _mm_add_epi32(xC, xD);
	__m128i xH = _mm_add_epi32(xE, xF);
	__m128i xI = _mm_sub_epi32(xC, xD);
	__m128i xJ = _mm_sub_epi32(xE, xF);
	__m128i f0 = _mm_add_epi32(xG, xH);
	__m128i f1 = _mm_sub_epi32(xG, xH);
	__m128i f2 = _mm_sub_epi32(xI, xJ);
	__m128i f3 = _mm_add_epi32(xI, xJ);
	
	// scale
	unsigned qP = ctx->QP[0]; // FIXME 4:4:4
	__m128i s32 = _mm_set1_epi32(32);
	__m128i LS = _mm_set1_epi32((ctx->ps.weightScale4x4[iYCbCr][0] * normAdjust4x4[qP % 6][0]) << (qP / 6));
	__m128i dc0 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(f0, LS), s32), 6);
	__m128i dc1 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(f1, LS), s32), 6);
	__m128i dc2 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(f2, LS), s32), 6);
	__m128i dc3 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(f3, LS), s32), 6);
	
	// store in zigzag order if needed later ...
	if (mb->bits[0] & 1 << 5) {
		ctx->c_v[4] = (v4si)_mm_unpacklo_epi64(dc0, dc1);
		ctx->c_v[5] = (v4si)_mm_unpackhi_epi64(dc0, dc1);
		ctx->c_v[6] = (v4si)_mm_unpacklo_epi64(dc2, dc3);
		ctx->c_v[7] = (v4si)_mm_unpackhi_epi64(dc2, dc3);
		
	// ... or prepare for storage in place
	} else {
		size_t stride = ctx->stride[iYCbCr];
		size_t stride3 = stride * 3;
		uint8_t *p = ctx->samples_mb[iYCbCr];
		uint8_t *q = p + stride * 4;
		uint8_t *r = p + stride * 8;
		uint8_t *s = q + stride * 8;
		__m128i r0 = _mm_srai_epi32(_mm_add_epi32(dc0, s32), 6);
		__m128i r1 = _mm_srai_epi32(_mm_add_epi32(dc1, s32), 6);
		__m128i r2 = _mm_srai_epi32(_mm_add_epi32(dc2, s32), 6);
		__m128i r3 = _mm_srai_epi32(_mm_add_epi32(dc3, s32), 6);
		__m128i shuflo = _mm_setr_epi8(0, 1, 0, 1, 0, 1, 0, 1, 4, 5, 4, 5, 4, 5, 4, 5);
		__m128i shufhi = _mm_setr_epi8(8, 9, 8, 9, 8, 9, 8, 9, 12, 13, 12, 13, 12, 13, 12, 13);
		__m128i lo0 = _mm_shuffle_epi8(r0, shuflo);
		__m128i lo1 = _mm_shuffle_epi8(r1, shuflo);
		__m128i lo2 = _mm_shuffle_epi8(r2, shuflo);
		__m128i lo3 = _mm_shuffle_epi8(r3, shuflo);
		__m128i hi0 = _mm_shuffle_epi8(r0, shufhi);
		__m128i hi1 = _mm_shuffle_epi8(r1, shufhi);
		__m128i hi2 = _mm_shuffle_epi8(r2, shufhi);
		__m128i hi3 = _mm_shuffle_epi8(r3, shufhi);
		
		// add to predicted samples
		if (ctx->clip[iYCbCr] == 255) {
			__m128i zero = _mm_setzero_si128();
			__m128i p0 = *(__m128i *)(p             );
			__m128i p1 = *(__m128i *)(p + stride    );
			__m128i p2 = *(__m128i *)(p + stride * 2);
			__m128i p3 = *(__m128i *)(p + stride3   );
			*(__m128i *)(p             ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p0, zero), lo0), _mm_adds_epi16(_mm_unpackhi_epi8(p0, zero), hi0));
			*(__m128i *)(p + stride    ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p1, zero), lo0), _mm_adds_epi16(_mm_unpackhi_epi8(p1, zero), hi0));
			*(__m128i *)(p + stride * 2) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p2, zero), lo0), _mm_adds_epi16(_mm_unpackhi_epi8(p2, zero), hi0));
			*(__m128i *)(p + stride3   ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p3, zero), lo0), _mm_adds_epi16(_mm_unpackhi_epi8(p3, zero), hi0));
			__m128i p4 = *(__m128i *)(q             );
			__m128i p5 = *(__m128i *)(q + stride    );
			__m128i p6 = *(__m128i *)(q + stride * 2);
			__m128i p7 = *(__m128i *)(q + stride3   );
			*(__m128i *)(q             ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p4, zero), lo1), _mm_adds_epi16(_mm_unpackhi_epi8(p4, zero), hi1));
			*(__m128i *)(q + stride    ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p5, zero), lo1), _mm_adds_epi16(_mm_unpackhi_epi8(p5, zero), hi1));
			*(__m128i *)(q + stride * 2) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p6, zero), lo1), _mm_adds_epi16(_mm_unpackhi_epi8(p6, zero), hi1));
			*(__m128i *)(q + stride3   ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p7, zero), lo1), _mm_adds_epi16(_mm_unpackhi_epi8(p7, zero), hi1));
			__m128i p8 = *(__m128i *)(r             );
			__m128i p9 = *(__m128i *)(r + stride    );
			__m128i pA = *(__m128i *)(r + stride * 2);
			__m128i pB = *(__m128i *)(r + stride3   );
			*(__m128i *)(r             ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p8, zero), lo2), _mm_adds_epi16(_mm_unpackhi_epi8(p8, zero), hi2));
			*(__m128i *)(r + stride    ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(p9, zero), lo2), _mm_adds_epi16(_mm_unpackhi_epi8(p9, zero), hi2));
			*(__m128i *)(r + stride * 2) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(pA, zero), lo2), _mm_adds_epi16(_mm_unpackhi_epi8(pA, zero), hi2));
			*(__m128i *)(r + stride3   ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(pB, zero), lo2), _mm_adds_epi16(_mm_unpackhi_epi8(pB, zero), hi2));
			__m128i pC = *(__m128i *)(s             );
			__m128i pD = *(__m128i *)(s + stride    );
			__m128i pE = *(__m128i *)(s + stride * 2);
			__m128i pF = *(__m128i *)(s + stride3   );
			*(__m128i *)(s             ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(pC, zero), lo3), _mm_adds_epi16(_mm_unpackhi_epi8(pC, zero), hi3));
			*(__m128i *)(s + stride    ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(pD, zero), lo3), _mm_adds_epi16(_mm_unpackhi_epi8(pD, zero), hi3));
			*(__m128i *)(s + stride * 2) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(pE, zero), lo3), _mm_adds_epi16(_mm_unpackhi_epi8(pE, zero), hi3));
			*(__m128i *)(s + stride3   ) = _mm_packus_epi16(_mm_adds_epi16(_mm_unpacklo_epi8(pF, zero), lo3), _mm_adds_epi16(_mm_unpackhi_epi8(pF, zero), hi3));
		}
	}
}

void FUNC(transform_dc2x2)
{
	// load both matrices interlaced+transposed and multiply right
	__m128i c0 = (__m128i)ctx->c_v[0];
	__m128i c1 = (__m128i)ctx->c_v[1];
	__m128i d0 = _mm_add_epi32(c0, c1);
	__m128i d1 = _mm_sub_epi32(c0, c1);
	
	// transpose and multiply left
	__m128i e0 = _mm_unpacklo_epi64(d0, d1);
	__m128i e1 = _mm_unpackhi_epi64(d0, d1);
	__m128i f0 = _mm_add_epi32(e0, e1);
	__m128i f1 = _mm_sub_epi32(e0, e1);
	
	// deinterlace and scale
	unsigned qPb = ctx->QP[1];
	unsigned qPr = ctx->QP[2];
	__m128i LSb = _mm_set1_epi32((ctx->ps.weightScale4x4[1 + mb->f.mbIsInterFlag * 3][0] * normAdjust4x4[qPb % 6][0]) << (qPb / 6));
	__m128i LSr = _mm_set1_epi32((ctx->ps.weightScale4x4[2 + mb->f.mbIsInterFlag * 3][0] * normAdjust4x4[qPr % 6][0]) << (qPr / 6));
	__m128i fb = (__m128i)_mm_shuffle_ps((__m128)f0, (__m128)f1, _MM_SHUFFLE(2, 0, 2, 0));
	__m128i fr = (__m128i)_mm_shuffle_ps((__m128)f0, (__m128)f1, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i dcCb = _mm_srai_epi32(_mm_mullo_epi32(fb, LSb), 5);
	__m128i dcCr = _mm_srai_epi32(_mm_mullo_epi32(fr, LSr), 5);
	
	// store if needed later ...
	if (mb->f.CodedBlockPatternChromaAC) {
		ctx->c_v[4] = (v4si)dcCb;
		ctx->c_v[5] = (v4si)dcCr;
		
	// ... or prepare for storage in place
	} else {
		size_t stride = ctx->stride[1];
		size_t stride3 = stride * 3;
		uint8_t *pb = ctx->samples_mb[1];
		uint8_t *pr = ctx->samples_mb[2];
		uint8_t *qb = pb + stride * 4;
		uint8_t *qr = pr + stride * 4;
		__m128i s32 = _mm_set1_epi32(32);
		__m128i rb = _mm_srai_epi32(_mm_add_epi32(dcCb, s32), 6);
		__m128i rr = _mm_srai_epi32(_mm_add_epi32(dcCr, s32), 6);
		__m128i shuflo = _mm_setr_epi8(0, 1, 0, 1, 0, 1, 0, 1, 4, 5, 4, 5, 4, 5, 4, 5);
		__m128i shufhi = _mm_setr_epi8(8, 9, 8, 9, 8, 9, 8, 9, 12, 13, 12, 13, 12, 13, 12, 13);
		__m128i lob = _mm_shuffle_epi8(rb, shuflo);
		__m128i hib = _mm_shuffle_epi8(rb, shufhi);
		__m128i lor = _mm_shuffle_epi8(rr, shuflo);
		__m128i hir = _mm_shuffle_epi8(rr, shufhi);
		__m128i zero = _mm_setzero_si128();
		
		// add to predicted samples
		if (ctx->clip[1] == 255) {
			__m128i b0 = _mm_adds_epi16(load8x1_8bit(pb             , zero), lob);
			__m128i b1 = _mm_adds_epi16(load8x1_8bit(pb + stride    , zero), lob);
			__m128i b2 = _mm_adds_epi16(load8x1_8bit(pb + stride * 2, zero), lob);
			__m128i b3 = _mm_adds_epi16(load8x1_8bit(pb + stride3   , zero), lob);
			__m128i b4 = _mm_adds_epi16(load8x1_8bit(qb             , zero), hib);
			__m128i b5 = _mm_adds_epi16(load8x1_8bit(qb + stride    , zero), hib);
			__m128i b6 = _mm_adds_epi16(load8x1_8bit(qb + stride * 2, zero), hib);
			__m128i b7 = _mm_adds_epi16(load8x1_8bit(qb + stride3   , zero), hib);
			v2li b8 = (v2li)_mm_packus_epi16(b0, b1);
			v2li b9 = (v2li)_mm_packus_epi16(b2, b3);
			v2li bA = (v2li)_mm_packus_epi16(b4, b5);
			v2li bB = (v2li)_mm_packus_epi16(b6, b7);
			*(int64_t *)(pb             ) = b8[0];
			*(int64_t *)(pb + stride    ) = b8[1];
			*(int64_t *)(pb + stride * 2) = b9[0];
			*(int64_t *)(pb + stride3   ) = b9[1];
			*(int64_t *)(qb             ) = bA[0];
			*(int64_t *)(qb + stride    ) = bA[1];
			*(int64_t *)(qb + stride * 2) = bB[0];
			*(int64_t *)(qb + stride3   ) = bB[1];
			__m128i r0 = _mm_adds_epi16(load8x1_8bit(pr             , zero), lor);
			__m128i r1 = _mm_adds_epi16(load8x1_8bit(pr + stride    , zero), lor);
			__m128i r2 = _mm_adds_epi16(load8x1_8bit(pr + stride * 2, zero), lor);
			__m128i r3 = _mm_adds_epi16(load8x1_8bit(pr + stride3   , zero), lor);
			__m128i r4 = _mm_adds_epi16(load8x1_8bit(qr             , zero), hir);
			__m128i r5 = _mm_adds_epi16(load8x1_8bit(qr + stride    , zero), hir);
			__m128i r6 = _mm_adds_epi16(load8x1_8bit(qr + stride * 2, zero), hir);
			__m128i r7 = _mm_adds_epi16(load8x1_8bit(qr + stride3   , zero), hir);
			v2li r8 = (v2li)_mm_packus_epi16(r0, r1);
			v2li r9 = (v2li)_mm_packus_epi16(r2, r3);
			v2li rA = (v2li)_mm_packus_epi16(r4, r5);
			v2li rB = (v2li)_mm_packus_epi16(r6, r7);
			*(int64_t *)(pr             ) = r8[0];
			*(int64_t *)(pr + stride    ) = r8[1];
			*(int64_t *)(pr + stride * 2) = r9[0];
			*(int64_t *)(pr + stride3   ) = r9[1];
			*(int64_t *)(qr             ) = rA[0];
			*(int64_t *)(qr + stride    ) = rA[1];
			*(int64_t *)(qr + stride * 2) = rB[0];
			*(int64_t *)(qr + stride3   ) = rB[1];
		}
	}
}

void FUNC(transform_dc2x4)
{
	int iYCbCr = (0/*BlkIdx*/ - 8) >> 3; // BlkIdx is 16 or 24
	unsigned qP_DC = 0; //mb->QP[iYCbCr] + 3;
	int w = ctx->ps.weightScale4x4[iYCbCr + mb->f.mbIsInterFlag * 3][0];
	int nA = normAdjust4x4[qP_DC % 6][0];
	__m128i x0 = (__m128i)ctx->c_v[0]; // {c00, c01, c10, c11} as per 8.5.11.1
	__m128i x1 = (__m128i)ctx->c_v[1]; // {c20, c21, c30, c31}
	__m128i x2 = _mm_add_epi32(x0, x1); // {c00+c20, c01+c21, c10+c30, c11+c31}
	__m128i x3 = _mm_sub_epi32(x0, x1); // {c00-c20, c01-c21, c10-c30, c11-c31}
	__m128i x4 = _mm_unpacklo_epi64(x2, x3); // {c00+c20, c01+c21, c00-c20, c01-c21}
	__m128i x5 = _mm_unpackhi_epi64(x2, x3); // {c10+c30, c11+c31, c10-c30, c11-c31}
	__m128i x6 = _mm_add_epi32(x4, x5); // {d00, d01, d10, d11}
	__m128i x7 = _mm_sub_epi32(x4, x5); // {d30, d31, d20, d21}
	__m128i x8 = _mm_hadd_epi32(x6, x7); // {f00, f10, f30, f20}
	__m128i x9 = _mm_hsub_epi32(x6, x7); // {f01, f11, f31, f21}
	__m128i s = _mm_set1_epi32((w * nA) << (qP_DC / 6 + ctx->ps.BitDepth_C - 8));
	__m128i s32 = _mm_set1_epi32(32);
	__m128i dc0 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(x8, s), s32), 6);
	__m128i dc1 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(x9, s), s32), 6);
	__m128i *c = (__m128i *)ctx->c_v + 2 + iYCbCr * 2;
	c[0] = _mm_unpacklo_epi32(dc0, dc1);
	c[1] = _mm_shuffle_epi32(_mm_unpackhi_epi64(dc0, dc1), _MM_SHUFFLE(2, 0, 3, 1));
}