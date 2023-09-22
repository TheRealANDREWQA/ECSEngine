#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {
	
	// This header was primarly created to allow the reflection manager add blittable exception for these
	// Types but without having to include them directly. We include this header and the .cpp will have the
	// necessary information

	enum ECS_MATH_STRUCTURE_TYPE : unsigned char {
		ECS_MATH_STRUCTURE_VECTOR8,
		ECS_MATH_STRUCTURE_MATRIX,
		ECS_MATH_STRUCTURE_QUATERNION,
		ECS_MATH_STRUCTURE_PLANE,
		ECS_MATH_STRUCTURE_VECTOR_TRANSFORM,
		ECS_MATH_STRUCTURE_AABB,
		ECS_MATH_STRUCTURE_TYPE_COUNT
	};

	// The number of entries is given by ECS_MATH_STRUCTURE_COUNT
	ECSENGINE_API extern Stream<char> ECS_MATH_STRUCTURE_TYPE_STRINGS[];

	// The number of entries is given by ECS_MATH_STRUCTURE_COUNT
	ECSENGINE_API extern size_t ECS_MATH_STRUCTURE_TYPE_BYTE_SIZES[];

	// At the moment, leave the math structure type alignment to 8 even tho they are SIMD types
	ECS_INLINE size_t MathStructureAlignment() {
		return alignof(void*);
	}

}