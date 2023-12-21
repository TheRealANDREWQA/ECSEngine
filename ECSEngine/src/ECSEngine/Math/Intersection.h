#pragma once
#include "Vector.h"
#include "Plane.h"
#include "AABB.h"

namespace ECSEngine {

	// It returns true if the segment intersects the plane, else false.
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from segment_a
	// to the actual point
	ECSENGINE_API Vector8 ECS_VECTORCALL IntersectSegmentPlaneMask(Vector8 segment_a, Vector8 segment_b, Plane plane, Vector8& output);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IntersectSegmentPlane,
		3,
		FORWARD(Vector8 segment_a, Vector8 segment_b, Plane plane, Vector8& output),
		FORWARD(segment_a, segment_b, plane, output)
	);

	// It returns true if the ray intersects the plane, else false.
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from ray_origin
	// to the actual point. Don't access a lane that is not intersected - the values are undefined
	ECSENGINE_API Vector8 ECS_VECTORCALL IntersectRayPlaneMask(Vector8 ray_origin, Vector8 ray_direction, Plane plane, Vector8& output);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IntersectRayPlane,
		3,
		FORWARD(Vector8 ray_origin, Vector8 ray_direction, Plane plane, Vector8& output),
		FORWARD(ray_origin, ray_direction, plane, output)
	);

	// Infinite line - as if testing two rays from the same location but with opposing directions
	// It returns true if the line intersects the plane, else false (they are parallel).
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from (0.0f, 0.0f, 0.0f)
	// to the actual point. Don't access a lane that is not intersected - the values are undefined
	ECSENGINE_API Vector8 ECS_VECTORCALL IntersectLinePlaneMask(Vector8 line_direction, Plane plane, Vector8& output);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IntersectLinePlane,
		3,
		FORWARD(Vector8 line_direction, Plane plane, Vector8& output),
		FORWARD(line_direction, plane, output)
	);

	// The low and high of the segments needs to be splatted with the same value
	// It returns true if the segment intersects the plane, else false.
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from segment_a
	// to the actual point
	ECSENGINE_API Vector8 ECS_VECTORCALL IntersectSegmentAABBMask(Vector8 segment_a_splatted, Vector8 segment_b_splatted, AABB aabb, Vector8& output);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IntersectSegmentAABB,
		3,
		FORWARD(Vector8 segment_a_splatted, Vector8 segment_b_splatted, AABB aabb, Vector8& output),
		FORWARD(segment_a_splatted, segment_b_splatted, aabb, output)
	);

	// The low and high of the origin and direction needs to be splatted
	// It returns true if the ray intersects the plane, else false.
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from ray_origin
	// to the actual point. Don't access a lane that is not intersected - the values are undefined
	ECSENGINE_API Vector8 ECS_VECTORCALL IntersectRayAABBMask(Vector8 ray_origin_splatted, Vector8 ray_direction_normalized_splatted, AABB aabb, Vector8& output);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IntersectRayAABB,
		3,
		FORWARD(Vector8 ray_origin_splatted, Vector8 ray_direction_normalized_splatted, AABB aabb, Vector8& output),
		FORWARD(ray_origin_splatted, ray_direction_normalized_splatted, aabb, output)
	);

	// The direction needs to be normalized. The low and high must be splatted
	// Infinite line - as if testing two rays from the same location but with opposing directions
	// It returns true if the line intersects the plane, else false (they are parallel).
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from (0.0f, 0.0f, 0.0f)
	// to the actual point. Don't access a lane that is not intersected - the values are undefined
	ECSENGINE_API Vector8 ECS_VECTORCALL IntersectLineAABBMask(Vector8 line_direction_normalized_splatted, AABB aabb, Vector8& output);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IntersectLineAABB,
		3,
		FORWARD(Vector8 line_direction_normalized_splatted, AABB aabb, Vector8& output),
		FORWARD(line_direction_normalized_splatted, aabb, output)
	);

}