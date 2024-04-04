#include "ecspch.h"
#include "MathTypeSizes.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "Transform.h"
#include "Plane.h"
#include "AABB.h"

namespace ECSEngine {

#define ENTRY(type) { STRING(type), (unsigned int)sizeof(type), (unsigned int)alignof(type) }

	MathStructureInfo ECS_MATH_STRUCTURE_TYPE_INFOS[] = {
		ENTRY(Vector3),
		ENTRY(Matrix),
		ENTRY(Quaternion),
		ENTRY(Plane),
		ENTRY(Transform),
		ENTRY(AABB),
		ENTRY(Matrix3x3)
	};

	static_assert(ECS_MATH_STRUCTURE_TYPE_COUNT == ECS_COUNTOF(ECS_MATH_STRUCTURE_TYPE_INFOS));

#undef ENTRY

}