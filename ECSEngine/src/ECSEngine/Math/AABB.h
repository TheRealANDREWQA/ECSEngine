#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"
#include "Vector.h"
#include "Matrix.h"
#include "Quaternion.h"

namespace ECSEngine {

	struct AABBStorage {
		float3 min;
		float3 max;
	};

	struct AABB {
		ECS_INLINE AABB() {}
		ECS_INLINE AABB(float3 min, float3 max) : value(min, max) {}
		ECS_INLINE AABB(AABBStorage storage) : value((float3*)&storage) {}
		ECS_INLINE AABB(Vector8 _value) : value(_value) {}
		
		ECS_INLINE operator Vector8() const {
			return value;
		}

		ECS_INLINE AABBStorage ToStorage() const {
			AABBStorage aabb;
			value.StoreFloat3((float3*)&aabb);
			return aabb;
		}

		Vector8 value;
	};

	// This is the same as an OOBB but we haven't decided upon the OOBB
	// Representation yet. Use this as a placeholder
	// This structure is maintained as a list of 4 vertices
	// Where 3 belong to the front face and 1 to the back face
	// The 3 vertices are the top left, bottom right and bottom left corners
	// The back vertex is the bottom left coner
	struct RotatedAABB {
		Vector8 bottom_left_top_right_front;
		Vector8 bottom_left_back_bottom_right_front;
	};

	// Returns the AABBStorage that encompases all of them
	ECSENGINE_API AABBStorage GetCoalescedAABB(Stream<AABBStorage> aabbs);

	ECSENGINE_API AABBStorage GetPointsBoundingBox(Stream<float3> points);

	ECSENGINE_API AABBStorage GetCombinedBoundingBox(AABBStorage first, AABBStorage second);

	ECS_INLINE AABBStorage InfiniteBoundingBox() {
		return { float3::Splat(-FLT_MAX), float3::Splat(FLT_MAX) };
	}

	// Can use this to initialized the bounding box when calculating it from points
	ECS_INLINE AABBStorage ReverseInfiniteBoundingBox() {
		return { float3::Splat(FLT_MAX), float3::Splat(-FLT_MAX) };
	}

	ECS_INLINE bool CompareAABB(AABBStorage first, AABBStorage second, float3 epsilon = float3::Splat(0.0001f)) {
		return BasicTypeFloatCompareBoolean(first.min, first.min, epsilon) && BasicTypeFloatCompareBoolean(first.max, second.max, epsilon);
	}

	ECS_INLINE AABB TranslateAABB(AABB aabb, Vector8 translation) {
		return aabb.value + translation;
	}

	ECS_INLINE float3 AABBCenter(AABBStorage aabb) {
		return aabb.min + (aabb.max - aabb.min) * float3::Splat(0.5f);
	}

	ECS_INLINE Vector8 AABBCenter(AABB aabb) {
		Vector8 shuffle_min = Permute2f128Helper<0, 0>(aabb.value, aabb.value);
		Vector8 shuffle_max = Permute2f128Helper<1, 1>(aabb.value, aabb.value);
		Vector8 half = 0.5f;
		return Fmadd(shuffle_max - shuffle_min, half, shuffle_min);
	}

	ECS_INLINE Vector8 AABBExtents(AABB aabb) {
		Vector8 shuffle = Permute2f128Helper<1, 0>(aabb.value, aabb.value);
		// Broadcast the low lane to the upper lane
		Vector8 extents = shuffle - aabb.value;
		return Permute2f128Helper<0, 0>(extents, extents);
	}

	ECS_INLINE RotatedAABB AABBToRotatedAABBPoints(AABB aabb) {
		Vector8 bottom_right_front = aabb.value;
		Vector8 extents = AABBExtents(aabb);
		Vector8 zero_vector = ZeroVector();
		Vector8 x = PerLaneBlend <
	}

	ECS_INLINE RotatedAABB RotateAABB(AABB aabb, Matrix rotation_matrix) {
		// Rotate the 4 points accordingly
		RotatedAABB rotated_aabb = AABBToRotatedAABBPoints(aabb);
		return TransformPoint()
	}

	ECS_INLINE RotatedAABB RotateAABB(AABB aabb, Quaternion rotation) {

	}

}