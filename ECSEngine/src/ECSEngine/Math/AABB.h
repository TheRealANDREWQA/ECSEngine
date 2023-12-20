#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"
#include "Conversion.h"

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
	struct RotatedAABB {
		// Low lane center, high lane extents
		Vector8 center_extents;
		Vector8 local_axis_xy;
		// The axis is splatted in both lanes
		Vector8 local_axis_z;
	};

	// Returns the AABBStorage that encompases all of them
	ECSENGINE_API AABBStorage GetCoalescedAABB(Stream<AABBStorage> aabbs);

	ECSENGINE_API AABBStorage GetAABBFromPoints(Stream<float3> points);

	ECSENGINE_API AABBStorage GetCombinedAABBStorage(AABBStorage first, AABBStorage second);

	ECS_INLINE AABBStorage InfiniteAABBStorage() {
		return { float3::Splat(-FLT_MAX), float3::Splat(FLT_MAX) };
	}

	// Can use this to initialized the bounding box when calculating it from points
	ECS_INLINE AABBStorage ReverseInfiniteAABBStorage() {
		return { float3::Splat(FLT_MAX), float3::Splat(-FLT_MAX) };
	}

	ECSENGINE_API bool CompareAABBStorage(AABBStorage first, AABBStorage second, float3 epsilon = float3::Splat(ECS_SIMD_VECTOR_EPSILON_VALUE));

	ECS_INLINE float3 AABBCenterStorage(AABBStorage aabb) {
		return aabb.min + (aabb.max - aabb.min) * float3::Splat(0.5f);
	}

	ECSENGINE_API Vector8 ECS_VECTORCALL AABBCenter(AABB aabb);

	ECSENGINE_API Vector8 ECS_VECTORCALL AABBExtents(AABB aabb);

	ECSENGINE_API Vector8 ECS_VECTORCALL AABBHalfExtents(AABB aabb);

	// The translation value needs to be same in the low and high lanes
	ECS_INLINE AABB ECS_VECTORCALL TranslateAABB(AABB aabb, Vector8 translation) {
		return aabb.value + translation;
	}

	// It does not rotate the center of the AABB, it only rotates the corners
	// With respect to the center of the AABB
	ECSENGINE_API RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Matrix rotation_matrix);

	// It does not rotate the center of the AABB, it only rotates the corners
	// With respect to the center of the AABB
	ECSENGINE_API RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Quaternion rotation);

	// Computes the corresponding AABB from the given rotation matrix
	// If you want to use a quaternion, you must convert it to a rotation matrix first
	// And then call this function
	ECSENGINE_API AABB ECS_VECTORCALL AABBFromRotation(AABB aabb, Matrix rotation_matrix);

	// The low and high lanes of the scale need to be the same
	// This one scales the dimensions of the aabb (the center stays the same)
	ECSENGINE_API AABB ECS_VECTORCALL ScaleAABB(AABB aabb, Vector8 scale);

	// The low and high lanes of the scale need to be the same
	// This version scales the values from the origin (it is as if the
	// min and max are multiplied with the factor scale)
	ECS_INLINE AABB ECS_VECTORCALL ScaleAABBFromOrigin(AABB aabb, Vector8 scale) {
		return aabb.value * scale;
	}

	// Returns an encompasing AABB from the given transformation
	// Only the low for translation and scale are considered
	ECSENGINE_API AABB ECS_VECTORCALL TransformAABB(AABB aabb, Vector8 translation, Matrix rotation_matrix, Vector8 scale);

	// Applies the matrix to both the min and max
	ECS_INLINE Vector8 ECS_VECTORCALL ApplyMatrixOnAABB(AABB aabb, Matrix matrix) {
		return TransformPoint(aabb.value, matrix);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL CompareAABBMask(AABB first, AABB second, Vector8 epsilon = VectorGlobals::EPSILON) {
		return CompareMask(first.value, second.value, epsilon);
	}

	ECS_INLINE bool ECS_VECTORCALL CompareAABB(AABB first, AABB second, Vector8 epsilon = VectorGlobals::EPSILON) {
		return CompareWhole(first.value, second.value, epsilon);
	}

	ECSENGINE_API AABB ECS_VECTORCALL GetCombinedAABB(AABB first, AABB second);

	ECS_INLINE AABB ECS_VECTORCALL InfiniteAABB() {
		return InfiniteAABBStorage();
	}

	ECS_INLINE AABB ECS_VECTORCALL ReverseInfiniteAABB() {
		return ReverseInfiniteAABBStorage();
	}

	ECSENGINE_API bool ECS_VECTORCALL AABBOverlap(AABB first, AABB second);

	ECSENGINE_API bool AABBOverlapStorage(AABBStorage first, AABBStorage second);

	// There need to be 4 entries for the corners pointer
	// The first 2 entries are the "left" face and the other
	// 2 are for the "right" face
	ECSENGINE_API void GetAABBCorners(AABB aabb, Vector8* corners);

}