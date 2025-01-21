#pragma once
#include "Quaternion.h"
#include "Matrix.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL QuaternionToMatrix(QuaternionScalar quaternion);

	ECSENGINE_API Matrix3x3 ECS_VECTORCALL QuaternionToMatrix3x3(QuaternionScalar quaternion);

	// Converts the quaternion at the given index to a matrix
	ECSENGINE_API Matrix ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, size_t index);

	// There needs to be enough space in the matrices pointer as write count
	ECSENGINE_API void ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, Matrix* matrices, size_t write_count);

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation) {
		return QuaternionToMatrix(QuaternionFromEuler(rotation));
	}

	// -------------------------------------------------------------------------------------------------

	// The resulting quaternion is normalized. If the matrix has scaling, if might not produce an accurate result
	ECSENGINE_API QuaternionScalar ECS_VECTORCALL MatrixToQuaternion(Matrix matrix);

	// The resulting quaternion is normalized. If the matrix has scaling, if might not produce an accurate result
	ECSENGINE_API QuaternionScalar Matrix3x3ToQuaternion(const Matrix3x3& matrix);

	// Converts the matrix to a quaternion at the given index
	// The resulting quaternion is normalized. If the matrix has scaling, if might not produce an accurate result
	ECSENGINE_API void ECS_VECTORCALL MatrixToQuaternion(Matrix matrix, Quaternion* quaternion, size_t index);

	// The resulting quaternion is normalized. This function handles better the case when the matrix has scaling
	// Applied to it, as opposed to the normal function
	ECSENGINE_API QuaternionScalar ECS_VECTORCALL MatrixWithScalingToQuaternion(Matrix matrix);

	// The resulting quaternion is normalized. This function handles better the case when the matrix has scaling
	// Applied to it, as opposed to the normal function
	ECSENGINE_API QuaternionScalar Matrix3x3WithScalingToQuaternion(const Matrix3x3& matrix);

	// Converts the matrix to a quaternion at the given index
	// The resulting quaternion is normalized. This function handles better the case when the matrix has scaling
	// Applied to it, as opposed to the normal function
	ECSENGINE_API void ECS_VECTORCALL MatrixWithScalingToQuaternion(Matrix matrix, Quaternion* quaternion, size_t index);

	// -------------------------------------------------------------------------------------------------

}