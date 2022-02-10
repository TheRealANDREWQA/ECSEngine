#pragma once
#include "ecspch.h"
#include "../Core.h"

namespace ECSEngine {

#pragma region Cast

	// Only compiler hint - no instruction generated
	ECS_INLINE __m256 ECS_VECTORCALL CastToFloat(__m256i vector) {
		return _mm256_castsi256_ps(vector);
	}

	// Only compiler hint - no instruction generated
	ECS_INLINE __m256 ECS_VECTORCALL CastToFloat(__m256d vector) {
		return _mm256_castpd_ps(vector);
	}

	// Only compiler hint - no instruction generated
	ECS_INLINE __m256i ECS_VECTORCALL CastToInteger(__m256 vector) {
		return _mm256_castps_si256(vector);
	}

	// Only compiler hint - no instruction generated
	ECS_INLINE __m256i ECS_VECTORCALL CastToInteger(__m256d vector) {
		return _mm256_castpd_si256(vector);
	}

	// Only compiler hint - no instruction generated
	ECS_INLINE __m256d ECS_VECTORCALL CastToDouble(__m256 vector) {
		return _mm256_castps_pd(vector);
	}

	// Only compiler hint - no instruction generated
	ECS_INLINE __m256d ECS_VECTORCALL CastToDouble(__m256i vector) {
		return _mm256_castsi256_pd(vector);
	}

#pragma endregion

#pragma region Zero Vector

	ECS_INLINE __m256 ECS_VECTORCALL ZeroVectorFloat() {
		return _mm256_setzero_ps();
	}

	ECS_INLINE __m256i ECS_VECTORCALL ZeroVectorInteger() {
		return _mm256_setzero_si256();
	}

	ECS_INLINE __m256d ECS_VECTORCALL ZeroVectorDouble() {
		return _mm256_setzero_pd();
	}

#pragma endregion

#pragma region Dot

	// Conditionally multiply the packed single-precision(32-bit) floating-point elements in a and b 
	// using the high 4 bits in imm8, sum the four products, and conditionally store the sum in dst using the low 4 bits of imm8.
	template<int mask>
	Vec4f ECS_VECTORCALL Dot(Vec4f a, Vec4f b) {
		return _mm_dp_ps(a, b, mask);
	}

	// Conditionally multiply the packed single-precision(32-bit) floating-point elements in a and b 
	// using the high 4 bits in imm8, sum the four products, and conditionally store the sum in dst using the low 4 bits of imm8.
	// Works as if doing two dots for the high and the low part of the vector
	template<int mask>
	Vec8f ECS_VECTORCALL Dot(Vec8f a, Vec8f b) {
		return _mm256_dp_ps(a, b, mask);
	}

	// Conditionally multiply the packed double-precision (64-bit) floating-point elements in a and b using the high 4 bits in imm8, 
	// sum the two products, and conditionally store the sum in dst using the low 4 bits of imm8.
	template<int mask>
	Vec2d ECS_VECTORCALL Dot(Vec2d a, Vec2d b) {
		return _mm_dp_pd(a, b, mask);
	}

#pragma endregion

#pragma region Fmadd

	// Multiply packed single-precision(32-bit) floating-point elements in a and b, add the intermediate result 
	// to packed elements in c, and store the results in dst.
	ECS_INLINE Vec4f ECS_VECTORCALL Fmadd(Vec4f a, Vec4f b, Vec4f c) {
		return _mm_fmadd_ps(a, b, c);
	}

	// Multiply packed single-precision(32-bit) floating-point elements in a and b, add the intermediate result 
	// to packed elements in c, and store the results in dst.
	ECS_INLINE Vec8f ECS_VECTORCALL Fmadd(Vec8f a, Vec8f b, Vec8f c) {
		return _mm256_fmadd_ps(a, b, c);
	}

	// Multiply packed double-precision (64-bit) floating-point elements in a and b, add the intermediate result 
	// to packed elements in c, and store the results in dst.
	ECS_INLINE Vec2d ECS_VECTORCALL Fmadd(Vec2d a, Vec2d b, Vec2d c) {
		return _mm_fmadd_pd(a, b, c);
	}

	// Multiply packed double-precision (64-bit) floating-point elements in a and b, add the intermediate result 
	// to packed elements in c, and store the results in dst.
	ECS_INLINE Vec4d ECS_VECTORCALL Fmadd(Vec4d a, Vec4d b, Vec4d c) {
		return _mm256_fmadd_pd(a, b, c);
	}

#pragma endregion

#pragma region Fmsub

	// Multiply packed single-precision(32-bit) floating-point elements in a and b, subtract packed elements in c from the
	// intermediate result, and store the results in dst.
	ECS_INLINE Vec4f ECS_VECTORCALL Fmsub(Vec4f a, Vec4f b, Vec4f c) {
		return _mm_fmsub_ps(a, b, c);
	}

	// Multiply packed single-precision(32-bit) floating-point elements in a and b, subtract packed elements in c from the
	// intermediate result, and store the results in dst.
	ECS_INLINE Vec8f ECS_VECTORCALL Fmsub(Vec8f a, Vec8f b, Vec8f c) {
		return _mm256_fmsub_ps(a, b, c);
	}

	// Multiply packed double-precision(32-bit) floating-point elements in a and b, subtract packed elements in c from the
	// intermediate result, and store the results in dst.
	ECS_INLINE Vec2d ECS_VECTORCALL Fmsub(Vec2d a, Vec2d b, Vec2d c) {
		return _mm_fmsub_pd(a, b, c);
	}

	// Multiply packed double-precision(32-bit) floating-point elements in a and b, subtract packed elements in c from the
	// intermediate result, and store the results in dst.
	ECS_INLINE Vec4d ECS_VECTORCALL Fmsub(Vec4d a, Vec4d b, Vec4d c) {
		return _mm256_fmsub_pd(a, b, c);
	}

#pragma endregion

#pragma region Fmaddsub

	// Multiply packed single-precision (32-bit) floating-point elements in a and b, alternatively add and subtract packed 
	// elements in c to/from the intermediate result, and store the results in dst.
	ECS_INLINE Vec4f ECS_VECTORCALL Fmaddsub(Vec4f a, Vec4f b, Vec4f c) {
		return _mm_fmaddsub_ps(a, b, c);
	}

	// Multiply packed single-precision (32-bit) floating-point elements in a and b, alternatively add and subtract packed 
	// elements in c to/from the intermediate result, and store the results in dst.
	ECS_INLINE Vec8f ECS_VECTORCALL Fmaddsub(Vec8f a, Vec8f b, Vec8f c) {
		return _mm256_fmaddsub_ps(a, b, c);
	}

	// Multiply packed double-precision (32-bit) floating-point elements in a and b, alternatively add and subtract packed 
	// elements in c to/from the intermediate result, and store the results in dst.
	ECS_INLINE Vec2d ECS_VECTORCALL Fmaddsub(Vec2d a, Vec2d b, Vec2d c) {
		return _mm_fmaddsub_pd(a, b, c);
	}

	// Multiply packed double-precision (32-bit) floating-point elements in a and b, alternatively add and subtract packed 
	// elements in c to/from the intermediate result, and store the results in dst.
	ECS_INLINE Vec4d ECS_VECTORCALL Fmaddsub(Vec4d a, Vec4d b, Vec4d c) {
		return _mm256_fmaddsub_pd(a, b, c);
	}

#pragma endregion

#pragma region Fmsubadd

	// Multiply packed single-precision (32-bit) floating-point elements in a and b, alternatively subtract and add packed 
	// elements in c from/to the intermediate result, and store the results in dst.
	ECS_INLINE Vec4f ECS_VECTORCALL Fmsubadd(Vec4f a, Vec4f b, Vec4f c) {
		return _mm_fmsubadd_ps(a, b, c);
	}

	// Multiply packed single-precision (32-bit) floating-point elements in a and b, alternatively subtract and add packed 
	// elements in c from/to the intermediate result, and store the results in dst.
	ECS_INLINE Vec8f ECS_VECTORCALL Fmsubadd(Vec8f a, Vec8f b, Vec8f c) {
		return _mm256_fmsubadd_ps(a, b, c);
	}

	// Multiply packed double-precision (32-bit) floating-point elements in a and b, alternatively subtract and add packed 
	// elements in c from/to the intermediate result, and store the results in dst.
	ECS_INLINE Vec2d ECS_VECTORCALL Fmsubadd(Vec2d a, Vec2d b, Vec2d c) {
		return _mm_fmsubadd_pd(a, b, c);
	}

	// Multiply packed double-precision (32-bit) floating-point elements in a and b, alternatively subtract and add packed 
	// elements in c from/to the intermediate result, and store the results in dst.
	ECS_INLINE Vec4d ECS_VECTORCALL Fmsubadd(Vec4d a, Vec4d b, Vec4d c) {
		return _mm256_fmsubadd_pd(a, b, c);
	}

#pragma endregion

#pragma region Permute2f128

	/* Shuffle 128-bits (composed of 4 packed single-precision (32-bit) floating-point elements) selected by 
	imm8 from a and b, and store the results in dst.

	DEFINE SELECT4(src1, src2, control) {
	CASE(control[1:0]) OF
	0:	tmp[127:0] := src1[127:0]
	1:	tmp[127:0] := src1[255:128]
	2:	tmp[127:0] := src2[127:0]
	3:	tmp[127:0] := src2[255:128]
	ESAC
	IF control[3]
		tmp[127:0] := 0
	FI
	RETURN tmp[127:0]
	}
	dst[127:0] := SELECT4(a[255:0], b[255:0], imm8[3:0])
	dst[255:128] := SELECT4(a[255:0], b[255:0], imm8[7:4])
	dst[MAX:256] := 0
	*/
	template<int mask>
	Vec8f ECS_VECTORCALL Permute2f128(Vec8f a, Vec8f b) {
		return _mm256_permute2f128_ps(a, b, mask);
	}

	/* Shuffle 128-bits (composed of 2 packed double-precision (64-bit) floating-point elements) selected by 
	imm8 from a and b, and store the results in dst.
	DEFINE SELECT4(src1, src2, control) {
	CASE(control[1:0]) OF
	0:	tmp[127:0] := src1[127:0]
	1:	tmp[127:0] := src1[255:128]
	2:	tmp[127:0] := src2[127:0]
	3:	tmp[127:0] := src2[255:128]
	ESAC
	IF control[3]
		tmp[127:0] := 0
	FI
	RETURN tmp[127:0]
	}
	dst[127:0] := SELECT4(a[255:0], b[255:0], imm8[3:0])
	dst[255:128] := SELECT4(a[255:0], b[255:0], imm8[7:4])
	dst[MAX:256] := 0
	*/
	template<int mask>
	Vec4d ECS_VECTORCALL Permute2d128(Vec4d a, Vec4d b) {
		return _mm256_permute2f128_pd(a, b, mask);
	}

#pragma endregion

#pragma region Permute4x64

	/* Shuffle double-precision (64-bit) floating-point elements in a across lanes using the control in imm8,
	and store the results in dst.
	DEFINE SELECT4(src, control) {
	CASE(control[1:0]) OF
	0:	tmp[63:0] := src[63:0]
	1:	tmp[63:0] := src[127:64]
	2:	tmp[63:0] := src[191:128]
	3:	tmp[63:0] := src[255:192]
	ESAC
	RETURN tmp[63:0]
	}
	dst[63:0] := SELECT4(a[255:0], imm8[1:0])
	dst[127:64] := SELECT4(a[255:0], imm8[3:2])
	dst[191:128] := SELECT4(a[255:0], imm8[5:4])
	dst[255:192] := SELECT4(a[255:0], imm8[7:6])
	dst[MAX:256] := 0
	*/
	template<int mask>
	Vec4d ECS_VECTORCALL Permute4x64(Vec4d a) {
		return _mm256_permute4x64_pd(a, mask);
	}

#pragma endregion

#pragma region Precull Horizontal Find First

	// Use horizontal or to precull the find first search
	// Horizontal find first uses some SIMD and scalar instructions
	// while horizontal or is mostly a single SIMD instruction
	template<typename VectorType>
	void ECS_VECTORCALL PrecullHorizontalFindFirst(VectorType vector, int& flag) {
		if (horizontal_or(vector)) {
			flag = horizontal_find_first(vector);
		}
	}

#pragma endregion

#pragma region Broadcast element

	template<int position>
	__m256 ECS_VECTORCALL Broadcast8(__m256 vector) {
		static_assert(position >= 0);
		// Avoid using permutevar8x32 because that require a global constant vector of indices
		// Permute inside lanes and the choose the appropriate vector to broadcast since it needs
		// only an __m128

		__m256 permutation;		
		if constexpr (position < 4) {
			// Only interested in the low part (bits 1 and 0)
			if constexpr (position == 0) {
				permutation = vector;
			}
			else {
				permutation = _mm256_permute_ps(vector, position);
			}
			return _mm256_broadcastss_ps(_mm256_castps256_ps128(permutation));
		}
		else {
			constexpr int shifted_position = position >> 2;
			permutation = _mm256_permute_ps(vector, shifted_position);
			return _mm256_broadcastss_ps(_mm256_extractf128_ps(permutation, 1));
		}
	}

	template<int position>
	__m256i ECS_VECTORCALL Broadcast8(__m256i vector) {
		return _mm256_castps_si256(Broadcast8<position>(_mm256_castsi256_ps(vector)));
	}

	template<int position>
	__m256i ECS_VECTORCALL Broadcast16(__m256i vector) {
		// Avoid using permutevar8x32 because that require a global constant vector of indices
		// Permute inside lanes and the choose the appropriate vector to broadcast since it needs
		// only an __m128
		static_assert(position >= 0);

		__m256i permutation;
		if constexpr (position == 0) {
			permutation = vector;
		}
		else if constexpr (position < 4) {
			// It is in the low 64 bits of the register
			// Use shift lower to brind the value inside the low word
			permutation = _mm256_slli_epi64(vector, position * 16);
		}
		else {
			// The 16 bit types lack dedicated permute or shuffle functions
			// There is only shufflelo_epi16 and shufflehi_epi16 which operate on 16 bits
			// Bring the 64 bit chunk that contains the item in the low qword of the vector
			// permutevar4x64 works across lanes boundaries
			permutation = _mm256_permute4x64_epi64(vector, position >> 2);
			// Check to see if it is already in the lowest word
			// If it is not then bring it there
			if constexpr (position % 4 != 0) {
				// It is in the low 64 bits of the register
				// Use shuffle lo in order to bring the value in the low word
				permutation = _mm256_slli_epi64(vector, (position % 4) * 16);
			}
		}

		// Broadcast now
		return _mm256_broadcastw_epi16(_mm256_castsi256_si128(vector));
	}

	template<int position>
	__m256i ECS_VECTORCALL Broadcast32(__m256i vector) {
		static_assert(position >= 0);

		__m256i permutation;
		if constexpr (position == 0) {
			permutation = vector;
		}
		else {
			if constexpr (position < 8) {
				// It is in the low word of the vector, only shift it
				permutation = _mm256_slli_epi64(vector, position * 8);
			}
			else {
				// Use permutevar in order to bring the qword containing the byte into the low qword and then shift
				permutation = _mm256_permute4x64_epi64(vector, position >> 4);
				if constexpr (position % 8 != 0) {
					permutation = _mm256_slli_epi64(vector, (position % 8) * 8);
				}
			}
		}

		return _mm256_broadcastb_epi8(_mm256_castsi256_si128(permutation));
	}

	template<int position>
	__m256i ECS_VECTORCALL Broadcast4(__m256i vector) {
		static_assert(position >= 0);

		__m256i permutation;
		if constexpr (position == 0) {
			permutation = vector;
		}
		else {
			permutation = _mm256_permute4x64_epi64(vector, position);
		}

		return _mm256_broadcastq_epi64(_mm256_castsi256_si128(permutation));
	}

	template<int position>
	__m256d ECS_VECTORCALL Broadcast4(__m256d vector) {
		return _mm256_castsi256_pd(Broadcast4<position>(_mm256_castpd_si256(vector)));
	}


#pragma endregion

#pragma region Load Partial

	// Count must be between [0; 8)
	ECS_INLINE __m256 ECS_VECTORCALL LoadPartial8(const void* data, int count) {
		// Create a blend mask according to the count
		alignas(32) uint32_t masks[sizeof(__m256) / sizeof(uint32_t)] = { 0xFFFFFFFF };
		for (size_t index = 0; index < count; index++) {
			masks[index] = 0;
		}
		__m256 vector = _mm256_loadu_ps((const float*)data);
		__m256 zero = _mm256_setzero_ps();

		__m256 mask = _mm256_load_ps((const float*)masks);
		return _mm256_blendv_ps(vector, zero, mask);
	}

	// Count must be between [0; 32)
	ECS_INLINE __m256i ECS_VECTORCALL LoadPartial32(const void* data, int count) {
		// Create a blend mask according to the count
		alignas(32) uint8_t masks[sizeof(__m256i) / sizeof(uint8_t)] = { 0xFF };
		for (size_t index = 0; index < count; index++) {
			masks[index] = 0;
		}

		__m256i vector = _mm256_loadu_epi8(data);
		__m256i zero = _mm256_setzero_si256();

		__m256i mask = _mm256_load_si256((const __m256i*)masks);
		return _mm256_blendv_epi8(vector, zero, mask);
	}

	// Count must be between [0; 16)
	ECS_INLINE __m256i ECS_VECTORCALL LoadPartial16(const void* data, int count) {
		// Create a blend mask according to the count
		alignas(32) uint16_t masks[sizeof(__m256i) / sizeof(uint16_t)] = { 0xFFFF };
		for (size_t index = 0; index < count; index++) {
			masks[index] = 0;
		}

		__m256i vector = _mm256_loadu_epi16(data);
		__m256i zero = _mm256_setzero_si256();

		__m256i mask = _mm256_load_si256((const __m256i*)masks);
		return _mm256_blendv_epi8(vector, zero, mask);
	}

	// Count must be between [0; 4)
	ECS_INLINE __m256i ECS_VECTORCALL LoadPartial4(const void* data, int count) {
		// Use a switch to determine how many elements to mask out
		__m256i vector = _mm256_loadu_epi64(data);
		__m256i zero = _mm256_setzero_si256();

		__m256i result = vector;
		switch (count) {
		case 1:
			// Only the first element will be kept - 2 32bit values
			result = _mm256_blend_epi32(zero, vector, 0x03);
			break;
		case 2:
			result = _mm256_blend_epi32(zero, vector, 0x0F);
			break;
		case 3:
			result = _mm256_blend_epi32(zero, vector, (1 << 6) - 1);
			break;
		}
		return result;
	}

#pragma endregion

#pragma region Dynamic Mask Creation

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask32(
		bool element_0,
		bool element_1,
		bool element_2,
		bool element_3,
		bool element_4,
		bool element_5,
		bool element_6,
		bool element_7,
		bool element_8,
		bool element_9,
		bool element_10,
		bool element_11,
		bool element_12,
		bool element_13,
		bool element_14,
		bool element_15,
		bool element_16,
		bool element_17,
		bool element_18,
		bool element_19,
		bool element_20,
		bool element_21,
		bool element_22,
		bool element_23,
		bool element_24,
		bool element_25,
		bool element_26,
		bool element_27,
		bool element_28,
		bool element_29,
		bool element_30,
		bool element_31
	) {
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)];
#define LOOP_ITERATION(index) mask[index] = element_##index ? UCHAR_MAX : 0;

		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 4);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 8);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 12);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 16);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 20);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 24);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 28);

#undef LOOP_ITERATION

		return _mm256_load_si256((const __m256i*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask32(const bool* elements) {
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)];
#define LOOP_ITERATION(index) mask[index] = elements[index] ? UCHAR_MAX : 0;

		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 4);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 8);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 12);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 16);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 20);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 24);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 28);

#undef LOOP_ITERATION

		return _mm256_load_si256((const __m256i*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask16(
		bool element_0,
		bool element_1,
		bool element_2,
		bool element_3,
		bool element_4,
		bool element_5,
		bool element_6,
		bool element_7,
		bool element_8,
		bool element_9,
		bool element_10,
		bool element_11,
		bool element_12,
		bool element_13,
		bool element_14,
		bool element_15
	) {
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint16_t mask[sizeof(__m256i) / sizeof(uint16_t)];
#define LOOP_ITERATION(index) mask[index] = element_##index ? UINT_MAX : 0;

		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 4);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 8);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 12);

#undef LOOP_ITERATION

		return _mm256_load_si256((const __m256i*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask16(const bool* elements) {
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint16_t mask[sizeof(__m256i) / sizeof(uint16_t)];
#define LOOP_ITERATION(index) mask[index] = elements[index] ? UINT_MAX : 0;

		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 4);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 8);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 12);

#undef LOOP_ITERATION

		return _mm256_load_si256((const __m256i*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256 ECS_VECTORCALL SelectMask8(
		bool element_0,
		bool element_1,
		bool element_2,
		bool element_3,
		bool element_4,
		bool element_5,
		bool element_6,
		bool element_7
	) {
		__m256 zeros = ZeroVectorFloat();

		alignas(32) uint32_t mask[sizeof(__m256) / sizeof(float)];
#define LOOP_ITERATION(index) mask[index] = element_##index ? UINT_MAX : 0; 

		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 4);

#undef LOOP_ITERATION

		return _mm256_load_ps((const float*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256 ECS_VECTORCALL SelectMask8(const bool* elements) {
		__m256 zeros = ZeroVectorFloat();

		alignas(32) uint32_t mask[sizeof(__m256) / sizeof(float)];
#define LOOP_ITERATION(index) mask[index] = elements[index] ? UINT_MAX : 0; 

		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);
		LOOP_UNROLL_4(4, LOOP_ITERATION, 4);

#undef LOOP_ITERATION

		return _mm256_load_ps((const float*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask8Integer(
		bool element_0,
		bool element_1,
		bool element_2,
		bool element_3,
		bool element_4,
		bool element_5,
		bool element_6,
		bool element_7
	) {
		return _mm256_castps_si256(SelectMask8(element_0, element_1, element_2, element_3, element_4, element_5, element_6, element_7));
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask8Integer(const bool* elements) {
		return _mm256_castps_si256(SelectMask8(elements));
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask4Integer(
		bool element_0,
		bool element_1,
		bool element_2,
		bool element_3
	) {
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint64_t mask[sizeof(__m256i) / sizeof(uint64_t)];
#define LOOP_ITERATION(index) mask[index] = element_##index ? UINT_MAX : 0; 
		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);

#undef LOOP_ITERATION

		return _mm256_load_si256((const __m256i*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256i ECS_VECTORCALL SelectMask4Integer(const bool* elements) {
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint64_t mask[sizeof(__m256i) / sizeof(uint64_t)];
#define LOOP_ITERATION(index) mask[index] = elements[index] ? UINT_MAX : 0; 
		LOOP_UNROLL_4(4, LOOP_ITERATION, 0);

#undef LOOP_ITERATION

		return _mm256_load_si256((const __m256i*)mask);
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256d ECS_VECTORCALL SelectMask4(
		bool element_0,
		bool element_1,
		bool element_2,
		bool element_3
	) {
		return _mm256_castsi256_pd(SelectMask4Integer(element_0, element_1, element_2, element_3));
	}

	// Dynamic mask creation - if possible consider using the compile time variant
	ECS_INLINE __m256d ECS_VECTORCALL SelectMask4(const bool* elements) {
		return _mm256_castsi256_pd(SelectMask4Integer(elements));
	}

#pragma endregion

#pragma region Dynamic Select Element

	// All the other elements will be zerod
	ECS_INLINE __m256d ECS_VECTORCALL SelectElement(__m256d vector, unsigned int position) {
		__m256d zeros = ZeroVectorDouble();
		switch (position) {
		case 0:
			return _mm256_blend_pd(zeros, vector, 1 << 0);
		case 1:
			return _mm256_blend_pd(zeros, vector, 1 << 1);
		case 2:
			return _mm256_blend_pd(zeros, vector, 1 << 2);
		case 3:
			return _mm256_blend_pd(zeros, vector, 1 << 3);
		}

		return zeros;
	}

	// All the other elements will be zeroed
	ECS_INLINE __m256 ECS_VECTORCALL SelectElement(__m256 vector, unsigned int position) {
		__m256 zeros = ZeroVectorFloat();
		switch (position) {
		case 0:
			return _mm256_blend_ps(zeros, vector, 1 << 0);
		case 1:
			return _mm256_blend_ps(zeros, vector, 1 << 1);
		case 2:
			return _mm256_blend_ps(zeros, vector, 1 << 2);
		case 3:
			return _mm256_blend_ps(zeros, vector, 1 << 3);
		case 4:
			return _mm256_blend_ps(zeros, vector, 1 << 4);
		case 5:
			return _mm256_blend_ps(zeros, vector, 1 << 5);
		case 6:
			return _mm256_blend_ps(zeros, vector, 1 << 6);
		case 7:
			return _mm256_blend_ps(zeros, vector, 1 << 7);
		}

		// If no correct value is specified, return a vector full of zeros
		return zeros;
	}

	// All the other elements will be zeroed
	ECS_INLINE __m256i ECS_VECTORCALL SelectElement4(__m256i vector, unsigned int position) {
		return CastToInteger(SelectElement(CastToDouble(vector), position));
	}

	// All the other elements will be zeroed
	ECS_INLINE __m256i ECS_VECTORCALL SelectElement8(__m256i vector, unsigned int position) {
		return CastToInteger(SelectElement(CastToFloat(vector), position));
	}

	// All the other elements will be zeroed
	ECS_INLINE __m256i ECS_VECTORCALL SelectElement16(__m256i vector, unsigned int position) {
		// A mask will be used - 16 condition switch table might be too much
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint16_t mask[sizeof(__m256i) / sizeof(uint16_t)] = { 0 };
		mask[position] = USHORT_MAX;

		__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);
		return _mm256_blendv_epi8(zeros, vector, vector_mask);
	}

	// All the other elements will be zeroed
	ECS_INLINE __m256i ECS_VECTORCALL SelectElement32(__m256i vector, unsigned int position) {
		// A mask will be used - 32 condition switch table might be too much
		__m256i zeros = ZeroVectorInteger();

		alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)] = { 0 };
		mask[position] = UCHAR_MAX;

		__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);
		return _mm256_blendv_epi8(zeros, vector, vector_mask);
	}

#pragma endregion

#pragma region Rotate Elements (not intra elements)

	template<bool right>
	__m256 ECS_VECTORCALL Rotate(__m256 vector, unsigned int count) {
		alignas(32) uint32_t indices[sizeof(__m256i) / sizeof(uint32_t)];
		for (unsigned int index = 0; index < 7; index++) {
			if constexpr (right) {
				unsigned int offset = count + index;
				indices[index] = (offset) >= 8 ? offset - 8 : offset;
			}
			else {
				int offset = (int)count - (int)index;
				indices[index] = offset < 0 ? offset + 8 : offset;
			}
		}

		__m256i vector_indices = _mm256_load_si256((const __m256i*)indices);
		return _mm256_permutevar8x32_ps(vector, vector_indices);
	}

	template<bool right>
	__m256d ECS_VECTORCALL Rotate(__m256d vector, unsigned int count) {
		// Use the same permutevar8x32 but make two indices corresponding to a double
		alignas(32) uint32_t indices[sizeof(__m256i) / sizeof(uint32_t)];
		for (unsigned int index = 0; index < 4; index++) {
			unsigned int first = index * 2;
			unsigned int second = index * 2 + 1;

			if constexpr (right) {
				unsigned int first_offset = first + count * 2;
				unsigned int second_offset = first_offset + 1;
				indices[first] = first_offset >= 8 ? first_offset - 8 : first_offset;
				indices[second] = second_offset >= 8 ? second_offset - 8 : second_offset;
			}
			else {
				int first_offset = (int)first - count * 2;
				int second_offset = first_offset + 1;
				indices[first] = first_offset < 0 ? first_offset + 8 : first_offset;
				indices[second] = second_offset < 0 ? second_offset + 8 : second_offset;
			}
		}

		__m256i vector_indices = _mm256_load_si256((const __m256i*)indices);
		return CastToDouble(_mm256_permutevar8x32_ps(CastToFloat(vector), vector_indices));
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate4(__m256i vector, unsigned int count) {
		return CastToInteger(Rotate<right>(CastToDouble(vector), count));
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate8(__m256i vector, unsigned int count) {
		return CastToInteger(Rotate<right>(CastToFloat(vector), count));
	}

	// If the count is even, it can be permuted like 32 bit values using _mm256_permutevar8x32_epi32
	template<bool right>
	__m256i ECS_VECTORCALL Rotate16Even(__m256i vector, unsigned int count) {
		return Rotate8<right>(vector, count >> 1);
	}

	// If the count is odd, the values can be bit shifted inside the 32 bit values and then blended together
	template<bool right>
	__m256i ECS_VECTORCALL Rotate16Once(__m256i vector) {
		if constexpr (right) {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 16);
			__m256i left_shift = _mm256_slli_epi64(vector, 48);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_left_shift = _mm256_permute4x64_epi64(left_shift, (0x03 << 0) | (0x00 << 2) | (0x01 << 4) | (0x02 << 6));
			// Blend the values now - the mask is provided for a single 128 bit lane but it is used for both
			return _mm256_blend_epi16(right_shift, permuted_left_shift, (1 << 0) | (1 << 4));
		}
		else {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 48);
			__m256i left_shift = _mm256_slli_epi64(vector, 16);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_right_shift = _mm256_permute4x64_epi64(left_shift, (0x01 << 0) | (0x02 << 2) | (0x03 << 4) | (0x00 << 6));
			// Blend the values now - the mask is provided for a single 128 bit lane but it is used for both
			return _mm256_blend_epi16(left_shift, permuted_right_shift, (1 << 3) | (1 << 7));
		}
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate16(__m256i vector, unsigned int count) {
		// Eliminate the last bit
		unsigned int even_count = count & (UINT_MAX - 1);
		if (even_count > 0) {
			vector = Rotate16Even<right>(even_count);
		}
		// If odd, rotate once
		if (count & 1){
			vector = Rotate16Once<right>(vector);
		}

		return vector;
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate32MultipleOf4(__m256i vector, unsigned int count) {
		return Rotate8<right>(vector, count >> 2);
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate32Once(__m256i vector) {
		if constexpr (right) {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 8);
			__m256i left_shift = _mm256_slli_epi64(vector, 56);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_left_shift = _mm256_permute4x64_epi64(left_shift, (0x03 << 0) | (0x00 << 2) | (0x01 << 4) | (0x02 << 6));

			// Create the mask for blendv
			alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)] = { 0 };
			mask[0] = UCHAR_MAX;
			mask[4] = UCHAR_MAX;
			mask[8] = UCHAR_MAX;
			mask[12] = UCHAR_MAX;
			mask[16] = UCHAR_MAX;
			mask[20] = UCHAR_MAX;
			mask[24] = UCHAR_MAX;
			mask[28] = UCHAR_MAX;

			__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);

			// Blend the values now
			return _mm256_blendv_epi8(right_shift, permuted_left_shift, vector_mask);
		}
		else {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 56);
			__m256i left_shift = _mm256_slli_epi64(vector, 8);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_right_shift = _mm256_permute4x64_epi64(left_shift, (0x01 << 0) | (0x02 << 2) | (0x03 << 4) | (0x00 << 6));

			// Create the mask for blendv
			alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)] = { 0 };
			mask[3] = UCHAR_MAX;
			mask[7] = UCHAR_MAX;
			mask[11] = UCHAR_MAX;
			mask[15] = UCHAR_MAX;
			mask[19] = UCHAR_MAX;
			mask[23] = UCHAR_MAX;
			mask[27] = UCHAR_MAX;
			mask[31] = UCHAR_MAX;

			__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);

			// Blend the values now
			return _mm256_blend_epi16(left_shift, permuted_right_shift, vector_mask);
		}
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate32Twice(__m256i vector) {
		if constexpr (right) {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 16);
			__m256i left_shift = _mm256_slli_epi64(vector, 48);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_left_shift = _mm256_permute4x64_epi64(left_shift, (0x03 << 0) | (0x00 << 2) | (0x01 << 4) | (0x02 << 6));

			// Create the mask for blendv
			alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)] = { 0 };
			
#define LOOP_ITERATION(index) mask[(index) * 4] = UCHAR_MAX; mask[(index) * 4 + 1] = UCHAR_MAX;

			LOOP_UNROLL(8, LOOP_ITERATION);

#undef LOOP_ITERATION

			__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);

			// Blend the values now
			return _mm256_blendv_epi8(right_shift, permuted_left_shift, vector_mask);
		}
		else {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 48);
			__m256i left_shift = _mm256_slli_epi64(vector, 16);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_right_shift = _mm256_permute4x64_epi64(left_shift, (0x01 << 0) | (0x02 << 2) | (0x03 << 4) | (0x00 << 6));

			// Create the mask for blendv
			alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)] = { 0 };

#define LOOP_ITERATION(index) mask[(index) * 4 + 2] = UCHAR_MAX; mask[(index) * 4 + 3] = UCHAR_MAX;

			LOOP_UNROLL(8, LOOP_ITERATION);

#undef LOOP_ITERATION

			__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);

			// Blend the values now
			return _mm256_blend_epi16(left_shift, permuted_right_shift, vector_mask);
		}
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate32Thrice(__m256i vector) {
		if constexpr (right) {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 24);
			__m256i left_shift = _mm256_slli_epi64(vector, 40);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_left_shift = _mm256_permute4x64_epi64(left_shift, (0x03 << 0) | (0x00 << 2) | (0x01 << 4) | (0x02 << 6));

			// Create the mask for blendv
			alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)] = { 1 };

#define LOOP_ITERATION(index) mask[(index) * 4 + 3] = 0;

			LOOP_UNROLL(8, LOOP_ITERATION);

#undef LOOP_ITERATION

			__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);

			// Blend the values now
			return _mm256_blendv_epi8(right_shift, permuted_left_shift, vector_mask);
		}
		else {
			// Shift once to the right inside the 64 bit values
			__m256i right_shift = _mm256_srai_epi64(vector, 40);
			__m256i left_shift = _mm256_slli_epi64(vector, 24);

			// permute the left shift to get the elements in the correct order
			__m256i permuted_right_shift = _mm256_permute4x64_epi64(left_shift, (0x01 << 0) | (0x02 << 2) | (0x03 << 4) | (0x00 << 6));

			// Create the mask for blendv
			alignas(32) uint8_t mask[sizeof(__m256i) / sizeof(uint8_t)] = { 1 };

#define LOOP_ITERATION(index) mask[(index) * 4] = 0;

			LOOP_UNROLL(8, LOOP_ITERATION);

#undef LOOP_ITERATION

			__m256i vector_mask = _mm256_load_si256((const __m256i*)mask);

			// Blend the values now
			return _mm256_blend_epi16(left_shift, permuted_right_shift, vector_mask);
		}
	}

	template<bool right>
	__m256i ECS_VECTORCALL Rotate32(__m256i vector, unsigned int count) {
		unsigned int multiple_of_four = count & (~0x3);
		if (multiple_of_four > 0) {
			vector = Rotate32MultipleOf4(even_count);
		}

		unsigned int remainder = count & 0x3;
		if (remainder == 1) {
			vector = Rotate32Once<right>(vector);
		}
		else if (remainder == 2) {
			vector = Rotate32Twice<right>(vector);
		}
		else if (remainder == 3) {
			vector = Rotate32Thrice<right>(vector);
		}

		return vector;
	}

#pragma endregion

}