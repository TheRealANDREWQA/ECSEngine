#include "ecspch.h"
#include "Matrix.h"
#include "../Utilities/Utilities.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------

	Matrix::Matrix(
		float x00, float x01, float x02, float x03,
		float x10, float x11, float x12, float x13,
		float x20, float x21, float x22, float x23,
		float x30, float x31, float x32, float x33
	)
	{
		v[0] = Vec8f(x00, x01, x02, x03, x10, x11, x12, x13);
		v[1] = Vec8f(x20, x21, x22, x23, x30, x31, x32, x33);
	}

	Matrix::Matrix(const float* values)
	{
		v[0].load(values);
		v[1].load(values + 8);
	}

	Matrix::Matrix(const float4& row0, const float4& row1, const float4& row2, const float4& row3)
	{
		Vec4f first_row = Vec4f().load((const float*)&row0);
		Vec4f second_row = Vec4f().load((const float*)&row1);
		Vec4f third_row = Vec4f().load((const float*)&row2);
		Vec4f fourth_row = Vec4f().load((const float*)&row3);

		v[0] = Vec8f(first_row, second_row);
		v[1] = Vec8f(third_row, fourth_row);
	}

	bool ECS_VECTORCALL Matrix::operator == (Matrix other) {
		Vec8f epsilon(ECS_MATRIX_EPSILON);
		return horizontal_and(abs(v[0] - other.v[0]) < epsilon && abs(v[1] - other.v[1]) < epsilon);
	}
	
	bool ECS_VECTORCALL Matrix::operator != (Matrix other) {
		return !(*this == other);
	}

	Matrix ECS_VECTORCALL Matrix::operator + (Matrix other) const {
		return Matrix(v[0] + other.v[0], v[1] + other.v[1]);
	}

	Matrix ECS_VECTORCALL Matrix::operator - (Matrix other) const {
		return Matrix(v[0] - other.v[0], v[1] - other.v[1]);
	}

	Matrix ECS_VECTORCALL Matrix::operator * (float value) const {
		Vec8f vector_value = Vec8f(value);
		return Matrix(v[0] * vector_value, v[1] * vector_value);
	}

	Matrix ECS_VECTORCALL Matrix::Matrix::operator / (float value) const {
		Vec8f vector_value = Vec8f(1.0f / value);
		return Matrix(v[0] * vector_value, v[1] * vector_value);
	}

	Matrix ECS_VECTORCALL Matrix::operator * (Matrix other) const {
		return MatrixMultiply(*this, other);
	}

	Matrix& ECS_VECTORCALL Matrix::operator += (Matrix other) {
		v[0] += other.v[0];
		v[1] += other.v[1];
		return *this;
	}

	Matrix& ECS_VECTORCALL Matrix::operator -= (Matrix other) {
		v[0] -= other.v[0];
		v[1] -= other.v[1];
		return *this;
	}

	Matrix& Matrix::operator *= (float value) {
		Vec8f vector_value = Vec8f(value);
		v[0] *= vector_value;
		v[1] *= vector_value;
		return *this;
	}

	Matrix& Matrix::operator /= (float value) {
		Vec8f vector_value = Vec8f(1.0f / value);
		v[0] *= vector_value;
		v[1] *= vector_value;

		return *this;
	}

	Matrix& Matrix::Load(const void* values) {
		const float* float_values = (const float*)values;
		v[0].load(float_values);
		v[1].load(float_values + 8);
		return *this;
	}

	Matrix& Matrix::LoadAligned(const void* values) {
		const float* float_values = (const float*)values;
		v[0].load_a(float_values);
		v[1].load_a(float_values + 8);
		return *this;
	}

	void Matrix::Store(void* values) const
	{
		float* float_values = (float*)values;
		v[0].store(float_values);
		v[1].store(float_values + 8);
	}

	void Matrix::StoreAligned(void* values) const
	{
		float* float_values = (float*)values;
		v[0].store_a(float_values);
		v[1].store_a(float_values + 8);
	}

	void Matrix::StoreStreamed(void* values) const
	{
		float* float_values = (float*)values;
		v[0].store_nt(float_values);
		v[1].store_nt(float_values + 8);
	}

	// -----------------------------------------------------------------------------------------------------

	void ScalarMatrixMultiply(const float* ECS_RESTRICT a, const float* ECS_RESTRICT b, float* ECS_RESTRICT destination) {
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

	Vec8f ECS_VECTORCALL MatrixTwoRowProduct(Vec8f first_matrix_two_rows, Vec8f second_matrix_v0, Vec8f second_matrix_v1) {
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

	Matrix ECS_VECTORCALL MatrixMultiply(Matrix a, Matrix b) {
		return Matrix(MatrixTwoRowProduct(a.v[0], b.v[0], b.v[1]), MatrixTwoRowProduct(a.v[1], b.v[0], b.v[1]));
	}

	Matrix3x3 MatrixMultiply(const Matrix3x3& a, const Matrix3x3& b) {
		Matrix3x3 multiplication;
		for (size_t row = 0; row < 3; row++) {
			for (size_t column = 0; column < 3; column++) {
				for (size_t other_column = 0; other_column < 3; other_column++) {
					multiplication.values[row][column] += a.values[row][other_column] * b.values[other_column][column];
				}
			}
		}
		return multiplication;
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixTranspose(Matrix matrix) {
		Vec8f first, second;
		// 4 registers needed to keep the indices for blending
		first = blend8<0, 4, 8, 12, 1, 5, 9, 13>(matrix.v[0], matrix.v[1]);
		second = blend8<2, 6, 10, 14, 3, 7, 11, 15>(matrix.v[0], matrix.v[1]);
		return Matrix(first, second);
	}

	Matrix3x3 MatrixTranspose(const Matrix3x3& matrix) {
		return {
			matrix.values[0][0], matrix.values[1][0], matrix.values[2][0],
			matrix.values[0][1], matrix.values[1][1], matrix.values[2][1],
			matrix.values[0][2], matrix.values[1][2], matrix.values[2][2]
		};
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixIdentity() {
		Vec8f zero = ZeroVectorFloat();
		Vec8f one = VectorGlobals::ONE;

		return {
			blend8<8, 1, 2, 3, 4, 13, 6, 7>(zero, one),
			blend8<0, 1, 10, 3, 4, 5, 6, 15>(zero, one)
		};
	}

	Matrix3x3 Matrix3x3Identity() {
		return {
			1.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 1.0f
		};
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixNull() {
		Vec8f zero = ZeroVectorFloat();
		return Matrix(zero, zero);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Returns the matrix with the last column and row set to 0.0f
	Matrix3x3 ECS_VECTORCALL MatrixTo3x3(Matrix matrix) {
		alignas(alignof(Matrix)) float matrix_values[4][4];
		matrix.StoreAligned(matrix_values);

		return {
			matrix_values[0][0], matrix_values[0][1], matrix_values[0][2],
			matrix_values[1][0], matrix_values[1][1], matrix_values[1][2],
			matrix_values[2][0], matrix_values[2][1], matrix_values[2][2]
		};
	}

	// --------------------------------------------------------------------------------------------------------------

	// Returns the matrix with the last column set to 0.0f
	Matrix ECS_VECTORCALL MatrixTo4x3(Matrix matrix) {
		Vec8f zero_vector = ZeroVectorFloat();
		matrix.v[0] = PerLaneBlend<0, 1, 2, 7>(matrix.v[0], zero_vector);
		matrix.v[1] = PerLaneBlend<0, 1, 2, 7>(matrix.v[1], zero_vector);
		return matrix;
	}

	// --------------------------------------------------------------------------------------------------------------

	// Returns the matrix with the last row set to 0.0f
	Matrix ECS_VECTORCALL MatrixTo3x4(Matrix matrix) {
		Vec8f zero_vector = ZeroVectorFloat();
		matrix.v[1] = BlendLowAndHigh(matrix.v[1], zero_vector);
		return matrix;
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

	Vec4f ECS_VECTORCALL MatrixVec4fDeterminant(Matrix matrix) {
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

		Vec4f second_row = matrix.v[0].get_high();
		Vec4f determinants_factors1 = permute4<1, 0, 0, 0>(second_row);
		Vec4f determinants_factors2 = permute4<2, 2, 1, 1>(second_row);
		Vec4f determinants_factors3 = permute4<3, 3, 3, 2>(second_row);

		Vec4f determinants_2x2_factored1 = determinants_2x2_unfactored1 * determinants_factors1;
		Vec4f determinants_2x2_factored2 = determinants_2x2_unfactored2 * determinants_factors2;
		Vec4f determinants_2x2_factored3 = determinants_2x2_unfactored3 * determinants_factors3;

		Vec4f determinants_3x3 = determinants_2x2_factored1 - determinants_2x2_factored2 + determinants_2x2_factored3;

		Vec4f initial_4_factors = matrix.v[0].get_low();
		initial_4_factors = change_sign<0, 1, 0, 1>(initial_4_factors);

		return _mm_dp_ps(determinants_3x3, initial_4_factors, 0xFF);
	}

	// --------------------------------------------------------------------------------------------------------------

	float ECS_VECTORCALL MatrixDeterminant(Matrix matrix) {
		return _mm_cvtss_f32(MatrixVec4fDeterminant(matrix));
	}

	// --------------------------------------------------------------------------------------------------------------

	float Matrix3x3Determinant(const Matrix3x3& matrix) {
		// a00 a01 a02
		// a10 a11 a12
		// a20 a21 a22

		float a00 = matrix.values[0][0];
		float a01 = matrix.values[0][1];
		float a02 = matrix.values[0][2];
		float a10 = matrix.values[1][0];
		float a11 = matrix.values[1][1];
		float a12 = matrix.values[1][2];
		float a20 = matrix.values[2][0];
		float a21 = matrix.values[2][1];
		float a22 = matrix.values[2][2];

		float value0 = a00 * a11 * a22;
		float value1 = a10 * a21 * a02;
		float value2 = a20 * a01 * a12;
		float value3 = a02 * a11 * a20;
		float value4 = a12 * a21 * a00;
		float value5 = a22 * a01 * a10;
		
		return value0 + value1 + value2 - value3 - value4 - value5;
	}

	// --------------------------------------------------------------------------------------------------------------

	// Register pressure present, many memory swaps are needed to keep permute indices into registers
	Matrix ECS_VECTORCALL MatrixInverse(Matrix matrix) {
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
		Vec4f first_row = matrix.v[0].get_low();
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

	Matrix3x3 Matrix3x3Inverse(const Matrix3x3& matrix) {
		float a00 = matrix.values[0][0];
		float a01 = matrix.values[0][1];
		float a02 = matrix.values[0][2];
		float a10 = matrix.values[1][0];
		float a11 = matrix.values[1][1];
		float a12 = matrix.values[1][2];
		float a20 = matrix.values[2][0];
		float a21 = matrix.values[2][1];
		float a22 = matrix.values[2][2];

		float determinant = Matrix3x3Determinant(matrix);
		float determinant_inverse = 1.0f / determinant;

		float result00 = a21 * a12 - a11 * a22;
		float result01 = a01 * a22 - a21 * a02;
		float result02 = a11 * a02 - a01 * a12;
		float result10 = a22 * a10 - a12 * a20;
		float result11 = a02 * a20 - a00 * a22;
		float result12 = a00 * a12 - a02 * a10;
		float result20 = a11 * a20 - a10 * a21;
		float result21 = a00 * a21 - a20 * a01;
		float result22 = a01 * a10 - a00 * a11;

		return {
				result00 * determinant_inverse, result01 * determinant_inverse, result02 * determinant_inverse,
				result01 * determinant_inverse, result11 * determinant_inverse, result12 * determinant_inverse,
				result02 * determinant_inverse, result21 * determinant_inverse, result22 * determinant_inverse
		};
	}

	// --------------------------------------------------------------------------------------------------------------

	// Will use doubles for calculations and will convert at the end in floats
	Matrix ECS_VECTORCALL MatrixInversePrecise(Matrix matrix) {
		Vec4d first_row = to_double(matrix.v[0].get_low());
		Vec4d second_row = to_double(matrix.v[0].get_high());
		Vec4d third_row = to_double(matrix.v[1].get_low());
		Vec4d fourth_row = to_double(matrix.v[1].get_high());

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

	Matrix ECS_VECTORCALL MatrixTranslation(float x, float y, float z) {
		Matrix result;

		// The last row contains the translation values

		alignas(ECS_SIMD_BYTE_SIZE) float4 values[sizeof(Matrix) / sizeof(float4)];
		values[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
		values[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
		values[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
		values[3] = { x, y, z, 1.0f };

		return result.LoadAligned(values);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixTranslation(float3 translation) {
		return MatrixTranslation(translation.x, translation.y, translation.z);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixScale(float x, float y, float z) {
		Matrix result;

		Vec8f zero = ZeroVectorFloat();
		Vec8f values(x, 0.0f, z, 0.0f, 0.0f, y, 0.0f, 1.0f);

		result.v[0] = blend8<0, 1, 10, 3, 4, 5, 6, 15>(values, zero);
		result.v[1] = blend8<8, 1, 2, 3, 4, 13, 6, 7>(values, zero);

		return result;
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixScale(float3 scale) {
		return MatrixScale(scale.x, scale.y, scale.z);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixRotationXRad(float angle_radians) {
		Matrix result;

		float sin_value = sin(angle_radians);
		float cos_value = cos(angle_radians);

		alignas(ECS_SIMD_BYTE_SIZE) float4 scalar_values[sizeof(Matrix) / sizeof(float4)];
		scalar_values[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
		scalar_values[1] = { 0.0f, cos_value, sin_value, 0.0f };
		scalar_values[2] = { 0.0f, -sin_value, cos_value, 0.0f };
		scalar_values[3] = { 0.0f, 0.0f, 0.0f, 1.0f };

		return result.LoadAligned(scalar_values);
	}

	Matrix ECS_VECTORCALL MatrixRotationX(float angle) {
		angle = DegToRad(angle);
		return MatrixRotationXRad(angle);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixRotationYRad(float angle_radians) {
		Matrix result;

		float sin_value = sin(angle_radians);
		float cos_value = cos(angle_radians);

		alignas(ECS_SIMD_BYTE_SIZE) float4 scalar_values[sizeof(Matrix) / sizeof(float4)];
		scalar_values[0] = { cos_value, 0.0f, -sin_value, 0.0f };
		scalar_values[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
		scalar_values[2] = { sin_value, 0.0f, cos_value, 0.0f };
		scalar_values[3] = { 0.0f, 0.0f, 0.0f, 1.0f };

		return result.LoadAligned(scalar_values);
	}

	Matrix ECS_VECTORCALL MatrixRotationY(float angle) {
		angle = DegToRad(angle);
		return MatrixRotationYRad(angle);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixRotationZRad(float angle_radians) {
		Matrix result;

		float sin_value = sin(angle_radians);
		float cos_value = cos(angle_radians);

		alignas(ECS_SIMD_BYTE_SIZE) float4 scalar_values[sizeof(Matrix) / sizeof(float4)];
		scalar_values[0] = { cos_value, sin_value, 0.0f, 0.0f };
		scalar_values[1] = { -sin_value, cos_value, 0.0f, 0.0f };
		scalar_values[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
		scalar_values[3] = { 0.0f, 0.0f, 0.0f, 1.0f };

		return result.LoadAligned(scalar_values);
	}

	Matrix ECS_VECTORCALL MatrixRotationZ(float angle) {
		angle = DegToRad(angle);
		return MatrixRotationZRad(angle);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixRotation(float3 rotation) {
		return MatrixRotationX(rotation.x) * MatrixRotationY(rotation.y) * MatrixRotationZ(rotation.z);
	}

	Matrix ECS_VECTORCALL MatrixRotationRad(float3 rotation) {
		return MatrixRotationXRad(rotation.x) * MatrixRotationYRad(rotation.y) * MatrixRotationZRad(rotation.z);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixRotationXZY(float3 rotation) {
		return MatrixRotationX(rotation.x) * MatrixRotationZ(rotation.z) * MatrixRotationY(rotation.y);
	}

	Matrix ECS_VECTORCALL MatrixRotationZXY(float3 rotation) {
		return MatrixRotationZ(rotation.z) * MatrixRotationX(rotation.x) * MatrixRotationY(rotation.y);
	}

	Matrix ECS_VECTORCALL MatrixRotationZYX(float3 rotation) {
		return MatrixRotationZ(rotation.z) * MatrixRotationY(rotation.y) * MatrixRotationX(rotation.x);
	}

	Matrix ECS_VECTORCALL MatrixRotationYXZ(float3 rotation) {
		return MatrixRotationY(rotation.y) * MatrixRotationX(rotation.x) * MatrixRotationZ(rotation.z);
	}
	
	Matrix ECS_VECTORCALL MatrixRotationYZX(float3 rotation) {
		return MatrixRotationY(rotation.y) * MatrixRotationZ(rotation.z) * MatrixRotationX(rotation.x);
	}

	Matrix ECS_VECTORCALL MatrixRotationXZYRad(float3 rotation) {
		return MatrixRotationXRad(rotation.x) * MatrixRotationZRad(rotation.z) * MatrixRotationYRad(rotation.y);
	}

	Matrix ECS_VECTORCALL MatrixRotationZXYRad(float3 rotation) {
		return MatrixRotationZRad(rotation.z) * MatrixRotationXRad(rotation.x) * MatrixRotationYRad(rotation.y);
	}

	Matrix ECS_VECTORCALL MatrixRotationZYXRad(float3 rotation) {
		return MatrixRotationZRad(rotation.z) * MatrixRotationYRad(rotation.y) * MatrixRotationXRad(rotation.x);
	}

	Matrix ECS_VECTORCALL MatrixRotationYXZRad(float3 rotation) {
		return MatrixRotationYRad(rotation.y) * MatrixRotationXRad(rotation.x) * MatrixRotationZRad(rotation.z);
	}

	Matrix ECS_VECTORCALL MatrixRotationYZXRad(float3 rotation) {
		return MatrixRotationYRad(rotation.y) * MatrixRotationZRad(rotation.z) * MatrixRotationXRad(rotation.x);
	}

	Matrix ECS_VECTORCALL MatrixRotationCamera(float3 rotation) {
		return MatrixRotationZYX(-rotation);
	}

	Matrix ECS_VECTORCALL MatrixRotationCameraRad(float3 rotation) {
		return MatrixRotationZYXRad(-rotation);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixTRS(Matrix translation, Matrix rotation, Matrix scale) {
		return scale * rotation * translation;
	}

	Matrix ECS_VECTORCALL MatrixRS(Matrix rotation, Matrix scale) {
		return scale * rotation;
	}

	Matrix ECS_VECTORCALL MatrixTR(Matrix translation, Matrix rotation) {
		return rotation * translation;
	}

	Matrix ECS_VECTORCALL MatrixTS(Matrix translation, Matrix scale) {
		return scale * translation;
	}

	Matrix ECS_VECTORCALL MatrixMVP(Matrix object_matrix, Matrix camera_matrix) {
		return object_matrix * camera_matrix;
	}

	Matrix ECS_VECTORCALL MatrixGPU(Matrix matrix) {
		return MatrixTranspose(matrix);
	}

	Matrix ECS_VECTORCALL MatrixMVPToGPU(Matrix object_matrix, Matrix camera_matrix) {
		return MatrixGPU(MatrixMVP(object_matrix, camera_matrix));
	}

	Matrix ECS_VECTORCALL MatrixMVPToGPU(Matrix translation, Matrix rotation, Matrix scale, Matrix camera_matrix) {
		return MatrixMVPToGPU(MatrixTRS(translation, rotation, scale), camera_matrix);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixLookTo(float3 origin, float3 direction, float3 up) {
		// We construct basis vectors, and then we write them as rows inside the matrix
		// The result looks like this
		/*
			Here x is the right basis vector, y is the up basis vector, z is the forward (direction)
			basis vector and p is the translation - the dot product with the inverted origin
			{
				x.x, x.y, x.z, 0.0f
				y.x, y.y, y.z, 0.0f,
				z.x, z.y, z.z, 0.0f,
				p.x, p.y, p.z, 1.0f
			}
		*/

		float3 normalized_direction = NormalizeIfNot(direction);
		float3 normalized_right = Normalize(Cross(up, normalized_direction));
		float3 inverted_normalized_up = NormalizeIfNot(-up);

		float3 inverted_origin = -origin;
		float dot0 = Dot(normalized_right, inverted_origin);
		float dot1 = Dot(inverted_normalized_up, inverted_origin);
		float dot2 = Dot(normalized_direction, inverted_origin);

		alignas(ECS_SIMD_BYTE_SIZE) float4 matrix_elements[sizeof(Matrix) / sizeof(float4)];
		matrix_elements[0] = { normalized_right, 0.0f };
		matrix_elements[1] = { inverted_normalized_up, 0.0f };
		matrix_elements[2] = { normalized_direction, 0.0f };
		matrix_elements[3] = { dot0, dot1, dot2, 1.0f };
		return Matrix().LoadAligned(matrix_elements);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixLookAt(float3 origin, float3 focus_point, float3 up) {
		return MatrixLookTo(origin, focus_point - origin, up);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixPerspective(float left, float right, float top, float bottom, float near_z, float far_z) {
		float two_near_z = near_z + near_z;
		float range = far_z / (far_z - near_z);
		float width = right - left;
		float height = bottom - top;
		float width_inverse = 1.0f / width;
		float height_inverse = 1.0f / height;

		// 2n / width                  0                      0          0
		//     0                    2n / height               0          0
		// - (r + l) / width   - (b + t) / (b - t)          range        1
		//     0                       0               -range * near_z   0
		return Matrix(
			two_near_z * width_inverse, 0.0f, 0.0f, 0.0f, 
			0.0f, two_near_z * height_inverse, 0.0f, 0.0f, 
			- (right + left) * width_inverse, -(bottom + top) * height_inverse, range, 1.0f,
			0.0f, 0.0f, -range * near_z, 0.0f
		);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixPerspective(float width, float height, float near_z, float far_z) {
		float half_width = width * 0.5f;
		float half_height = height * 0.5f;
		return MatrixPerspective(-half_width, half_width, -half_height, half_height, near_z, far_z);
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixPerspectiveFOV(float fov, float aspect_ratio, float near_z, float far_z) {
		// y_scale = cotangent(fov / 2.0f);
		// x_scale = y_scale / aspect_ratio;
		// range = far_z / (far_z - near_z);
		

		// x_scale       0          0            0
		//    0      y_scale        0            0
		//    0         0        range           1
		//    0         0   -range * near_z      0
		
		fov = DegToRad(fov);
		float tangent = tanf(fov * 0.5f);
		float y_scale = 1.0f / tangent;
		float x_scale = y_scale / aspect_ratio;
		float range = far_z / (far_z - near_z);
		return Matrix(x_scale, 0.0f, 0.0f, 0.0f, 0.0f, y_scale, 0.0f, 0.0f, 0.0f, 0.0f, range, 1.0f, 0.0f, 0.0f, -range * near_z, 0.0f);
	}

	// --------------------------------------------------------------------------------------------------------------

	float HorizontalFOVFromVerticalRad(float fov_rad, float aspect_ratio) {
		// hfov = 2 * arctan(tan(V/2) * aspect_ratio)
		return 2 * atanf(tan(fov_rad * 0.5f) * aspect_ratio);
	}

	float HorizontalFOVFromVertical(float fov_angles, float aspect_ratio) {
		return RadToDeg(HorizontalFOVFromVerticalRad(DegToRad(fov_angles), aspect_ratio));
	}

	float VerticalFOVFromHorizontalRad(float fov_radians, float aspect_ratio) {
		// vfov = 2 * arctan(tan(H/2) / aspect_ratio)
		return 2 * atanf(tan(fov_radians * 0.5f) / aspect_ratio);
	}

	float VerticalFOVFromHorizontal(float fov_angles, float aspect_ratio) {
		return RadToDeg(VerticalFOVFromHorizontalRad(DegToRad(fov_angles), aspect_ratio));
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixOrthographic(float left, float right, float top, float bottom, float near_z, float far_z) {
		//         2 / width                     0                    0          0
		//           0                     2 / height                 0          0
		//           0                          0                   range        0
		// - (r + l) / (r - l)        - (b + t) / (b - t)   -range * near_z      1

		float range = 1.0f / (far_z - near_z);
		float width = right - left;
		float height = bottom - top;
		float width_inverse = 1.0f / width;
		float height_inverse = 1.0f / height;
		return Matrix(
			2.0f * width_inverse, 0.0f, 0.0f, 0.0f, 
			0.0f, 2.0f * height_inverse, 0.0f, 0.0f, 
			0.0f, 0.0f, range, 0.0f, 
			-(right + left) * width_inverse, - (bottom + top) * height_inverse, -range * near_z, 1.0f
		);
	}

	Matrix ECS_VECTORCALL MatrixOrthographic(float width, float height, float near_z, float far_z) {
		float half_width = width * 0.5f;
		float half_height = height * 0.5f;
		return MatrixOrthographic(-half_width, half_width, -half_height, half_height, near_z, far_z);
	}

	// --------------------------------------------------------------------------------------------------------------

#undef near
#undef far

	Matrix ECS_VECTORCALL MatrixFrustum(float left, float right, float top, float bottom, float near, float far) {
		float near2 = 2.0f * near;
		float right_left = right - left;
		float top_bottom = top - bottom;
		float far_near = far - near;

		float right_left_inverse = 1.0f / right_left;
		float top_bottom_inverse = 1.0f / top_bottom;
		float far_near_inverse = 1.0f / far_near;

		return Matrix(
			     near2 * right_left_inverse,                      0.0f,								0.0f,                0.0f,
			           0.0f,							near2 * top_bottom_inverse,                 0.0f,                0.0f,
			(right + left) * right_left_inverse,  (top + bottom) * top_bottom_inverse,    -(far + near) * far_near_inverse,     -1.0f,
			           0.0f,									 0.0f,               (-far * near2) * far_near_inverse,     0.0f
		);
	}

	// --------------------------------------------------------------------------------------------------------------

	// This is only for the scalar version
	template<typename Vector>
	static ECS_INLINE Vector ECS_VECTORCALL MatrixVectorMultiplyScalarImpl(Vector vector, Matrix matrix) {
		// Broadcast each vector element and then multiply with the matrix
		// Here it is probably easier to use 4 wide SSE instead of AVX since
		// Of the costs of doing operations on the high lane

		Vec4f vector_x = vector.x;
		Vec4f vector_y = vector.y;
		Vec4f vector_z = vector.z;

		Vec4f result = vector_x * matrix.v[0].get_low() + vector_y * matrix.v[0].get_high() + vector_z * matrix.v[1].get_low();
		if constexpr (std::is_same_v<Vector, float4>) {
			Vec4f vector_w = vector.w;
			result += vector_w * matrix.v[1].get_high();
		}

		alignas(ECS_SIMD_BYTE_SIZE) float4 values;
		result.store_a((float*)&values);

		if constexpr (std::is_same_v<Vector, float4>) {
			return values;
		}
		else {
			return values.xyz();
		}
	}

	// This is only for the SIMD version
	template<typename Vector>
	static Vector ECS_VECTORCALL MatrixVectorMultiplySIMDImpl(Vector vector, Matrix matrix) {
		// Broadcast each matrix element and add per columns such that
		// We use the least amount of registers and don't risk stack spilling
		Vec8f matrix_00 = Broadcast8<0>(matrix.v[0]);
		Vec8f matrix_10 = Broadcast8<4>(matrix.v[0]);
		Vec8f matrix_20 = Broadcast8<0>(matrix.v[1]);
		Vec8f result_x = vector.x * matrix_00 + vector.y * matrix_10 + vector.z * matrix_20;
		if constexpr (std::is_same_v<Vector, Vector4>) {
			Vec8f matrix_30 = Broadcast8<4>(matrix.v[1]);
			result_x += vector.w * matrix_30;
		}

		Vec8f matrix_01 = Broadcast8<1>(matrix.v[0]);
		Vec8f matrix_11 = Broadcast8<5>(matrix.v[0]);
		Vec8f matrix_21 = Broadcast8<1>(matrix.v[1]);
		Vec8f result_y = vector.x * matrix_01 + vector.y * matrix_11 + vector.z * matrix_21;
		if constexpr (std::is_same_v<Vector, Vector4>) {
			Vec8f matrix_31 = Broadcast8<5>(matrix.v[1]);
			result_y += vector.w * matrix_31;
		}

		Vec8f matrix_02 = Broadcast8<2>(matrix.v[0]);
		Vec8f matrix_12 = Broadcast8<6>(matrix.v[0]);
		Vec8f matrix_22 = Broadcast8<2>(matrix.v[1]);
		Vec8f result_z = vector.x * matrix_02 + vector.y * matrix_12 + vector.z * matrix_22;
		if constexpr (std::is_same_v<Vector, Vector4>) {
			Vec8f matrix_32 = Broadcast8<6>(matrix.v[1]);
			result_z += vector.w * matrix_32;
		}

		if constexpr (std::is_same_v<Vector, Vector4>) {
			Vec8f matrix_03 = Broadcast8<3>(matrix.v[0]);
			Vec8f matrix_13 = Broadcast8<7>(matrix.v[0]);
			Vec8f matrix_23 = Broadcast8<3>(matrix.v[1]);
			Vec8f matrix_33 = Broadcast8<7>(matrix.v[1]);
			Vec8f result_w = vector.x * matrix_03 + vector.y * matrix_13 + vector.z * matrix_23 + vector.w * matrix_33;
			return { result_x, result_y, result_z, result_w };
		}
		else {
			return { result_x, result_y, result_z };
		}
	}

	float3 ECS_VECTORCALL MatrixVectorMultiply(float3 vector, Matrix matrix) {
		return MatrixVectorMultiplyScalarImpl(vector, matrix);
	}

	float4 ECS_VECTORCALL MatrixVectorMultiply(float4 vector, Matrix matrix) {
		return MatrixVectorMultiplyScalarImpl(vector, matrix);
	}

	Vector3 ECS_VECTORCALL MatrixVectorMultiply(Vector3 vector, Matrix matrix) {
		return MatrixVectorMultiplySIMDImpl(vector, matrix);
	}

	Vector4 ECS_VECTORCALL MatrixVectorMultiply(Vector4 vector, Matrix matrix) {
		return MatrixVectorMultiplySIMDImpl(vector, matrix);
	}

	// --------------------------------------------------------------------------------------------------------------

	float3 ECS_VECTORCALL MatrixVectorMultiply(float3 vector, const Matrix3x3& matrix) {
		return {
			vector.x * matrix.values[0][0] + vector.y * matrix.values[1][0] + vector.z * matrix.values[2][0],
			vector.x * matrix.values[0][1] + vector.y * matrix.values[1][1] + vector.z * matrix.values[2][1],
			vector.x * matrix.values[0][2] + vector.y * matrix.values[1][2] + vector.z * matrix.values[2][2]
		};
	}

	// --------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL MatrixTransformRad(float3 translation, float3 rotation, float3 scale) {
		return MatrixTRS(MatrixTranslation(translation), MatrixRotationRad(rotation), MatrixScale(scale));
	}

	Matrix ECS_VECTORCALL MatrixTransform(float3 translation, float3 rotation, float3 scale) {
		return MatrixTransformRad(translation, DegToRad(rotation), scale);
	}

	// --------------------------------------------------------------------------------------------------------------

	void ECS_VECTORCALL ChangeLineSpace(float3* line_point, float3* line_direction_normalized, Matrix matrix) {
		// The point needs to be transformed as a point, while the direction as a vector
		*line_point = TransformPoint(*line_point, matrix).xyz();
		*line_direction_normalized = MatrixVectorMultiply(*line_direction_normalized, matrix);
	}
	
	// --------------------------------------------------------------------------------------------------------------

	void ECS_VECTORCALL TransformPoints(
		Matrix matrix, 
		const float* ECS_RESTRICT input_points_soa, 
		size_t count, 
		size_t capacity, 
		float* ECS_RESTRICT output_points_soa,
		size_t output_capacity
	)
	{
		ApplySIMDConstexpr(count, Vector3::ElementCount(), [matrix, capacity, input_points_soa, output_points_soa, output_capacity]
		(auto is_normal_iteration, size_t index, size_t current_count) {
			Vector3 elements;
			if constexpr (is_normal_iteration) {
				elements = Vector3().LoadAdjacent(input_points_soa, index, capacity);
			}
			else {
				elements = Vector3().LoadPartialAdjacent(input_points_soa, index, capacity, current_count);
			}

			Vector3 transformed_elements = TransformPoint(elements, matrix).xyz();
			if constexpr (is_normal_iteration) {
				transformed_elements.StoreAdjacent(output_points_soa, index, output_capacity);
			}
			else {
				transformed_elements.StorePartialAdjacent(output_points_soa, index, output_capacity, current_count);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------

}