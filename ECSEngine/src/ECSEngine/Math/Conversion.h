#pragma once
#include "Quaternion.h"
#include "Matrix.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionToMatrixLow(Quaternion quaternion) {
		// Make sure that the quaternion has the low and high
		quaternion.value = Permute2f128Helper<0, 0>(quaternion.value, quaternion.value);

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
			blend8<0, 1, 2, 3, 12, 13, 14, 15>(rotated_forward, quat_identity)
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
		Vector8 right_up_normalized = NormalizeIfNot(matrix.v[0]);
		Vector8 forward_normalized = NormalizeIfNot(matrix.v[1]);

		Vector8 up_right_normalized = Permute2f128Helper<1, 0>(right_up_normalized, right_up_normalized);
		Vector8 right = Cross(up_right_normalized, forward_normalized);
		Vector8 orthogonal_up = Cross(forward_normalized, right);
		return QuaternionLookRotation(forward_normalized, orthogonal_up);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL MatrixToQuaternion(Matrix matrix1, Matrix matrix2) {
		// Since a matrix can contain scale, the vectors cannot be taken as they are
		// So they must be deducted in order to be orthogonal

		Vector8 ups = Permute2f128Helper<1, 3>(matrix1.v[0], matrix2.v[0]);
		Vector8 forwards = Permute2f128Helper<0, 2>(matrix1.v[1], matrix2.v[1]);

		ups = NormalizeIfNot(ups);
		forwards = NormalizeIfNot(forwards);

		Vector8 rights = Cross(ups, forwards);
		Vector8 orthogonal_up = Cross(forwards, rights);
		return QuaternionLookRotation(forwards, orthogonal_up);
	}

	// -------------------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------------------

}