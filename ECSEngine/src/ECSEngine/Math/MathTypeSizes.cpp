#include "ecspch.h"
#include "MathTypeSizes.h"
#include "Vector.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "Transform.h"
#include "Plane.h"
#include "AABB.h"

namespace ECSEngine {

	Stream<char> ECS_MATH_STRUCTURE_TYPE_STRINGS[] = {
		STRING(Vector8),
		STRING(Matrix),
		STRING(Quaternion),
		STRING(Plane),
		STRING(VectorTransform),
		STRING(AABB),
	};

	static_assert(ECS_MATH_STRUCTURE_TYPE_COUNT == std::size(ECS_MATH_STRUCTURE_TYPE_STRINGS));

	size_t ECS_MATH_STRUCTURE_TYPE_BYTE_SIZES[] = {
		sizeof(Vector8),
		sizeof(Matrix),
		sizeof(Quaternion),
		sizeof(Plane),
		sizeof(VectorTransform),
		sizeof(AABB),
	};

	static_assert(ECS_MATH_STRUCTURE_TYPE_COUNT == std::size(ECS_MATH_STRUCTURE_TYPE_BYTE_SIZES));

}