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
		ENTRY(Vector4),
		ENTRY(Matrix),
		ENTRY(Quaternion),
		ENTRY(QuaternionScalar),
		ENTRY(Plane),
		ENTRY(PlaneScalar),
		ENTRY(Transform),
		ENTRY(TransformScalar),
		ENTRY(AABB),
		ENTRY(AABBScalar),
		ENTRY(Matrix3x3)
	};

	static_assert(ECS_MATH_STRUCTURE_TYPE_COUNT == ECS_COUNTOF(ECS_MATH_STRUCTURE_TYPE_INFOS));

#undef ENTRY

}