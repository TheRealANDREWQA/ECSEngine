#pragma once
#include "ecspch.h"
#include "../Core.h"

namespace ECSEngine {

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

#pragma region Precull horizontal find first

	// Use horizontal or to precull the find first search
	// Horizontal find first uses some SIMD and scalar instructions
	// while horizontal or is mostly a single SIMD instruction
	template<typename VectorType>
	void ECS_VECTORCALL precull_horizontal_find_first(VectorType vector, int& flag) {
		if (horizontal_or(vector)) {
			flag = horizontal_find_first(vector);
		}
	}

#pragma endregion

}