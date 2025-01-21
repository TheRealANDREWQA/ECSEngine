#include "ecspch.h"
#include "Conversion.h"

ECS_OPTIMIZE_END;

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------

	void ECS_VECTORCALL QuaternionToMatrixImpl(QuaternionScalar quaternion, float4 values[4]) {
		// The formula is this one (on the internet the values are provided in column major format, here they are row major)
		// w * w + x * x - y * y - z * z          2 w * z + 2 x * y               2 x * z - 2 y * w         0
		//       2 x * y - 2 z * w          w * w + y * y - x * x - z * z         2 x * w + 2 y * z         0
		//       2 w * y + 2 x * z                2 y * z - 2 x * w         w * w + z * z - x * x - y * y   0
		//               0                                0                               0                 1

		float two = 2.0f;
		float x_squared = quaternion.x * quaternion.x;
		float y_squared = quaternion.y * quaternion.y;
		float z_squared = quaternion.z * quaternion.z;
		float w_squared = quaternion.w * quaternion.w;
		float two_wz = two * quaternion.w * quaternion.z;
		float two_xy = two * quaternion.x * quaternion.y;
		float two_xz = two * quaternion.x * quaternion.z;
		float two_yw = two * quaternion.y * quaternion.w;
		float two_xw = two * quaternion.x * quaternion.w;
		float two_yz = two * quaternion.y * quaternion.z;
		values[0] = { w_squared + x_squared - y_squared - z_squared, two_wz + two_xy, two_xz - two_yw, 0.0f };
		values[1] = { two_xy - two_wz, w_squared + y_squared - x_squared - z_squared, two_xw + two_yz, 0.0f };
		values[2] = { two_yw + two_xz, two_yz - two_xw, w_squared + z_squared - x_squared - y_squared, 0.0f };
		values[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
	}

	Matrix ECS_VECTORCALL QuaternionToMatrix(QuaternionScalar quaternion) {
		alignas(ECS_SIMD_BYTE_SIZE) float4 values[sizeof(Matrix) / sizeof(float4)];
		QuaternionToMatrixImpl(quaternion, values);
		return Matrix().LoadAligned(values);
	}

	Matrix3x3 ECS_VECTORCALL QuaternionToMatrix3x3(QuaternionScalar quaternion) {
		float4 values[sizeof(Matrix) / sizeof(float4)];
		QuaternionToMatrixImpl(quaternion, values);
		return Matrix3x3(values[0].xyz(), values[1].xyz(), values[2].xyz());
	}

	void ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, Matrix* matrices, size_t write_count) {
		// The formula is this one (on the internet the values are provided in column major format, here they are row major)
		// w * w + x * x - y * y - z * z          2 w * z + 2 x * y               2 x * z - 2 y * w         0
		//       2 x * y - 2 z * w          w * w + y * y - x * x - z * z         2 x * w + 2 y * z         0
		//       2 w * y + 2 x * z                2 y * z - 2 x * w         w * w + z * z - x * x - y * y   0
		//               0                                0                               0                 1

		Vec8f two = 2.0f;
		Vec8f x_squared = quaternion.x * quaternion.x;
		Vec8f y_squared = quaternion.y * quaternion.y;
		Vec8f z_squared = quaternion.z * quaternion.z;
		Vec8f w_squared = quaternion.w * quaternion.w;
		Vec8f two_wz = two * quaternion.w * quaternion.z;
		Vec8f two_xy = two * quaternion.x * quaternion.y;
		Vec8f two_xz = two * quaternion.x * quaternion.z;
		Vec8f two_yw = two * quaternion.y * quaternion.w;
		Vec8f two_xw = two * quaternion.x * quaternion.w;
		Vec8f two_yz = two * quaternion.y * quaternion.z;

		Vec8f element_00 = w_squared + x_squared - y_squared - z_squared;
		Vec8f element_01 = two_xy - two_wz;
		Vec8f element_02 = two_yw + two_xz;
		Vec8f element_10 = two_wz + two_xy;
		Vec8f element_11 = w_squared + y_squared - x_squared - z_squared;
		Vec8f element_12 = two_yz - two_xw;
		Vec8f element_20 = two_xz - two_yw;
		Vec8f element_21 = two_xw + two_yz;
		Vec8f element_22 = w_squared + z_squared - x_squared - y_squared;

		// Unfortunately, we don't have a scatter function, so we will have to store into a temp
		// stack buffer and copy with a scalar loop
		alignas(ECS_SIMD_BYTE_SIZE) float scalar_values[Vec8f::size()];
		auto write_values = [write_count, matrices, &scalar_values](size_t offset) {
			float* float_matrices = (float*)matrices;
			for (size_t index = 0; index < write_count; index++) {
				size_t index_offset = index * 16;
				float_matrices[index_offset + offset] = scalar_values[index];
			}
		};

		element_00.store_a(scalar_values);
		write_values(0);
		element_01.store_a(scalar_values);
		write_values(1);
		element_02.store_a(scalar_values);
		write_values(2);
		element_10.store_a(scalar_values);
		write_values(4);
		element_11.store_a(scalar_values);
		write_values(5);
		element_12.store_a(scalar_values);
		write_values(6);
		element_20.store_a(scalar_values);
		write_values(8);
		element_21.store_a(scalar_values);
		write_values(9);
		element_22.store_a(scalar_values);
		write_values(10);

		// Now write the 0s and the 1
		for (size_t index = 0; index < write_count; index++) {
			scalar_values[index] = 0.0f;
		}
		write_values(3);
		write_values(7);
		write_values(11);
		write_values(12);
		write_values(13);
		write_values(14);
		for (size_t index = 0; index < write_count; index++) {
			scalar_values[index] = 1.0f;
		}
		write_values(15);
	}

	Matrix ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, size_t index) {
		// The formula is this one (on the internet the values are provided in column major format, here they are row major)
		// w * w + x * x - y * y - z * z           2 x * y - 2 z * w               2 w * y + 2 x * z        0
		//       2 w * z + 2 x * y           w * w + y * y - x * x - z * z         2 y * z - 2 x * w        0
		//       2 x * z - 2 y * w                 2 x * w + 2 y * z         w * w + z * z - x * x - y * y  0
		//               0                                 0                               0                1

		Matrix matrix;

		Vec8f two = 2.0f;
		Vec8f x_squared = quaternion.x * quaternion.x;
		Vec8f y_squared = quaternion.y * quaternion.y;
		Vec8f z_squared = quaternion.z * quaternion.z;
		Vec8f w_squared = quaternion.w * quaternion.w;
		Vec8f two_wz = two * quaternion.w * quaternion.z;
		Vec8f two_xy = two * quaternion.x * quaternion.y;
		Vec8f two_xz = two * quaternion.x * quaternion.z;
		Vec8f two_yw = two * quaternion.y * quaternion.w;
		Vec8f two_xw = two * quaternion.x * quaternion.w;
		Vec8f two_yz = two * quaternion.y * quaternion.z;

		Vec8f element_00 = w_squared + x_squared - y_squared - z_squared;
		Vec8f element_01 = two_xy - two_wz;
		Vec8f element_02 = two_yw + two_xz;
		Vec8f element_10 = two_wz + two_xy;
		Vec8f element_11 = w_squared + y_squared - x_squared - z_squared;
		Vec8f element_12 = two_yz - two_xw;
		Vec8f element_20 = two_xz - two_yw;
		Vec8f element_21 = two_xw + two_yz;
		Vec8f element_22 = w_squared + z_squared - x_squared - y_squared;

		element_00 = Broadcast8(element_00, index);
		element_01 = Broadcast8(element_01, index);
		element_02 = Broadcast8(element_02, index);
		element_10 = Broadcast8(element_10, index);
		element_11 = Broadcast8(element_11, index);
		element_12 = Broadcast8(element_12, index);
		element_20 = Broadcast8(element_20, index);
		element_21 = Broadcast8(element_21, index);
		element_22 = Broadcast8(element_22, index);

		Vec8f blend_00_01 = blend8<0, 9, V_DC, V_DC, V_DC, V_DC, V_DC, V_DC>(element_00, element_01);
		Vec8f blend_00_01_02 = blend8<0, 1, 10, V_DC, V_DC, V_DC, V_DC, V_DC>(blend_00_01, element_02);
		Vec8f blend_10_11 = blend8<V_DC, V_DC, V_DC, V_DC, 4, 13, V_DC, V_DC>(element_10, element_11);
		Vec8f blend_10_11_12 = blend8<V_DC, V_DC, V_DC, V_DC, 4, 5, 14, V_DC>(blend_10_11, element_12);
		Vec8f blend_first_two_rows = blend8<0, 1, 2, V_DC, 12, 13, 14, V_DC>(blend_00_01_02, blend_10_11_12);

		Vec8f blend_20_21 = blend8<0, 9, V_DC, V_DC, V_DC, V_DC, V_DC, V_DC>(element_20, element_21);
		Vec8f blend_20_21_22 = blend8<0, 1, 10, V_DC, V_DC, V_DC, V_DC, V_DC>(blend_20_21, element_22);

		matrix.v[0] = blend8<0, 1, 2, 11, 4, 5, 6, 15>(blend_first_two_rows, ZeroVectorFloat());
		matrix.v[1] = ZeroVectorFloat();
		// Blend without the final 1
		matrix.v[1] = blend8<0, 1, 2, 11, 12, 13, 14, 15>(blend_20_21_22, matrix.v[1]);
		// Blend to set the final one
		matrix.v[1] = blend8<0, 1, 2, 3, 4, 5, 6, 15>(matrix.v[1], VectorGlobals::ONE);

		return matrix;
	}

	// -------------------------------------------------------------------------------------------------

	QuaternionScalar ECS_VECTORCALL MatrixToQuaternion(Matrix matrix) {
		return Matrix3x3ToQuaternion(MatrixTo3x3(matrix));
	}

	QuaternionScalar Matrix3x3ToQuaternion(const Matrix3x3& matrix) {
		// Solution based on the article https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf
		QuaternionScalar result;
		float trace = 0.0f;

		float m00 = matrix[0][0];
		float m01 = matrix[0][1];
		float m02 = matrix[0][2];
		float m10 = matrix[1][0];
		float m11 = matrix[1][1];
		float m12 = matrix[1][2];
		float m20 = matrix[2][0];
		float m21 = matrix[2][1];
		float m22 = matrix[2][2];

		if (m22 < 0.0f) {
			if (m00 > m11) {
				trace = 1.0f + m00 - m11 - m22;
				result = QuaternionScalar(trace, m01 + m10, m20 + m02, m12 - m21);
			}
			else {
				trace = 1.0f - m00 + m11 - m22;
				result = QuaternionScalar(m01 + m10, trace, m12 + m21, m20 - m02);
			}
		}
		else {
			if (m00 < -m11) {
				trace = 1.0f - m00 - m11 + m22;
				result = QuaternionScalar(m20 + m02, m12 + m21, trace, m01 - m10);
			}
			else {
				trace = 1.0f + m00 + m11 + m22;
				result = QuaternionScalar(m12 - m21, m20 - m02, m01 - m10, trace);
			}
		}
		
		// PERFORMANCE TODO: Can we use approx_rsqrt?
		result *= 0.5f / sqrt(trace);
		return result;
	}

	void ECS_VECTORCALL MatrixToQuaternion(Matrix matrix, Quaternion* quaternion, size_t index) {
		// We can use the scalar version and then write into the SIMD quaternion at the end
		// If this is really necessary, we can convert to an all SIMD conversion - if need be
		QuaternionScalar scalar_quaternion = MatrixToQuaternion(matrix);
		quaternion->Set(scalar_quaternion, index);
	}


	QuaternionScalar ECS_VECTORCALL MatrixWithScalingToQuaternion(Matrix matrix) {
		return Matrix3x3WithScalingToQuaternion(MatrixTo3x3(matrix));
	}

	QuaternionScalar Matrix3x3WithScalingToQuaternion(const Matrix3x3& matrix) {
		// Since a matrix can contain scale, the vectors cannot be taken as they are
		// So they must be deducted in order to be orthogonal
		float3 up = matrix[1];
		float3 forward = matrix[2];
		float3 right = Cross(up, forward);
		float3 orthogonal_up = Cross(forward, right);
		return QuaternionLookRotation(forward, orthogonal_up);
	}

	void ECS_VECTORCALL MatrixWithScalingToQuaternion(Matrix matrix, Quaternion* quaternion, size_t index) {
		// We can use the scalar version and then write into the SIMD quaternion at the end
		// If this is really necessary, we can convert to an all SIMD conversion - if need be
		QuaternionScalar scalar_quaternion = MatrixWithScalingToQuaternion(matrix);
		quaternion->Set(scalar_quaternion, index);
	}

	// -------------------------------------------------------------------------------------------------

}