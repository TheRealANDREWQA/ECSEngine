#include "ecspch.h"
#include "MathTypeSizes.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "Transform.h"
#include "Plane.h"
#include "AABB.h"

namespace ECSEngine {

	Stream<char> ECS_MATH_STRUCTURE_TYPE_STRINGS[] = {
		STRING(Vector3),
		STRING(Matrix),
		STRING(Quaternion),
		STRING(Plane),
		STRING(Transform),
		STRING(AABB),
	};

	static_assert(ECS_MATH_STRUCTURE_TYPE_COUNT == ECS_COUNTOF(ECS_MATH_STRUCTURE_TYPE_STRINGS));

	size_t ECS_MATH_STRUCTURE_TYPE_BYTE_SIZES[] = {
		sizeof(Vector3),
		sizeof(Matrix),
		sizeof(Quaternion),
		sizeof(Plane),
		sizeof(Transform),
		sizeof(AABB),
	};

	static_assert(ECS_MATH_STRUCTURE_TYPE_COUNT == ECS_COUNTOF(ECS_MATH_STRUCTURE_TYPE_BYTE_SIZES));

}