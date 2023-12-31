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

}