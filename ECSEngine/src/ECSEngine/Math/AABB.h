#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"
#include "Conversion.h"

namespace ECSEngine {

	struct AABBScalar {
		ECS_INLINE AABBScalar() {}
		ECS_INLINE AABBScalar(float3 min, float3 max) : min(min), max(max) {}

		ECS_INLINE float3& operator [](size_t index) {
			float3* corners = (float3*)this;
			return corners[index];
		}

		ECS_INLINE const float3& operator [](size_t index) const {
			const float3* corners = (const float3*)this;
			return corners[index];
		}

		float3 min;
		float3 max;
	};

	struct AABB {
		ECS_INLINE AABB() {}
		ECS_INLINE AABB(Vector3 min, Vector3 max) : min(min), max(max) {}

		ECS_INLINE AABBScalar At(size_t index) const {
			return { min.At(index), max.At(index) };
		}

		ECS_INLINE Vector3& operator[](size_t index) {
			Vector3* corners = (Vector3*)this;
			return corners[index];
		}

		ECS_INLINE const Vector3& operator [](size_t index) const {
			const Vector3* corners = (const Vector3*)this;
			return corners[index];
		}

		ECS_INLINE void Set(AABBScalar aabb, size_t index) {
			min.Set(aabb.min, index);
			max.Set(aabb.max, index);
		}

		Vector3 min;
		Vector3 max;
	};

	// This is the same as an OOBB but we haven't decided upon the OOBB
	// Representation yet. Use this as a placeholder
	struct RotatedAABB {
		Vector3 center;
		Vector3 extents;
		// The Z axis can be deduced from the 
		Vector3 local_x_axis;
		Vector3 local_y_axis;
	};

	// Returns the AABBScalar that encompases all of them
	ECSENGINE_API AABBScalar GetCoalescedAABB(Stream<AABBScalar> aabbs);

	ECSENGINE_API AABBScalar GetAABBFromPoints(Stream<float3> points);

	ECS_INLINE float3 AABBCenter(AABBScalar aabb) {
		return (aabb.min + aabb.max) * float3::Splat(0.5f);
	}

	ECS_INLINE Vector3 ECS_VECTORCALL AABBCenter(AABB aabb) {
		Vector3 half = Vector3::Splat(0.5f);
		return (aabb.min + aabb.max) * half;
	}

	ECS_INLINE float3 AABBExtents(AABBScalar aabb) {
		return aabb.max - aabb.min;
	}

	ECS_INLINE Vector3 ECS_VECTORCALL AABBExtents(AABB aabb) {
		return aabb.max - aabb.min;
	}

	ECS_INLINE float3 AABBHalfExtents(AABBScalar aabb) {
		return AABBExtents(aabb) * float3::Splat(0.5f);
	}

	ECS_INLINE Vector3 ECS_VECTORCALL AABBHalfExtents(AABB aabb) {
		return AABBExtents(aabb) * Vector3::Splat(0.5f);
	}

	ECS_INLINE AABBScalar TranslateAABB(AABBScalar aabb, float3 translation) {
		return { aabb.min + translation, aabb.max + translation };
	}

	ECS_INLINE AABB ECS_VECTORCALL TranslateAABB(AABB aabb, Vector3 translation) {
		return { aabb.min + translation, aabb.max + translation };
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
	ECSENGINE_API AABBScalar AABBFromRotation(const AABBScalar& aabb, const Matrix& rotation_matrix);

	// Computes the corresponding AABB from the given rotation matrix
	// If you want to use a quaternion, you must convert it to a rotation matrix first
	// And then call this function
	ECSENGINE_API AABB ECS_VECTORCALL AABBFromRotation(AABB aabb, Matrix rotation_matrix);

	ECSENGINE_API AABBScalar ScaleAABB(const AABBScalar& aabb, float3 scale);

	// This one scales the dimensions of the aabb (the center stays the same)
	ECSENGINE_API AABB ECS_VECTORCALL ScaleAABB(AABB aabb, Vector3 scale);

	// This version scales the values from the origin (it is as if the
	// min and max are multiplied with the factor scale)
	ECS_INLINE AABBScalar ECS_VECTORCALL ScaleAABBFromOrigin(AABBScalar aabb, float3 scale) {
		return { aabb.min * scale, aabb.max * scale };
	}

	// This version scales the values from the origin (it is as if the
	// min and max are multiplied with the factor scale)
	ECS_INLINE AABB ECS_VECTORCALL ScaleAABBFromOrigin(AABB aabb, Vector3 scale) {
		return { aabb.min * scale, aabb.max * scale };
	}

	// Returns an encompasing AABB from the given transformation
	ECSENGINE_API AABBScalar TransformAABB(const AABBScalar& aabb, float3 translation, const Matrix& rotation_matrix, float3 scale);

	// Returns an encompasing AABB from the given transformation
	ECSENGINE_API AABB ECS_VECTORCALL TransformAABB(AABB aabb, Vector3 translation, Matrix rotation_matrix, Vector3 scale);

	ECS_INLINE bool CompareAABBMask(AABBScalar first, AABBScalar second, float epsilon = ECS_SIMD_VECTOR_EPSILON_VALUE) {
		return CompareMask(first.min, second.min, float3::Splat(epsilon)) && CompareMask(first.max, second.max, float3::Splat(epsilon));
	}

	ECS_INLINE SIMDVectorMask ECS_VECTORCALL CompareAABBMask(AABB first, AABB second, Vec8f epsilon = VectorGlobals::EPSILON) {
		return CompareMask(first.min, second.min, epsilon) && CompareMask(first.max, second.max, epsilon);
	}

	ECSENGINE_API AABBScalar GetCombinedAABB(const AABBScalar& first, const AABBScalar& second);

	ECSENGINE_API AABB ECS_VECTORCALL GetCombinedAABB(AABB first, AABB second);

	ECS_INLINE AABBScalar InfiniteAABBScalar() {
		return { float3::Splat(-FLT_MAX), float3::Splat(FLT_MAX) };
	}

	ECS_INLINE AABB ECS_VECTORCALL InfiniteAABB() {
		return { Vector3::Splat(-FLT_MAX), Vector3::Splat(FLT_MAX) };
	}

	ECS_INLINE AABBScalar ReverseInfiniteAABBScalar() {
		return { float3::Splat(FLT_MAX), float3::Splat(-FLT_MAX) };
	}

	ECS_INLINE AABB ECS_VECTORCALL ReverseInfiniteAABB() {
		return { Vector3::Splat(FLT_MAX), Vector3::Splat(-FLT_MAX) };
	}

	ECSENGINE_API bool AABBOverlap(const AABBScalar& first, const AABBScalar& second);

	// It will test a given aabb from the first against all the aabbs from the second
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL AABBOverlap(AABB first, AABB second, size_t first_index);

	// Tests the selected aabb against all the other aabbs
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL AABBOverlap(AABB aabbs, size_t index);

	// There need to be 8 entries for the corners pointer
	// The first 4 entries are the "left" face and the other
	// 4 are for the "right" face
	ECSENGINE_API void GetAABBCorners(const AABBScalar& aabb, float3* corners);

	// Returns the 8 cornerns of the AABB packed into a SIMD vector.
	ECSENGINE_API Vector3 ECS_VECTORCALL GetAABBCornersScalarToSIMD(AABBScalar aabb);

	// There need to be 8 entries for the corners pointer
	// The first 4 entries are the "left" face and the other
	// 4 are for the "right" face
	ECSENGINE_API void ECS_VECTORCALL GetAABBCorners(AABB aabb, Vector3* corners);

	// Returns the squared distance from the AABB to the point
	ECSENGINE_API float AABBToPointSquaredDistance(const AABBScalar& aabb, float3 point);

	// Returns the squared distance from the AABB to the point
	ECSENGINE_API Vec8f ECS_VECTORCALL AABBToPointSquaredDistance(AABB aabb, Vector3 point);

	// Returns the closest point from the given point to the AABB
	ECSENGINE_API float3 AABBClosestPoint(const AABBScalar& aabb, float3 point);

	// Returns the closest point from the given point to the AABB
	ECSENGINE_API Vector3 ECS_VECTORCALL AABBClosestPoint(AABB aabb, Vector3 point);
}