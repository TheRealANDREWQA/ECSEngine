#pragma once
#include "Vector.h"
#include "Plane.h"
#include "AABB.h"

namespace ECSEngine {

	// It returns true if the segment intersects the plane, else false.
	// In case an intersection was detected, the output is split into the actual
	// Point and the interpolation factor T.
	ECSENGINE_API bool IntersectSegmentPlane(float3 segment_a, float3 segment_b, PlaneScalar plane, float3& point_output, float& t_output);

	// It returns true if the segment intersects the plane, else false.
	// In case an intersection was detected, the output is split into the actual
	// Point and the interpolation factor T. Don't access a lane that is not intersected
	// The values are undefined
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IntersectSegmentPlane(Vector3 segment_a, Vector3 segment_b, Plane plane, Vector3& point_output, Vec8f& t_output);

	// It returns true if the ray intersects the plane, else false.
	// In case an intersection was detected, the output is split into the actual
	// Point and the interpolation factor T.
	ECSENGINE_API bool IntersectRayPlane(float3 ray_origin, float3 ray_direction, PlaneScalar plane, float3& point_output, float& t_output);

	// It returns true if the ray intersects the plane, else false.
	// In case an intersection was detected, the output is split into the actual
	// Point and the interpolation factor T. Don't access a lane that is not intersected
	// The values are undefined
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IntersectRayPlane(Vector3 ray_origin, Vector3 ray_direction, Plane plane, Vector3& point_output, Vec8f& t_output);

	struct IntersectAABBPrepareData {
		Vector3 direction;
		Vector3 inverse_direction;
		Vector3 origin;
		bool3 signs;
	};

	// Produces some values that can be cached between multiple tests to speed up the implementation
	ECSENGINE_API IntersectAABBPrepareData ECS_VECTORCALL IntersectAABBPrepareIterate(float3 origin, float3 direction);

	// It returns true if the ray intersects the aabb, else false.
	// In case an intersection was detected, the output is split into the actual
	// Point and the interpolation factor T.
	ECSENGINE_API bool IntersectRayAABB(
		float3 ray_origin,
		float3 ray_direction,
		AABBScalar aabb,
		float3& point_output,
		float& t_output
	);

	// It returns true if the ray intersects the aabb, else false.
	// In case an intersection was detected, the output is split into the actual
	// Point and the interpolation factor T. Don't access a lane that is not intersected
	// The values are undefined
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IntersectRayAABB(
		float3 ray_origin, 
		float3 ray_direction, 
		AABB aabb, 
		Vector3& point_output, 
		Vec8f& t_output
	);

	// It returns true if the ray intersects the aabb, else false.
	// In case an intersection was detected, the output is split into the actual
	// Point and the interpolation factor T. Don't access a lane that is not intersected
	// The values are undefined
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IntersectRayAABBIterate(
		const IntersectAABBPrepareData* prepare_data,
		AABB aabb,
		Vector3& point_output,
		Vec8f& t_output
	);

	template<typename Vector>
	ECS_INLINE typename Vector::T ECS_VECTORCALL InitializeClipTMin() {
		return SingleZeroVector<Vector>();
	}

	template<typename Vector>
	ECS_INLINE typename Vector::T ECS_VECTORCALL InitializeClipTMax() {
		return OneVector<Vector>();
	}

	template<typename Vector>
	ECS_INLINE typename Vector::T ECS_VECTORCALL InitializeClipTFactor(Vector segment_a, Vector segment_b) {
		// We need the reciprocal length such that we can use the multiply more efficiently
		// Inside multiple iterations
		return ReciprocalLength(segment_b - segment_a);
	}

	// Segment_t_min and segment_t_max describe the actual points after the clipping
	// If this is the first iteration, t_min and t_max need to be initialized using
	// InitializeClipTMin/Max(). This can be used iteratively to clip these points
	// Against multiple planes. The last argument is used to determine when a segment
	// Is considered to be parallel to the plane to skip the t_update.
	// The parameters are like this instead of a more clasical (segment_a, segment_b)
	// In order to allow for a single code path to be implemented when using this in a loop
	// The direction needs to be normalized mostly for consistency parallel check. It is
	// Not core to the algorithm otherwise
	ECSENGINE_API void ClipSegmentAgainstPlane(
		const Plane& plane,
		const Vector3& segment_a,
		const Vector3& normalized_direction,
		const Vec8f& segment_t_factor,
		Vec8f& segment_t_min,
		Vec8f& segment_t_max,
		float parallel_epsilon = 0.0001f
	);

	// Segment_t_min and segment_t_max describe the actual points after the clipping
	// If this is the first iteration, t_min and t_max need to be initialized using
	// InitializeClipTMin/Max(). This can be used iteratively to clip these points
	// Against multiple planes. The last argument is used to determine when a segment
	// Is considered to be parallel to the plane to skip the t_update.
	// The parameters are like this instead of a more clasical (segment_a, segment_b)
	// In order to allow for a single code path to be implemented when using this in a loop
	// It returns true if the segment is still valid, else false (like when the segment
	// Is completely clipped).
	// The direction needs to be normalized mostly for consistency parallel check. It is
	// Not core to the algorithm otherwise
	ECSENGINE_API bool ClipSegmentAgainstPlane(
		const PlaneScalar& plane,
		const float3& segment_a,
		const float3& normalized_direction,
		float segment_t_factor,
		float& segment_t_min,
		float& segment_t_max,
		float parallel_epsilon = 0.0001f
	);

	// TType must be Vec8f or float
	template<typename TType>
	ECS_INLINE auto ECS_VECTORCALL ClipSegmentsValidStatus(TType segment_t_min, TType segment_t_max) {
		return segment_t_min < segment_t_max;
	}

	// Calculates a point based on the t factor
	template<typename Vector>
	ECS_INLINE Vector ECS_VECTORCALL ClipSegmentCalculatePoint(Vector segment_a, Vector normalized_direction, typename Vector::T segment_t, typename Vector::T t_factor) {
		return Fmadd(normalized_direction, Vector::Splat(segment_t * t_factor), segment_a);
	}
	
}