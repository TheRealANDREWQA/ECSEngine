#pragma once
#include "Quaternion.h"
#include "Matrix.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL QuaternionToMatrix(QuaternionScalar quaternion);

	// Converts the quaternion at the given index to a matrix
	ECSENGINE_API Matrix ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, size_t index);

	// There needs to be enough space in the matrices pointer as write count
	ECSENGINE_API void ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, Matrix* matrices, size_t write_count);

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation) {
		return QuaternionToMatrix(QuaternionFromEuler(rotation));
	}

	// -------------------------------------------------------------------------------------------------

	// The resulting quaternion is normalized
	ECSENGINE_API QuaternionScalar ECS_VECTORCALL MatrixToQuaternion(Matrix matrix);

	// Converts the matrix to a quaternion at the given index
	// The resulting quaternion is normalized
	ECSENGINE_API void ECS_VECTORCALL MatrixToQuaternion(Matrix matrix, Quaternion* quaternion, size_t index);

	// -------------------------------------------------------------------------------------------------

}