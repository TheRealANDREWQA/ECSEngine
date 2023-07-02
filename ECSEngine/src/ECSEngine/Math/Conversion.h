#pragma once
#include "Quaternion.h"
#include "Matrix.h"

namespace ECSEngine {

#pragma region Quaternion to Matrix

	// -------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion) {
		Vector4 right = QuaternionVectorMultiply(quaternion, VectorGlobals::RIGHT_4);
		Vector4 up = QuaternionVectorMultiply(quaternion, VectorGlobals::UP_4);
		Vector4 forward = QuaternionVectorMultiply(quaternion, VectorGlobals::FORWARD_4);

		Vector4 zero = ZeroVector4();
		// zero the last element of each vector
		right = blend4<0, 1, 2, 7>(right, zero);
		up = blend4<0, 1, 2, 7>(up, zero);
		forward = blend4<0, 1, 2, 7>(forward, zero);

		return Matrix(right, up, forward, VectorGlobals::QUATERNION_IDENTITY_4);
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionToMatrix(PackedQuaternion quaternion, Matrix& first, Matrix& second) {
		Vector8 right = QuaternionVectorMultiply(quaternion, VectorGlobals::RIGHT_8);
		Vector8 up = QuaternionVectorMultiply(quaternion, VectorGlobals::UP_8);
		Vector8 forward = QuaternionVectorMultiply(quaternion, VectorGlobals::FORWARD_8);

		Vector8 zero = ZeroVector8();

		// zero the last element of low and high
		right = blend8<0, 1, 2, 11, 4, 5, 6, 15>(right, zero);
		up = blend8<0, 1, 2, 11, 4, 5, 6, 15>(up, zero);
		forward = blend8<0, 1, 2, 11, 4, 5, 6, 15>(forward, zero);

		first = Matrix(right.Low(), up.Low(), forward.Low(), VectorGlobals::QUATERNION_IDENTITY_4);
		second = Matrix(right.High(), up.High(), forward.High(), VectorGlobals::QUATERNION_IDENTITY_4);
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionToMatrix(PackedQuaternion quaternion, Matrix* matrices) {
		QuaternionToMatrix(quaternion, matrices[0], matrices[1]);
	}

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation) {
		return QuaternionToMatrix(QuaternionFromRotation(rotation));
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation1, float3 rotation2, Matrix& first, Matrix& second) {
		QuaternionToMatrix(QuaternionFromRotation(rotation1, rotation2), first, second);
	}

	// -------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Matrix to quaternion

	// -------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL MatrixToQuaternion(Matrix matrix) {
		// Since a matrix can contain scale, the vectors cannot be taken as they are
		// So they must be deducted in order to be orthogonal
		Vector8 right_up_normalized = Normalize3(matrix.v[0]);
		Vector8 forward_normalized = Normalize3(matrix.v[1]);

		Vector8 up_right_normalized = permute8<4, 5, 6, 7, 0, 1, 2, 3>(right_up_normalized);
		Vector8 right = Cross(up_right_normalized, forward_normalized);
		Vector8 orthogonal_up = Cross(forward_normalized, right);
		return QuaternionLookRotation(forward_normalized.Low(), orthogonal_up.Low());
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL MatrixToQuaternion(Matrix matrix1, Matrix matrix2) {
		// Since a matrix can contain scale, the vectors cannot be taken as they are
		// So they must be deducted in order to be orthogonal

		Vector8 ups = blend8<4, 5, 6, 7, 12, 13, 14, 15>(matrix1.v[0], matrix2.v[0]);
		Vector8 forwards = blend8<0, 1, 2, 3, 8, 9, 10, 11>(matrix1.v[1], matrix2.v[1]);

		ups = Normalize3(ups);
		forwards = Normalize3(forwards);

		Vector8 rights = Cross(ups, forwards);
		Vector8 orthogonal_up = Cross(forwards, rights);
		return QuaternionLookRotation(forwards, orthogonal_up);
	}

	// -------------------------------------------------------------------------------------------------

#pragma endregion

	// -------------------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------------------

}