#pragma once
#include "Quaternion.h"
#include "Matrix.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionToMatrixLow(Quaternion quaternion) {
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

	ECS_INLINE void ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, Matrix* low, Matrix* high) {
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

		// This seems reduntant seems we already perform a normalization inside the quaternion look rotation
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

		// This seems reduntant seems we already perform a normalization inside the quaternion look rotation
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