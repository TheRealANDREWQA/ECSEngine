#pragma once
#include "Quaternion.h"
#include "Matrix.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {

		// These are placed here because these are a probabily slower way of producing the matrices
		// From quaternions. Can investigate which is faster

		ECS_INLINE Matrix ECS_VECTORCALL QuaternionToMatrixLowMultiplication(Quaternion quaternion) {
			// Make sure that the quaternion has the low and high
			quaternion.value = SplatLowLane(quaternion.value);

			Vector8 right = RightVector();
			Vector8 up = UpVector();
			Vector8 forward = ForwardVector();

			Vector8 right_up_shuffle = Permute2f128Helper<0, 2>(right, up);
			Vector8 rotated_right_up = QuaternionVectorMultiply(quaternion, right_up_shuffle);
			Vector8 rotated_forward = QuaternionVectorMultiply(quaternion, forward);

			Vector8 zero = ZeroVector();
			rotated_right_up = PerLaneBlend<0, 1, 2, 7>(rotated_right_up, zero);
			rotated_forward = PerLaneBlend<0, 1, 2, 7>(rotated_forward, zero);

			Vector8 quat_identity = QuaternionIdentity().value;
			return Matrix(
				rotated_right_up,
				BlendLowAndHigh(rotated_forward, quat_identity)
			);
		}

		ECS_INLINE void ECS_VECTORCALL QuaternionToMatrixMultiplication(Quaternion quaternion, Matrix* low, Matrix* high) {
			Vector8 right = QuaternionVectorMultiply(quaternion, RightVector());
			Vector8 up = QuaternionVectorMultiply(quaternion, UpVector());
			Vector8 forward = QuaternionVectorMultiply(quaternion, ForwardVector());

			Vector8 zero = ZeroVector();
			// zero the last element of each vector
			right = PerLaneBlend<0, 1, 2, 7>(right, zero);
			up = PerLaneBlend<0, 1, 2, 7>(up, zero);
			forward = PerLaneBlend<0, 1, 2, 7>(forward, zero);

			Vector8 quat_identity = QuaternionIdentity().value;
			*low = Matrix(
				Permute2f128Helper<0, 2>(right, up),
				PerLaneBlend<0, 1, 2, 3>(forward, quat_identity)
			);

			*low = Matrix(
				Permute2f128Helper<1, 3>(right, up),
				Permute2f128Helper<1, 3>(forward, quat_identity)
			);
		}

		ECS_INLINE Matrix ECS_VECTORCALL QuaternionRotationMatrixMultiplication(float3 rotation) {
			return QuaternionToMatrixLowMultiplication(QuaternionFromEuler(rotation));
		}

		ECS_INLINE void ECS_VECTORCALL QuaternionRotationMatrixMultiplication(float3 rotation1, float3 rotation2, Matrix* low, Matrix* high) {
			QuaternionToMatrixMultiplication(QuaternionFromEuler(rotation1, rotation2), low, high);
		}

		// This is for the method that computes using component multiplications
		ECS_INLINE void ECS_VECTORCALL QuaternionToMatrixVectors(Quaternion quaternion, Vector8* diagonal, Vector8* upper_three, Vector8* lower_three) {
			// The formula is this one (on the internet the values are provided in column major format, here they are row major)
			// w * w + x * x - y * y - z * z          2 w * z + 2 x * y               2 x * z - 2 y * w         0
			//       2 x * y - 2 z * w          w * w + y * y - x * x - z * z         2 x * w + 2 y * z         0
			//       2 w * y + 2 x * z                2 y * z - 2 x * w         w * w + z * z - x * x - y * y   0
			//               0                                0                               0                 1
			Vector8 squared = quaternion.value * quaternion.value;

			// Construct the elements on the main diagonal
			Vector8 diagonal_first_permutation = PerLanePermute<3, 3, 3, V_DC>(squared);
			// They are already in order here
			Vector8 diagonal_second_permutation = squared;
			Vector8 diagonal_third_permutation = PerLanePermute<1, 0, 0, V_DC>(squared);
			Vector8 diagonal_fourth_permutation = PerLanePermute<2, 2, 1, V_DC>(squared);
			*diagonal = diagonal_first_permutation + diagonal_second_permutation - diagonal_third_permutation - diagonal_fourth_permutation;

			// Now calculate the upper three elements and the lower three elements separately
			Vector8 two = 2.0f;

			// Upper three elements
			Vector8 upper_three_first = PerLanePermute<0, 3, 0, V_DC>(quaternion.value);
			Vector8 upper_three_second = PerLanePermute<1, 2, 2, V_DC>(quaternion.value);
			Vector8 upper_three_third = PerLanePermute<2, 0, 1, V_DC>(quaternion.value);
			Vector8 upper_three_fourth = PerLanePermute<3, 1, 3, V_DC>(quaternion.value);
			// Can use Fmsubadd in order to avoid the change sign operation in order for the second value to be subtracted
			*upper_three = Fmsubadd(upper_three_first, upper_three_second, upper_three_third * upper_three_fourth);
			*upper_three = *upper_three * two;

			// Lower three elements
			Vector8 lower_three_first = PerLanePermute<3, 1, 0, V_DC>(quaternion.value);
			Vector8 lower_three_second = PerLanePermute<1, 2, 3, V_DC>(quaternion.value);
			Vector8 lower_three_third = PerLanePermute<0, 0, 1, V_DC>(quaternion.value);
			Vector8 lower_three_fourth = PerLanePermute<2, 3, 2, V_DC>(quaternion.value);
			// Can use Fmaddsub in order to avoid the change sign operation in order for the second value to be added
			*lower_three = Fmaddsub(lower_three_first, lower_three_second, lower_three_third * lower_three_fourth);
			*lower_three = *lower_three * two;
		}

	}

	static Matrix ECS_VECTORCALL QuaternionToMatrixLow(Quaternion quaternion) {
		// The formula is this one (on the internet the values are provided in column major format, here they are row major)
		// w * w + x * x - y * y - z * z          2 w * z + 2 x * y               2 x * z - 2 y * w         0
		//       2 x * y - 2 z * w          w * w + y * y - x * x - z * z         2 x * w + 2 y * z         0
		//       2 w * y + 2 x * z                2 y * z - 2 x * w         w * w + z * z - x * x - y * y   0
		//               0                                0                               0                 1

		quaternion = QuaternionNormalize(quaternion);
		Vector8 zero = ZeroVector();
		Vector8 quat_identity = LastElementOneVector();
		
		Vector8 diagonal, upper_three, lower_three;
		SIMDHelpers::QuaternionToMatrixVectors(quaternion, &diagonal, &upper_three, &lower_three);

		Vector8 diagonal_low_to_high = Permute2f128Helper<1, 0>(diagonal, diagonal);
		Vector8 upper_three_low_to_high = Permute2f128Helper<1, 0>(upper_three, upper_three);
		Vector8 lower_three_low_to_high = Permute2f128Helper<1, 0>(lower_three, lower_three);

		Vector8 matrix_low_4_0 = PerLaneBlend<0, 5, 6, V_DC>(diagonal, upper_three);
		Vector8 matrix_high_4_0_temp = PerLaneBlend<0, 5, V_DC, V_DC>(upper_three_low_to_high, diagonal_low_to_high);
		Vector8 matrix_high_4_0 = PerLaneBlend<0, 1, 6, V_DC>(matrix_high_4_0_temp, lower_three_low_to_high);
		Vector8 matrix_0 = BlendLowAndHigh(matrix_low_4_0, matrix_high_4_0);

		Vector8 matrix_low_12_0 = PerLaneBlend<0, 1, 6, V_DC>(lower_three, diagonal);
		Vector8 matrix_1_temp = PerLaneBlend<0, 1, 2, 7>(matrix_low_12_0, zero);

		// Now set with 0 the last column of the matrix values
		matrix_0 = PerLaneBlend<0, 1, 2, 7>(matrix_0, zero);
		Vector8 matrix_1 = BlendLowAndHigh(matrix_1_temp, quat_identity);

		return { matrix_0, matrix_1 };
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, Matrix* low, Matrix* high) {
		// The formula is this one (on the internet the values are provided in column major format, here they are row major)
		// w * w + x * x - y * y - z * z           2 x * y - 2 z * w               2 w * y + 2 x * z        0
		//       2 w * z + 2 x * y           w * w + y * y - x * x - z * z         2 y * z - 2 x * w        0
		//       2 x * z - 2 y * w                 2 x * w + 2 y * z         w * w + z * z - x * x - y * y  0
		//               0                                 0                               0                1

		Vector8 zero = ZeroVector();
		Vector8 quat_identity = LastElementOneVector();

		Vector8 diagonal, upper_three, lower_three;
		SIMDHelpers::QuaternionToMatrixVectors(quaternion, &diagonal, &upper_three, &lower_three);

		// Calculate the first 2 rows - do the calculations for both lanes
		Vector8 matrix_low_4_0 = PerLaneBlend<0, 5, 6, V_DC>(diagonal, upper_three);
		Vector8 matrix_high_4_0_temp = PerLaneBlend<0, 5, V_DC, V_DC>(upper_three, diagonal);
		Vector8 matrix_high_4_0 = PerLaneBlend<0, 1, 6, V_DC>(matrix_high_4_0_temp, lower_three);

		matrix_low_4_0 = PerLaneBlend<0, 1, 2, 7>(matrix_low_4_0, zero);
		matrix_high_4_0 = PerLaneBlend<0, 1, 2, 7>(matrix_high_4_0, zero);
		Vector8 matrix_low_first = Permute2f128Helper<0, 2>(matrix_low_4_0, matrix_high_4_0);
		Vector8 matrix_high_first = Permute2f128Helper<1, 3>(matrix_low_4_0, matrix_high_4_0);

		// Calculate the 3rd row
		Vector8 matrix_low_12_0 = PerLaneBlend<0, 1, 6, V_DC>(lower_three, diagonal);
		matrix_low_12_0 = PerLaneBlend<0, 1, 2, 7>(matrix_low_12_0, zero);

		Vector8 matrix_low_second = BlendLowAndHigh(matrix_low_12_0, quat_identity);
		Vector8 matrix_high_second = BlendLowAndHigh(quat_identity, matrix_low_12_0);

		*low = { matrix_low_first, matrix_low_second };
		*high = { matrix_high_first, matrix_high_second };
	}

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation) {
		return QuaternionToMatrixLow(QuaternionFromEuler(rotation));
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation1, float3 rotation2, Matrix* low, Matrix* high) {
		QuaternionToMatrix(QuaternionFromEuler(rotation1, rotation2), low, high);
	}

	// -------------------------------------------------------------------------------------------------

	// Only the low is a valid quaternion
	ECS_INLINE Quaternion ECS_VECTORCALL MatrixToQuaternion(Matrix matrix) {
		// Since a matrix can contain scale, the vectors cannot be taken as they are
		// So they must be deducted in order to be orthogonal

		// This seems reduntant, we already perform a normalization inside the quaternion look rotation
		/*Vector8 right_up = NormalizeIfNot(matrix.v[0]);
		Vector8 forward = NormalizeIfNot(matrix.v[1]);*/

		Vector8 right_up = matrix.v[0];
		Vector8 forward = matrix.v[1];

		Vector8 up_right = Permute2f128Helper<1, 0>(right_up, right_up);
		Vector8 right = Cross(up_right, forward);
		Vector8 orthogonal_up = Cross(forward, right);
		return QuaternionLookRotation(forward, orthogonal_up);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL MatrixToQuaternion(Matrix matrix1, Matrix matrix2) {
		// Since a matrix can contain scale, the vectors cannot be taken as they are
		// So they must be deducted in order to be orthogonal

		Vector8 ups = Permute2f128Helper<1, 3>(matrix1.v[0], matrix2.v[0]);
		Vector8 forwards = Permute2f128Helper<0, 2>(matrix1.v[1], matrix2.v[1]);

		// This seems reduntant, we already perform a normalization inside the quaternion look rotation
		/*ups = NormalizeIfNot(ups);
		forwards = NormalizeIfNot(forwards);*/

		Vector8 rights = Cross(ups, forwards);
		Vector8 orthogonal_up = Cross(forwards, rights);
		return QuaternionLookRotation(forwards, orthogonal_up);
	}

	// -------------------------------------------------------------------------------------------------

	// RotationStructure can only be a Quaternion or Matrix
	template<typename RotationStructure>
	ECS_INLINE Vector8 ECS_VECTORCALL RotateVector(Vector8 vector, RotationStructure rotation_structure) {
		if constexpr (std::is_same_v<RotationStructure, Quaternion>) {
			return RotateVectorQuaternionSIMD(vector, rotation_structure);
		}
		else if constexpr (std::is_same_v<RotationStructure, Matrix>) {
			return RotateVectorMatrixSIMD(vector, rotation_structure);
		}
		else {
			static_assert(false, "Invalid RotationStructure for RotateVector. It can only be a Matrix or Quaternion");
		}
	}

	// RotationStructure can only be a Quaternion or Matrix
	template<typename RotationStructure>
	ECS_INLINE Vector8 ECS_VECTORCALL RotatePoint(Vector8 point, RotationStructure rotation_structure) {
		if constexpr (!std::is_same_v<RotationStructure, Quaternion> && !std::is_same_v<RotationStructure, Matrix>) {
			static_assert(fasel, "Invalid RotationStructure for RotatePoint. It can only be a Matrix or Quaternion");
		}
		return RotateVector(point, rotation_structure);
	}

	// -------------------------------------------------------------------------------------------------

}