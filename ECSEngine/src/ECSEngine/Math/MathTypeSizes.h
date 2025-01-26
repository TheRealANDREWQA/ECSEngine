#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {
	
	// This header was primarly created to allow the reflection manager add blittable exception for these
	// Types but without having to include them directly. We include this header and the .cpp will have the
	// necessary information

	enum ECS_MATH_STRUCTURE_TYPE : unsigned char {
		ECS_MATH_STRUCTURE_VECTOR3,
		ECS_MATH_STRUCTURE_VECTOR4,
		ECS_MATH_STRUCTURE_MATRIX,
		ECS_MATH_STRUCTURE_QUATERNION,
		ECS_MATH_STRUCTURE_QUATERNION_SCALAR,
		ECS_MATH_STRUCTURE_PLANE,
		ECS_MATH_STRUCTURE_PLANE_SCALAR,
		ECS_MATH_STRUCTURE_VECTOR_TRANSFORM,
		ECS_MATH_STRUCTURE_VECTOR_TRANSFORM_SCALAR,
		ECS_MATH_STRUCTURE_AABB,
		ECS_MATH_STRUCTURE_AABB_SCALAR,
		ECS_MATH_STRUCTURE_MATRIX3x3,
		ECS_MATH_STRUCTURE_TYPE_COUNT
	};

	struct MathStructureInfo {
		Stream<char> name;
		unsigned int byte_size;
		unsigned int alignment;
	};

	// The number of entries is given by ECS_MATH_STRUCTURE_COUNT
	ECSENGINE_API extern MathStructureInfo ECS_MATH_STRUCTURE_TYPE_INFOS[];

}