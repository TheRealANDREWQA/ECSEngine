#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "ecspch.h"
#include "VCLExtensions.h"
#include "Vector.h"
#include <math.h>
#include "../Utilities/Assert.h"

#define ECS_MATRIX_EPSILON 0.001f

namespace ECSEngine {

	struct Matrix;

	Matrix ECS_VECTORCALL MatrixMultiply(Matrix a, Matrix b);

	struct Matrix {
		ECS_INLINE Matrix() {}

		ECS_INLINE Matrix(float x00, float x01, float x02, float x03,
			float x10, float x11, float x12, float x13,
			float x20, float x21, float x22, float x23,
			float x30, float x31, float x32, float x33)
		{
			v[0] = Vec8f(x00, x01, x02, x03, x10, x11, x12, x13);
			v[1] = Vec8f(x20, x21, x22, x23, x30, x31, x32, x33);
		}

		ECS_INLINE Matrix(const float* values) {
			v[0].Load(values);
			v[1].Load(values + 8);
		}

		ECS_INLINE Matrix(const float4& row0, const float4& row1, const float4& row2, const float4& row3) {
			Vec4f first_row = Vec4f().load((const float*)&row0);
			Vec4f second_row = Vec4f().load((const float*)&row1);
			Vec4f third_row = Vec4f().load((const float*)&row2);
			Vec4f fourth_row = Vec4f().load((const float*)&row3);

			v[0] = Vec8f(first_row, second_row);
			v[1] = Vec8f(third_row, fourth_row);
		}

		ECS_INLINE Matrix(Vector4 row0, Vector4 row1, Vector4 row2, Vector4 row3) {
			v[0] = Vector8(row0, row1);
			v[1] = Vector8(row2, row3);
		}

		ECS_INLINE Matrix(Vector8 _v1, Vector8 _v2) {
			v[0] = _v1;
			v[1] = _v2;
		}

		ECS_INLINE ECS_VECTORCALL Matrix(const Matrix& other) = default;
		ECS_INLINE Matrix& ECS_VECTORCALL operator = (const Matrix& other) = default;

		ECS_INLINE bool ECS_VECTORCALL operator == (Matrix other) {
			Vec8f epsilon(ECS_MATRIX_EPSILON);
			return horizontal_and(abs(v[0] - other.v[0]) < epsilon && abs(v[1] - other.v[1]) < epsilon);
		}

		ECS_INLINE bool ECS_VECTORCALL operator != (Matrix other) {
			return !(*this == other);
		}

		ECS_INLINE Matrix ECS_VECTORCALL operator + (Matrix other) const {
			return Matrix(v[0] + other.v[0], v[1] + other.v[1]);
		}

		ECS_INLINE Matrix ECS_VECTORCALL operator - (Matrix other) const {
			return Matrix(v[0] - other.v[0], v[1] - other.v[1]);
		}

		ECS_INLINE Matrix operator * (float value) const {
			Vector8 vector_value = Vector8(value);
			return Matrix(v[0] * vector_value, v[1] * vector_value);
		}

		ECS_INLINE Matrix operator / (float value) const {
			Vector8 vector_value = Vector8(1.0f / value);
			return Matrix(v[0] * vector_value, v[1] * vector_value);
		}

		ECS_INLINE Matrix ECS_VECTORCALL operator * (Matrix other) const {
			return MatrixMultiply(*this, other);
		}

		ECS_INLINE Matrix& ECS_VECTORCALL operator += (Matrix other) {
			v[0] += other.v[0];
			v[1] += other.v[1];
			return *this;
		}

		ECS_INLINE Matrix& ECS_VECTORCALL operator -= (Matrix other) {
			v[0] -= other.v[0];
			v[1] -= other.v[1];
			return *this;
		}

		ECS_INLINE Matrix& operator *= (float value) {
			Vector8 vector_value = Vector8(value);
			v[0] *= vector_value;
			v[1] *= vector_value;
			return *this;
		}

		ECS_INLINE Matrix& operator /= (float value) {
			Vector8 vector_value = Vector8(1.0f / value);
			v[0] *= vector_value;
			v[1] *= vector_value;

			return *this;
		}

		ECS_INLINE Matrix& Load(const void* values) {
			const float* float_values = (const float*)values;
			v[0].Load(float_values);
			v[1].Load(float_values + 8);
			return *this;
		}

		ECS_INLINE Matrix& LoadAligned(const void* values) {
			const float* float_values = (const float*)values;
			v[0].LoadAligned(float_values);
			v[1].LoadAligned(float_values + 8);
			return *this;
		}

		ECS_INLINE void Store(void* values) const {
			float* float_values = (float*)values;
			v[0].Store(float_values);
			v[1].Store(float_values + 8);
		}

		ECS_INLINE void StoreAligned(void* values) const {
			float* float_values = (float*)values;
			v[0].StoreAligned(float_values);
			v[1].StoreAligned(float_values + 8);
		}

		ECS_INLINE void StoreStreamed(void* values) const {
			float* float_values = (float*)values;
			v[0].StoreStreamed(float_values);
			v[1].StoreStreamed(float_values + 8);
		}

		Vector8 v[2];
	};

	// -----------------------------------------------------------------------------------------------------

	ECS_INLINE void ScalarMatrixMultiply(const float* ECS_RESTRICT a, const float* ECS_RESTRICT b, float* ECS_RESTRICT destination) {
		for (size_t index = 0; index < 4; index++) {
			for (size_t column_index = 0; column_index < 4; column_index++) {
				float current_value = 0.0f;
				for (size_t subindex = 0; subindex < 4; subindex++) {
					current_value += a[index * 4 + subindex] * b[subindex * 4 + column_index];
				}
				destination[index * 4 + column_index] = current_value;
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vec8f ECS_VECTORCALL MatrixTwoRowProduct(Vec8f first_matrix_two_rows, Vec8f second_matrix_v0, Vec8f second_matrix_v1) {
		Vec8f a00a10 = permute8<0, 0, 0, 0, 4, 4, 4, 4>(first_matrix_two_rows);
		Vec8f b_first_row_splatted = permute8<0, 1, 2, 3, 0, 1, 2, 3>(second_matrix_v0);

		Vec8f a01a11 = permute8<1, 1, 1, 1, 5, 5, 5, 5>(first_matrix_two_rows);
		Vec8f b_second_row_splatted = permute8<4, 5, 6, 7, 4, 5, 6, 7>(second_matrix_v0);

		Vec8f a02a12 = permute8<2, 2, 2, 2, 6, 6, 6, 6>(first_matrix_two_rows);
		Vec8f b_third_row_splatted = permute8<0, 1, 2, 3, 0, 1, 2, 3>(second_matrix_v1);

		Vec8f a03a13 = permute8<3, 3, 3, 3, 7, 7, 7, 7>(first_matrix_two_rows);
		Vec8f b_fourth_row_splatted = permute8<4, 5, 6, 7, 4, 5, 6, 7>(second_matrix_v1);

		Vec8f intermediate = a00a10 * b_first_row_splatted;

		// ------------------------------------------- splitting multiplications and additions -----------------------------
		/*Vec8f mul_intermediate = _mm256_mul_ps(a01a11, b_second_row_splatted);
		intermediate = _mm256_add_ps(intermediate, mul_intermediate);

		mul_intermediate = _mm256_mul_ps(a02a12, b_third_row_splatted);
		intermediate = _mm256_add_ps(intermediate, mul_intermediate);

		mul_intermediate = _mm256_mul_ps(a03a13, b_fourth_row_splatted);
		return _mm256_add_ps(intermediate, mul_intermediate);*/
		// ------------------------------------------- splitting multiplications and additions ----------------------------

		// ----------------------------------------------------- fmadd version --------------------------------------------
		intermediate = Fmadd(a01a11, b_second_row_splatted, intermediate);
		intermediate = Fmadd(a02a12, b_third_row_splatted, intermediate);
		return Fmadd(a03a13, b_fourth_row_splatted, intermediate);
		// ----------------------------------------------------- fmadd version --------------------------------------------
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixMultiply(Matrix a, Matrix b) {
		return Matrix(MatrixTwoRowProduct(a.v[0], b.v[0], b.v[1]), MatrixTwoRowProduct(a.v[1], b.v[0], b.v[1]));
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixTranspose(Matrix matrix) {
		Vec8f first, second;
		// 4 registers needed to keep the indices for blending
		first = blend8<0, 4, 8, 12, 1, 5, 9, 13>(matrix.v[0], matrix.v[1]);
		second = blend8<2, 6, 10, 14, 3, 7, 11, 15>(matrix.v[0], matrix.v[1]);
		return Matrix(first, second);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixIdentity() {
		return Matrix(VectorGlobals::RIGHT_4, VectorGlobals::UP_4, VectorGlobals::FORWARD_4, VectorGlobals::QUATERNION_IDENTITY_4);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixNull() {
		Vector8 zero = ZeroVector8();
		return Matrix(zero, zero);
	}

	// --------------------------------------------------------------------------------------------------------------

	//Vec4f ECS_INLINE ECS_VECTORCALL MatrixVec4fDeterminant(Matrix matrix) {
	//	Vec4f second_row = matrix.v[0].get_high();
	//	Vec4f third_row = matrix.v[1].get_low();
	//	Vec4f fourth_row = matrix.v[1].get_high();

	//	Vec4f positive_parts_top1 = permute4<2, 2, 1, 1>(third_row);
	//	Vec4f positive_parts_top2 = permute4<1, 0, 0, 0>(third_row);
	//	Vec4f positive_parts_top3 = positive_parts_top2;

	//	Vec4f positive_parts_bottom1 = permute4<3, 3, 3, 2>(fourth_row);
	//	Vec4f positive_parts_bottom2 = positive_parts_bottom1;
	//	Vec4f positive_parts_bottom3 = permute4<2, 2, 1, 1>(fourth_row);

	//	Vec4f positive_parts1 = _mm_mul_ps(positive_parts_top1, positive_parts_bottom1);
	//	Vec4f positive_parts2 = _mm_mul_ps(positive_parts_top2, positive_parts_bottom2);
	//	Vec4f positive_parts3 = _mm_mul_ps(positive_parts_top3, positive_parts_bottom3);

	//	Vec4f negative_parts_top1 = permute4<3, 3, 3, 2>(third_row);
	//	Vec4f negative_parts_top2 = negative_parts_top1;
	//	Vec4f negative_parts_top3 = permute4<2, 2, 1, 1>(third_row);

	//	Vec4f negative_parts_bottom1 = permute4<2, 2, 1, 1>(fourth_row);
	//	Vec4f negative_parts_bottom2 = permute4<1, 0, 0, 0>(fourth_row);
	//	Vec4f negative_parts_bottom3 = negative_parts_bottom2;
	//	
	//	Vec4f negative_parts1 = negative_parts_top1 * negative_parts_bottom1;
	//	Vec4f negative_parts2 = negative_parts_top2 * negative_parts_bottom2;
	//	Vec4f negative_parts3 = negative_parts_top3 * negative_parts_bottom3;

	//	Vec4f determinants_2x2_unfactored1 = _mm_sub_ps(positive_parts1, negative_parts1);
	//	Vec4f determinants_2x2_unfactored2 = _mm_sub_ps(positive_parts2, negative_parts2);
	//	Vec4f determinants_2x2_unfactored3 = _mm_sub_ps(positive_parts3, negative_parts3);

	//	Vec4f determinants_factors1 = permute4<1, 0, 0, 0>(second_row);
	//	Vec4f determinants_factors2 = permute4<2, 2, 1, 1>(second_row);
	//	Vec4f determinants_factors3 = permute4<3, 3, 3, 2>(second_row);

	//	Vec4f determinants_2x2_factored1 = _mm_mul_ps(determinants_2x2_unfactored1, determinants_factors1);
	//	Vec4f determinants_2x2_factored2 = _mm_mul_ps(determinants_2x2_unfactored2, determinants_factors2);
	//	Vec4f determinants_2x2_factored3 = _mm_mul_ps(determinants_2x2_unfactored3, determinants_factors3);

	//	Vec4f determinants_3x3 = _mm_sub_ps(_mm_add_ps(determinants_2x2_factored1, determinants_2x2_factored3), determinants_2x2_factored2);

	//	Vec4f initial_4_factors = matrix.v[0].get_low();
	//	initial_4_factors = change_sign<0, 1, 0, 1>(initial_4_factors);

	//	return _mm_dp_ps(determinants_3x3, initial_4_factors, 0xFF);
	//	//return horizontal_add(determinants_3x3);
	//}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vec4f ECS_VECTORCALL MatrixVec4fDeterminant(Matrix matrix) {
		Vec8f positive_parts_top = permute8<2, 1, 1, 0, 0, 0, V_DC, V_DC>(matrix.v[1]);

		Vec8f positive_parts_bottom = permute8<7, 7, 6, 7, 6, 5, V_DC, V_DC>(matrix.v[1]);

		Vec8f positive_parts = positive_parts_top * positive_parts_bottom;

		Vec8f negative_parts_top = permute8<3, 3, 2, 3, 2, 1, V_DC, V_DC>(matrix.v[1]);

		Vec8f negative_parts_bottom = permute8<6, 5, 5, 4, 4, 4, V_DC, V_DC>(matrix.v[1]);

		Vec8f negative_parts = negative_parts_top * negative_parts_bottom;

		Vec8f determinants_2x2_unfactored = positive_parts - negative_parts;

		Vec4f determinants_2x2_unfactored1 = permute8<0, 0, 1, 2, V_DC, V_DC, V_DC, V_DC>(determinants_2x2_unfactored).get_low();
		Vec4f determinants_2x2_unfactored2 = permute8<1, 3, 3, 4, V_DC, V_DC, V_DC, V_DC>(determinants_2x2_unfactored).get_low();
		Vec4f determinants_2x2_unfactored3 = permute8<2, 4, 5, 5, V_DC, V_DC, V_DC, V_DC>(determinants_2x2_unfactored).get_low();

		Vec4f second_row = matrix.v[0].value.get_high();
		Vec4f determinants_factors1 = permute4<1, 0, 0, 0>(second_row);
		Vec4f determinants_factors2 = permute4<2, 2, 1, 1>(second_row);
		Vec4f determinants_factors3 = permute4<3, 3, 3, 2>(second_row);

		Vec4f determinants_2x2_factored1 = determinants_2x2_unfactored1 * determinants_factors1;
		Vec4f determinants_2x2_factored2 = determinants_2x2_unfactored2 * determinants_factors2;
		Vec4f determinants_2x2_factored3 = determinants_2x2_unfactored3 * determinants_factors3;

		Vec4f determinants_3x3 = determinants_2x2_factored1 - determinants_2x2_factored2 + determinants_2x2_factored3;

		Vec4f initial_4_factors = matrix.v[0].value.get_low();
		initial_4_factors = change_sign<0, 1, 0, 1>(initial_4_factors);

		return _mm_dp_ps(determinants_3x3, initial_4_factors, 0xFF);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float ECS_VECTORCALL MatrixDeterminant(Matrix matrix) {
		return _mm_cvtss_f32(MatrixVec4fDeterminant(matrix));
	}

	// --------------------------------------------------------------------------------------------------------------

	// Register pressure present, many memory swaps are needed to keep permute indices into registers
	ECS_INLINE Matrix ECS_VECTORCALL MatrixInverse(Matrix matrix) {
		Vec8f positive_parts_top0 = permute8<2, 1, 1, 0, 0, 0, V_DC, V_DC>(matrix.v[0]);
		Vec8f positive_parts_top1 = permute8<2, 1, 1, 0, 0, 0, V_DC, V_DC>(matrix.v[1]);

		Vec8f positive_parts_bottom0 = permute8<7, 7, 6, 7, 6, 5, V_DC, V_DC>(matrix.v[0]);
		Vec8f positive_parts_bottom1 = permute8<7, 7, 6, 7, 6, 5, V_DC, V_DC>(matrix.v[1]);

		Vec8f positive_parts0 = positive_parts_top0 * positive_parts_bottom0;
		Vec8f positive_parts1 = positive_parts_top1 * positive_parts_bottom1;

		Vec8f negative_parts_top0 = permute8<3, 3, 2, 3, 2, 1, V_DC, V_DC>(matrix.v[0]);
		Vec8f negative_parts_top1 = permute8<3, 3, 2, 3, 2, 1, V_DC, V_DC>(matrix.v[1]);

		Vec8f negative_parts_bottom0 = permute8<6, 5, 5, 4, 4, 4, V_DC, V_DC>(matrix.v[0]);
		Vec8f negative_parts_bottom1 = permute8<6, 5, 5, 4, 4, 4, V_DC, V_DC>(matrix.v[1]);

		Vec8f negative_parts0 = negative_parts_top0 * negative_parts_bottom0;
		Vec8f negative_parts1 = negative_parts_top1 * negative_parts_bottom1;

		Vec8f determinants_2x2_unfactored0 = positive_parts0 - negative_parts0;
		Vec8f determinants_2x2_unfactored1 = positive_parts1 - negative_parts1;

		Vec8f determinants_2x2_unfactored_vertical_1 = permute8<0, 0, 1, 2, 0, 0, 1, 2>(determinants_2x2_unfactored0);
		Vec8f determinants_2x2_unfactored_vertical_2 = permute8<1, 3, 3, 4, 1, 3, 3, 4>(determinants_2x2_unfactored0);
		Vec8f determinants_2x2_unfactored_vertical_3 = permute8<2, 4, 5, 5, 2, 4, 5, 5>(determinants_2x2_unfactored0);
		Vec8f determinants_2x2_unfactored_vertical_4 = permute8<0, 0, 1, 2, 0, 0, 1, 2>(determinants_2x2_unfactored1);
		Vec8f determinants_2x2_unfactored_vertical_5 = permute8<1, 3, 3, 4, 1, 3, 3, 4>(determinants_2x2_unfactored1);
		Vec8f determinants_2x2_unfactored_vertical_6 = permute8<2, 4, 5, 5, 2, 4, 5, 5>(determinants_2x2_unfactored1);

		// the vectors that they come from must be flipped: first three with 1 and next three with 0; 
		// the upper two rows of determinants need factors from the second vector meanwhile the lower 
		// two rows of determinants need factors from the first vector
		/*Vec8f determinants_factors1 = permute8<5, 4, 4, 4, 1, 0, 0, 0>(matrix.v[1]);
		Vec8f determinants_factors2 = permute8<6, 6, 5, 5, 2, 2, 1, 1>(matrix.v[1]);
		Vec8f determinants_factors3 = permute8<7, 7, 7, 6, 3, 3, 3, 2>(matrix.v[1]);
		Vec8f determinants_factors4 = permute8<5, 4, 4, 4, 1, 0, 0, 0>(matrix.v[0]);
		Vec8f determinants_factors5 = permute8<6, 6, 5, 5, 2, 2, 1, 1>(matrix.v[0]);
		Vec8f determinants_factors6 = permute8<7, 7, 7, 6, 3, 3, 3, 2>(matrix.v[0]);*/

		Vec8f v0 = matrix.v[0];
		Vec8f v1 = matrix.v[1];
		Vec8f v0_lanes_switched = Permute2f128<0x01>(matrix.v[0], matrix.v[0]);
		Vec8f v1_lanes_switched = Permute2f128<0x01>(matrix.v[1], matrix.v[1]);

		/*Vec8f determinants_factors1 = permute8<1, 0, 0, 0, 5, 4, 4, 4>(v1_lanes_switched);
		Vec8f determinants_factors2 = permute8<2, 2, 1, 1, 6, 6, 5, 5>(v1_lanes_switched);
		Vec8f determinants_factors3 = permute8<3, 3, 3, 2, 7, 7, 7, 6>(v1_lanes_switched);
		Vec8f determinants_factors4 = permute8<1, 0, 0, 0, 5, 4, 4, 4>(v0_lanes_switched);
		Vec8f determinants_factors5 = permute8<2, 2, 1, 1, 6, 6, 5, 5>(v0_lanes_switched);
		Vec8f determinants_factors6 = permute8<3, 3, 3, 2, 7, 7, 7, 6>(v0_lanes_switched);*/
		Vec8f determinants_factors1 = _mm256_permute_ps(v1_lanes_switched, _MM_SHUFFLE(0, 0, 0, 1));
		Vec8f determinants_factors2 = _mm256_permute_ps(v1_lanes_switched, _MM_SHUFFLE(1, 1, 2, 2));
		Vec8f determinants_factors3 = _mm256_permute_ps(v1_lanes_switched, _MM_SHUFFLE(2, 3, 3, 3));
		Vec8f determinants_factors4 = _mm256_permute_ps(v0_lanes_switched, _MM_SHUFFLE(0, 0, 0, 1));
		Vec8f determinants_factors5 = _mm256_permute_ps(v0_lanes_switched, _MM_SHUFFLE(1, 1, 2, 2));
		Vec8f determinants_factors6 = _mm256_permute_ps(v0_lanes_switched, _MM_SHUFFLE(2, 3, 3, 3));

		Vec8f determinants_2x2_factored1 = determinants_2x2_unfactored_vertical_1 * determinants_factors1;
		Vec8f determinants_2x2_factored2 = determinants_2x2_unfactored_vertical_2 * determinants_factors2;
		Vec8f determinants_2x2_factored3 = determinants_2x2_unfactored_vertical_3 * determinants_factors3;
		Vec8f determinants_2x2_factored4 = determinants_2x2_unfactored_vertical_4 * determinants_factors4;
		Vec8f determinants_2x2_factored5 = determinants_2x2_unfactored_vertical_5 * determinants_factors5;
		Vec8f determinants_2x2_factored6 = determinants_2x2_unfactored_vertical_6 * determinants_factors6;

		// must be swapped again
		Vec8f determinants_3x3_1 = determinants_2x2_factored4 - determinants_2x2_factored5 + determinants_2x2_factored6;
		Vec8f determinants_3x3_2 = determinants_2x2_factored1 - determinants_2x2_factored2 + determinants_2x2_factored3;

		//float determinant_xd = MatrixDeterminantInlined(matrix);

		// in order to change the sign of the 3x3 determinants
		Vec8i determinant_mask_int = Vec8i(0, 0x80000000, 0, 0x80000000, 0x80000000, 0, 0x80000000, 0);
		Vec8f determinant_mask = _mm256_castsi256_ps(determinant_mask_int);

		Vec4f four_3x3_determinants = determinants_3x3_1.get_low();
		Vec4f first_row = matrix.v[0].value.get_low();
		first_row ^= determinant_mask.get_low();

		Vec4f determinant3x3_for_whole_determinant = _mm_dp_ps(first_row, four_3x3_determinants, 0xFF);
		// determinant bias; values then to skew towards higher values when the determinant gets very small
		// so substract a very small value in order to balance errors
		determinant3x3_for_whole_determinant -= _mm_set1_ps(0.0000003243f);

		// for more precision choose 1.0f / determinant3x3_for_whole_determinant
		//Vec4f inverse_determinant_value = approx_recipr(determinant3x3_for_whole_determinant);
		Vec4f inverse_determinant_value = 1.0f / determinant3x3_for_whole_determinant;
		Vec8f inverse_determinant_value_8 = Vec8f(inverse_determinant_value, inverse_determinant_value);

		determinants_3x3_1 ^= determinant_mask;
		determinants_3x3_2 ^= determinant_mask;

		Matrix transposed_adjugate = MatrixTranspose(Matrix(determinants_3x3_1, determinants_3x3_2));
		transposed_adjugate.v[0] *= inverse_determinant_value_8;
		transposed_adjugate.v[1] *= inverse_determinant_value_8;
		return transposed_adjugate;
	}

	// --------------------------------------------------------------------------------------------------------------

	// Will use doubles for calculations and will convert at the end in floats
	ECS_INLINE Matrix ECS_VECTORCALL MatrixInversePrecise(Matrix matrix) {
		Vec4d first_row = to_double(matrix.v[0].value.get_low());
		Vec4d second_row = to_double(matrix.v[0].value.get_high());
		Vec4d third_row = to_double(matrix.v[1].value.get_low());
		Vec4d fourth_row = to_double(matrix.v[1].value.get_high());

		Vec4d positive_parts_top0 = permute4<2, 1, 1, 0>(first_row);
		Vec4d positive_parts_top1 = permute4<2, 1, 1, 0>(third_row);
		Vec4d positive_parts_top2 = blend4<0, 0, 4, 4>(first_row, third_row);

		Vec4d positive_parts_bottom0 = permute4<3, 3, 2, 3>(second_row);
		Vec4d positive_parts_bottom1 = permute4<3, 3, 2, 3>(fourth_row);
		Vec4d positive_parts_bottom2 = blend4<2, 1, 6, 5>(second_row, fourth_row);

		Vec4d positive_parts0 = positive_parts_top0 * positive_parts_bottom0;
		Vec4d positive_parts1 = positive_parts_top1 * positive_parts_bottom1;
		Vec4d positive_parts2 = positive_parts_top2 * positive_parts_bottom2;

		Vec4d negative_parts_top0 = permute4<3, 3, 2, 3>(first_row);
		Vec4d negative_parts_top1 = permute4<3, 3, 2, 3>(third_row);
		Vec4d negative_parts_top2 = blend4<2, 1, 6, 5>(first_row, third_row);

		Vec4d negative_parts_bottom0 = permute4<2, 1, 1, 0>(second_row);
		Vec4d negative_parts_bottom1 = permute4<2, 1, 1, 0>(fourth_row);
		Vec4d negative_parts_bottom2 = blend4<0, 0, 4, 4>(second_row, fourth_row);

		Vec4d negative_parts0 = negative_parts_top0 * negative_parts_bottom0;
		Vec4d negative_parts1 = negative_parts_top1 * negative_parts_bottom1;
		Vec4d negative_parts2 = negative_parts_top2 * negative_parts_bottom2;

		Vec4d determinants_2x2_unfactored0 = positive_parts0 - negative_parts0;
		Vec4d determinants_2x2_unfactored1 = positive_parts1 - negative_parts1;
		Vec4d determinants_2x2_unfactored2 = positive_parts2 - negative_parts2;

		Vec4d determinants_2x2_unfactored_vertical_1_ = permute4<0, 0, 1, 2>(determinants_2x2_unfactored0);
		Vec4d determinants_2x2_unfactored_vertical_2_ = blend4<1, 3, 3, 4>(determinants_2x2_unfactored0, determinants_2x2_unfactored2);
		Vec4d determinants_2x2_unfactored_vertical_3_ = blend4<2, 4, 5, 5>(determinants_2x2_unfactored0, determinants_2x2_unfactored2);
		Vec4d determinants_2x2_unfactored_vertical_4_ = permute4<0, 0, 1, 2>(determinants_2x2_unfactored1);
		Vec4d determinants_2x2_unfactored_vertical_5_ = blend4<1, 3, 3, 6>(determinants_2x2_unfactored1, determinants_2x2_unfactored2);
		Vec4d determinants_2x2_unfactored_vertical_6_ = blend4<2, 6, 7, 7>(determinants_2x2_unfactored1, determinants_2x2_unfactored2);

		// the vectors that they come from must be flipped: first three with 1 and next three with 0; 
		// the upper two rows of determinants need factors from the second vector meanwhile the lower 
		// two rows of determinants need factors from the first vector
		/*Vec8f determinants_factors1 = permute8<5, 4, 4, 4, 1, 0, 0, 0>(matrix.v[1]);
		Vec8f determinants_factors2 = permute8<6, 6, 5, 5, 2, 2, 1, 1>(matrix.v[1]);
		Vec8f determinants_factors3 = permute8<7, 7, 7, 6, 3, 3, 3, 2>(matrix.v[1]);
		Vec8f determinants_factors4 = permute8<5, 4, 4, 4, 1, 0, 0, 0>(matrix.v[0]);
		Vec8f determinants_factors5 = permute8<6, 6, 5, 5, 2, 2, 1, 1>(matrix.v[0]);
		Vec8f determinants_factors6 = permute8<7, 7, 7, 6, 3, 3, 3, 2>(matrix.v[0]);*/

		Vec4d determinants_factors1 = permute4<1, 0, 0, 0>(fourth_row);
		Vec4d determinants_factors2 = permute4<1, 0, 0, 0>(third_row);
		Vec4d determinants_factors3 = permute4<2, 2, 1, 1>(fourth_row);
		Vec4d determinants_factors4 = permute4<2, 2, 1, 1>(third_row);
		Vec4d determinants_factors5 = permute4<3, 3, 3, 2>(fourth_row);
		Vec4d determinants_factors6 = permute4<3, 3, 3, 2>(third_row);
		Vec4d determinants_factors7 = permute4<1, 0, 0, 0>(second_row);
		Vec4d determinants_factors8 = permute4<1, 0, 0, 0>(first_row);
		Vec4d determinants_factors9 = permute4<2, 2, 1, 1>(second_row);
		Vec4d determinants_factors10 = permute4<2, 2, 1, 1>(first_row);
		Vec4d determinants_factors11 = permute4<3, 3, 3, 2>(second_row);
		Vec4d determinants_factors12 = permute4<3, 3, 3, 2>(first_row);

		Vec4d determinants_2x2_factored1_ = determinants_2x2_unfactored_vertical_1_ * determinants_factors1;
		Vec4d determinants_2x2_factored2_ = determinants_2x2_unfactored_vertical_1_ * determinants_factors2;
		Vec4d determinants_2x2_factored3_ = determinants_2x2_unfactored_vertical_2_ * determinants_factors3;
		Vec4d determinants_2x2_factored4_ = determinants_2x2_unfactored_vertical_2_ * determinants_factors4;
		Vec4d determinants_2x2_factored5_ = determinants_2x2_unfactored_vertical_3_ * determinants_factors5;
		Vec4d determinants_2x2_factored6_ = determinants_2x2_unfactored_vertical_3_ * determinants_factors6;
		Vec4d determinants_2x2_factored7_ = determinants_2x2_unfactored_vertical_4_ * determinants_factors7;
		Vec4d determinants_2x2_factored8_ = determinants_2x2_unfactored_vertical_4_ * determinants_factors8;
		Vec4d determinants_2x2_factored9_ = determinants_2x2_unfactored_vertical_5_ * determinants_factors9;
		Vec4d determinants_2x2_factored10_ = determinants_2x2_unfactored_vertical_5_ * determinants_factors10;
		Vec4d determinants_2x2_factored11_ = determinants_2x2_unfactored_vertical_6_ * determinants_factors11;
		Vec4d determinants_2x2_factored12_ = determinants_2x2_unfactored_vertical_6_ * determinants_factors12;

		// must be swapped again
		Vec4d determinants_3x3_1 = determinants_2x2_factored7_ - determinants_2x2_factored9_ + determinants_2x2_factored11_;
		Vec4d determinants_3x3_2 = determinants_2x2_factored8_ - determinants_2x2_factored10_ + determinants_2x2_factored12_;
		Vec4d determinants_3x3_3 = determinants_2x2_factored1_ - determinants_2x2_factored3_ + determinants_2x2_factored5_;
		Vec4d determinants_3x3_4 = determinants_2x2_factored2_ - determinants_2x2_factored4_ + determinants_2x2_factored6_;

		//Matrix matrixu = MatrixInverseInlined(matrix);

		// in order to change the sign of the 3x3 determinants
		Vec4q determinant_mask_int = Vec4q(0, 0x8000000000000000, 0, 0x8000000000000000);
		Vec4d determinant_mask = _mm256_castsi256_pd(determinant_mask_int);

		first_row ^= determinant_mask;

		// xyzw
		Vec4d dot_product_intermediate = first_row * determinants_3x3_1;
		// yzwx
		Vec4d dot_product_intermediate1 = permute4<1, 2, 3, 0>(dot_product_intermediate);
		// zwxy
		Vec4d dot_product_intermediate2 = permute4<2, 3, 0, 1>(dot_product_intermediate);
		// wxyz
		Vec4d dot_product_intermediate3 = permute4<3, 0, 1, 2>(dot_product_intermediate);

		Vec4d determinant3x3_for_whole_determinant = dot_product_intermediate + dot_product_intermediate1 + dot_product_intermediate2 + dot_product_intermediate3;

		//float determinant_xd = MatrixDeterminantInlined(matrix);

		// for more precision choose 1.0f / determinant3x3_for_whole_determinant
		//Vec4f inverse_determinant_value = approx_recipr(determinant3x3_for_whole_determinant);
		Vec4d inverse_determinant_value = 1.0f / determinant3x3_for_whole_determinant;

		determinants_3x3_1 ^= determinant_mask;
		determinants_3x3_3 ^= determinant_mask;
		determinant_mask = permute4<1, 0, 1, 0>(determinant_mask);
		determinants_3x3_2 ^= determinant_mask;
		determinants_3x3_4 ^= determinant_mask;

		determinants_3x3_1 *= inverse_determinant_value;
		determinants_3x3_2 *= inverse_determinant_value;
		determinants_3x3_3 *= inverse_determinant_value;
		determinants_3x3_4 *= inverse_determinant_value;
		Matrix inverse = MatrixTranspose(Matrix(
			Vec8f(to_float(determinants_3x3_1), to_float(determinants_3x3_2)),
			Vec8f(to_float(determinants_3x3_3), to_float(determinants_3x3_4)))
		);
		return inverse;
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixTranslation(float x, float y, float z) {
		Matrix result;
		result.v[0] = Vec8f(VectorGlobals::RIGHT_4, VectorGlobals::UP_4);
		result.v[1] = Vec8f(VectorGlobals::FORWARD_4, Vector4(x, y, z, 1.0f));
		return result;
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixTranslation(float3 translation) {
		return MatrixTranslation(translation.x, translation.y, translation.z);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixScale(float x, float y, float z) {
		Matrix result;

		Vector8 zero = ZeroVector8();
		Vector8 values(x, 0.0f, z, 0.0f, 0.0f, y, 0.0f, 1.0f);

		result.v[0] = blend8<0, 1, 10, 3, 4, 5, 6, 15>(values, zero);
		result.v[1] = blend8<8, 1, 2, 3, 4, 13, 6, 7>(values, zero);

		return result;
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixScale(float3 scale) {
		return MatrixScale(scale.x, scale.y, scale.z);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotationXRad(float angle_radians) {
		Matrix result;

		Vector8 zero = ZeroVector8();
		Vec8f cosine;
		Vector8 sine = sincos(&cosine, Vector8(angle_radians));
		Vector8 sine_cosine = blend8<V_DC, 9, 2, V_DC, V_DC, 5, 14, V_DC>(cosine, sine);
		sine_cosine = change_sign<0, 1, 0, 0, 0, 0, 0, 0>(sine_cosine);

		sine_cosine = blend8<8, 1, 2, 11, 12, 5, 6, 15>(sine_cosine, zero);
		result.v[0] = Vec8f(VectorGlobals::RIGHT_4, sine_cosine.value.get_high());
		result.v[1] = Vec8f(sine_cosine.value.get_low(), VectorGlobals::QUATERNION_IDENTITY_4);

		return result;
	}

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotationX(float angle) {
		angle = DegToRad(angle);
		return MatrixRotationXRad(angle);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotationYRad(float angle_radians) {
		Matrix result;

		Vector8 zero = ZeroVector8();
		Vec8f cosine;
		Vector8 sine = sincos(&cosine, Vector8(angle_radians));

		Vector8 sine_cosine = blend8<0, V_DC, 10, V_DC, 12, V_DC, 6, V_DC>(cosine, sine);
		sine_cosine = change_sign<0, 0, 1, 0, 0, 0, 0, 0>(sine_cosine);
		sine_cosine = blend8<0, 9, 2, 11, 4, 13, 6, 15>(sine_cosine, zero);

		result.v[0] = Vec8f(sine_cosine.value.get_low(), VectorGlobals::UP_4);
		result.v[1] = Vec8f(sine_cosine.value.get_high(), VectorGlobals::QUATERNION_IDENTITY_4);

		return result;
	}

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotationY(float angle) {
		angle = DegToRad(angle);
		return MatrixRotationYRad(angle);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotationZRad(float angle_radians) {
		Matrix result;

		Vector8 angle_vector(angle_radians);
		Vec8f cosine;
		Vector8 sine = sincos(&cosine, angle_vector);
		Vector8 sine_cosine = blend8<0, 9, V_DC, V_DC, 4, 13, V_DC, V_DC>(cosine, sine);
		sine_cosine = change_sign<0, 0, 0, 0, 0, 1, 0, 0>(sine_cosine);

		Vector8 zero = ZeroVector8();
		sine_cosine = blend8<0, 1, 10, 11, 5, 6, 14, 15>(sine_cosine, zero);
		result.v[0] = sine_cosine;
		result.v[1] = Vec8f(VectorGlobals::FORWARD_4, VectorGlobals::QUATERNION_IDENTITY_4);

		return result;
	}

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotationZ(float angle) {
		angle = DegToRad(angle);
		return MatrixRotationZRad(angle);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotation(float3 rotation) {
		return MatrixRotationX(rotation.x) * MatrixRotationY(rotation.y) * MatrixRotationZ(rotation.z);
		//return MatrixRotationZ(rotation.z) * MatrixRotationY(rotation.y) * MatrixRotationX(rotation.x);
	}

	ECS_INLINE Matrix ECS_VECTORCALL MatrixRotationRad(float3 rotation) {
		return MatrixRotationXRad(rotation.x) * MatrixRotationYRad(rotation.y) * MatrixRotationZRad(rotation.z);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixLookTo(Vector4 origin, Vector4 direction, Vector4 up) {
		ECS_ASSERT(!IsInfinite(direction));
		ECS_ASSERT(!IsInfinite(up));

		Vector4 normalized_direction = Normalize(direction);
		Vector4 normalized_right = Normalize(Cross(up, normalized_direction));
		Vector4 inverted_normalized_up = Normalize(-up);

		Vector4 inverted_origin = -origin;
		Vector4 mask_last_element = Vector4(INT_MAX, INT_MAX, INT_MAX, 0);

		Vector4 dot0 = Dot(normalized_direction, inverted_origin);
		Vector4 dot1 = Dot(inverted_normalized_up, inverted_origin);
		Vector4 dot2 = Dot(normalized_right, inverted_origin);

		Matrix matrix(
			Select(mask_last_element, dot0, normalized_right),
			Select(mask_last_element, dot1, inverted_normalized_up),
			Select(mask_last_element, dot2, normalized_direction),
			VectorGlobals::QUATERNION_IDENTITY_4
		);

		return MatrixTranspose(matrix);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixLookAt(Vector4 origin, Vector4 focus_point, Vector4 up) {
		return MatrixLookTo(origin, focus_point - origin, up);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixPerspective(float width, float height, float near_z, float far_z) {
		float two_near_z = near_z + near_z;
		float range = far_z / (far_z - near_z);

		// 2n / width        0           0          0
		//     0        2n / height      0          0
		//     0             0         range        1
		//     0             0    -range * near_z   0
		return Matrix(two_near_z / width, 0.0f, 0.0f, 0.0f, 0.0f, two_near_z / height, 0.0f, 0.0f, 0.0f, 0.0f, range, 1.0f, 0.0f, 0.0f, -range * near_z, 0.0f);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixPerspectiveFOV(float angle_y, float aspect_ratio, float near_z, float far_z) {
		// x_scale = y_scale / aspect_ratio;
		// y_scale = cotangent(angle_y / 2.0f);
		// range = far_z / (far_z - near_z);
		

		// x_scale       0          0            0
		//    0      y_scale        0            0
		//    0         0        range           1
		//    0         0   -range * near_z      0
		
		angle_y = DegToRad(angle_y);
		float tangent = tanf(angle_y * 0.5f);
		float y_scale = 1.0f / tangent;
		float x_scale = y_scale / aspect_ratio;
		float range = far_z / (far_z - near_z);
		return Matrix(x_scale, 0.0f, 0.0f, 0.0f, 0.0f, y_scale, 0.0f, 0.0f, 0.0f, 0.0f, range, 1.0f, 0.0f, 0.0f, -range * near_z, 0.0f);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL MatrixOrthographic(float width, float height, float near_z, float far_z) {
		// 2 / width       0           0          0
		//    0        2 / height      0          0
		//    0            0         range        0
		//    0            0     -range * near_z  1

		float range = 1.0f / (far_z - near_z);
		return Matrix(2.0f / width, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f / height, 0.0f, 0.0f, 0.0f, 0.0f, range, 0.0f, 0.0f, 0.0f, -range * near_z, 1.0f);
	}

	// --------------------------------------------------------------------------------------------------------------

#undef near
#undef far

	ECS_INLINE Matrix ECS_VECTORCALL MatrixFrustum(float left, float right, float top, float bottom, float near, float far) {
		float near2 = 2.0f * near;
		float right_left = right - left;
		float top_bottom = top - bottom;
		float far_near = far - near;
		return Matrix(
			     near2 / right_left,                      0.0f,                         0.0f,                0.0f,
			           0.0f,                        near2 / top_bottom,                 0.0f,                0.0f,
			(right + left) / right_left,      (top + bottom) / top_bottom,    -(far + near) / far_near,     -1.0f,
			           0.0f,                              0.0f,               (-far * near2) / far_near,     0.0f
		);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL MatrixVectorMultiply(Vector4 vector, Matrix matrix) {
		Vector4 x_splatted = permute4<0, 0, 0, 0>(vector);
		Vector4 y_splatted = permute4<1, 1, 1, 1>(vector);
		Vector4 z_splatted = permute4<2, 2, 2, 2>(vector);
		Vector4 w_splatted = permute4<3, 3, 3, 3>(vector);

		Vector8 xy_splatted(x_splatted, y_splatted);
		Vector8 zw_splatted(z_splatted, w_splatted);

		Vector8 rows01 = xy_splatted * matrix.v[0];
		Vector8 rows23 = zw_splatted * matrix.v[1];

		Vector8 half_sum = rows01 + rows23;
		Vector8 other_half = permute8<4, 5, 6, 7, V_DC, V_DC, V_DC, V_DC>(half_sum);

		Vector8 result = half_sum + other_half;
		return result.Low();
	}

	// Applies the same matrix to low and upper
	ECS_INLINE Vector8 ECS_VECTORCALL MatrixVectorMultiply(Vector8 vector, Matrix matrix) {
		Vector8 x_splatted = permute8<0, 0, 0, 0, 4, 4, 4, 4>(vector);
		Vector8 y_splatted = permute8<1, 1, 1, 1, 5, 5, 5, 5>(vector);
		Vector8 z_splatted = permute8<2, 2, 2, 2, 6, 6, 6, 6>(vector);
		Vector8 w_splatted = permute8<3, 3, 3, 3, 7, 7, 7, 7>(vector);

		Vector8 matrix_row0 = Permute2f128<0x00>(matrix.v[0], matrix.v[0]);
		Vector8 matrix_row1 = Permute2f128<0x11>(matrix.v[0], matrix.v[0]);
		Vector8 matrix_row2 = Permute2f128<0x00>(matrix.v[1], matrix.v[1]);
		Vector8 matrix_row3 = Permute2f128<0x11>(matrix.v[1], matrix.v[1]);

		Vector8 row0 = x_splatted * matrix_row0;
		Vector8 row1 = y_splatted * matrix_row1;
		Vector8 row2 = z_splatted * matrix_row2;
		Vector8 row3 = w_splatted * matrix_row3;

		return row0 + row1 + row2 + row3;
	}

	// If matrices are different, this method can be used but it will perform worse than the single matrix
	// because of the register usage that might push onto the stack some registers
	ECS_INLINE Vector8 ECS_VECTORCALL MatrixVectorMultiply(Vector8 vector, Matrix matrix0, Matrix matrix1) {
		Vector8 x_splatted = permute8<0, 0, 0, 0, 4, 4, 4, 4>(vector);
		Vector8 y_splatted = permute8<1, 1, 1, 1, 5, 5, 5, 5>(vector);
		Vector8 z_splatted = permute8<2, 2, 2, 2, 6, 6, 6, 6>(vector);
		Vector8 w_splatted = permute8<3, 3, 3, 3, 7, 7, 7, 7>(vector);

		Vector8 matrix_row0 = Permute2f128<0x20>(matrix0.v[0], matrix1.v[0]);
		Vector8 matrix_row1 = Permute2f128<0x31>(matrix0.v[0], matrix1.v[0]);
		Vector8 matrix_row2 = Permute2f128<0x20>(matrix0.v[1], matrix1.v[1]);
		Vector8 matrix_row3 = Permute2f128<0x31>(matrix0.v[1], matrix1.v[1]);

		Vector8 row0 = x_splatted * matrix_row0;
		Vector8 row1 = y_splatted * matrix_row1;
		Vector8 row2 = z_splatted * matrix_row2;
		Vector8 row3 = w_splatted * matrix_row3;

		return row0 + row1 + row2 + row3;
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL RotateVector(float3 rotation, Vector4 direction) {
		alignas(16) float4 result;

		Matrix rotation_matrix = MatrixRotation(rotation);
		Vector4 rotated_direction = MatrixVectorMultiply(direction, rotation_matrix);
		rotated_direction.StoreAligned(&result);

		return { result.x, result.y, result.z };
	}

	ECS_INLINE Vector4 ECS_VECTORCALL RotateVectorSIMD(float3 rotation, Vector4 direction) {
		Matrix rotation_matrix = MatrixRotation(rotation);
		return MatrixVectorMultiply(direction, rotation_matrix);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL RotateVectorSIMD(float3 rotation, Vector8 direction) {
		Matrix rotation_matrix = MatrixRotation(rotation);
		return MatrixVectorMultiply(direction, rotation_matrix);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL RotateVectorSIMD(float3 rotation0, float3 rotation1, Vector8 direction) {
		Matrix rotation_matrix0 = MatrixRotation(rotation0);
		Matrix rotation_matrix1 = MatrixRotation(rotation1);
		return MatrixVectorMultiply(direction, rotation_matrix0, rotation_matrix1);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL GetRightVector(float3 rotation) {
		return RotateVector(rotation, VectorGlobals::RIGHT_4);
	}

	ECS_INLINE Vector4 ECS_VECTORCALL GetRightVectorSIMD(float3 rotation) {
		return RotateVectorSIMD(rotation, VectorGlobals::RIGHT_4);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL GetUpVector(float3 rotation) {
		return RotateVector(rotation, VectorGlobals::UP_4);
	}

	ECS_INLINE Vector4 ECS_VECTORCALL GetUpVectorSIMD(float3 rotation) {
		return RotateVectorSIMD(rotation, VectorGlobals::UP_4);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL GetForwardVector(float3 rotation) {
		return RotateVector(rotation, VectorGlobals::FORWARD_4);
	}

	ECS_INLINE Vector4 ECS_VECTORCALL GetForwardVectorSIMD(float3 rotation) {
		return RotateVectorSIMD(rotation, VectorGlobals::FORWARD_4);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL TransformPointSIMD(float3 point, Matrix matrix) {
		Vector4 vector_point(&point);
		Vector4 one = VectorGlobals::ONE_4;
		vector_point = blend4<0, 1, 2, 7>(vector_point, one);

		return MatrixVectorMultiply(vector_point, matrix);
	}

	// Applies the same matrix to low and high
	ECS_INLINE Vector8 ECS_VECTORCALL TransformPointSIMD(float3 point0, float3 point1, Matrix matrix) {
		Vector8 vector_point(point0.x, point0.y, point0.z, 1.0f, point1.x, point1.y, point1.z, 1.0f);

		return MatrixVectorMultiply(vector_point, matrix);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL TransformPointSIMD(float3 point0, float3 point1, Matrix matrix0, Matrix matrix1) {
		Vector8 vector_point(point0.x, point0.y, point0.z, 1.0f, point1.x, point1.y, point1.z, 1.0f);

		return MatrixVectorMultiply(vector_point, matrix0, matrix1);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL TransformPoint(float3 point, Matrix matrix) {
		float3 result;

		Vector4 vector_result = TransformPointSIMD(point, matrix);
		vector_result.StorePartialConstant<3>(&result);

		return result;
	}

	// Uses the same matrix for both points
	ECS_INLINE void ECS_VECTORCALL TransformPoint(float3& point0, float3& point1, Matrix matrix) {
		Vector8 result = TransformPointSIMD(point0, point1, matrix);	
		result.StorePartialConstant<3>(&point0);
		result.High().StorePartialConstant<3>(&point1);
	}

	// Might perform slower than the single matrix because of the high register usage that
	// can cause stack spillage
	ECS_INLINE void ECS_VECTORCALL TransformPoint(float3& point0, float3& point1, Matrix matrix0, Matrix matrix1) {
		Vector8 result = TransformPointSIMD(point0, point1, matrix0, matrix1);
		result.StorePartialConstant<3>(&point0);
		result.High().StorePartialConstant<3>(&point1);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Rotation in angles
	ECS_INLINE Matrix ECS_VECTORCALL MatrixTransform(float3 translation, float3 rotation, float3 scale) {
		return MatrixScale(scale) * MatrixRotation(rotation) * MatrixTranslation(translation);
	}

	// Rotation in radians
	ECS_INLINE Matrix ECS_VECTORCALL MatrixTransformRad(float3 translation, float3 rotation, float3 scale) {
		return MatrixScale(scale) * MatrixRotationRad(rotation) * MatrixTranslation(translation);
	}

	// --------------------------------------------------------------------------------------------------------------

}