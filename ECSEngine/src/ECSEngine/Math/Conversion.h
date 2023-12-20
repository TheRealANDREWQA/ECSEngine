#pragma once
#include "Quaternion.h"
#include "Matrix.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL QuaternionToMatrixLow(Quaternion quaternion);

	ECSENGINE_API void ECS_VECTORCALL QuaternionToMatrix(Quaternion quaternion, Matrix* low, Matrix* high);

	ECS_INLINE Matrix ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation) {
		return QuaternionToMatrixLow(QuaternionFromEuler(rotation));
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionRotationMatrix(float3 rotation1, float3 rotation2, Matrix* low, Matrix* high) {
		QuaternionToMatrix(QuaternionFromEuler(rotation1, rotation2), low, high);
	}

	// -------------------------------------------------------------------------------------------------

	// Only the low is a valid quaternion
	ECSENGINE_API Quaternion ECS_VECTORCALL MatrixToQuaternion(Matrix matrix);

	ECSENGINE_API Quaternion ECS_VECTORCALL MatrixToQuaternion(Matrix matrix1, Matrix matrix2);

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