#pragma once
#include "../Utilities/BasicTypes.h"
#include "Vector.h"

namespace ECSEngine {

	// Returns the normal calculated as the cross product between AB and AC
	// The normal is not normalized. You must do that manually or use the different function
	ECSENGINE_API float3 TriangleNormal(float3 a, float3 b, float3 c);

	// Returns the normal of the triangle facing the given point
	// The normal is not normalized. You must do that manually or use the different function
	ECSENGINE_API float3 TriangleNormal(float3 a, float3 b, float3 c, float3 look_point);

	// Returns the normal calculated as the cross product between AB and AC. The result is normalized
	ECSENGINE_API float3 TriangleNormalNormalized(float3 a, float3 b, float3 c);

	// Returns the normal of the triangle facing the given point. The result is normalized
	ECSENGINE_API float3 TriangleNormalNormalized(float3 a, float3 b, float3 c, float3 look_point);

	// Returns true if the triangle ABC is facing d using the TriangleNormal function
	ECSENGINE_API bool IsTriangleFacing(float3 a, float3 b, float3 c, float3 d);

	ECS_INLINE bool IsTriangleFacing(float3 triangle_normal, float3 triangle_point, float3 test_point) {
		return Dot(triangle_normal, test_point - triangle_point) > 0.0f;
	}

	ECSENGINE_API float TriangleArea(float3 point_a, float3 point_b, float3 point_c);

	// This function produces the distance fast by default. If you want the squared distance, just perform
	// The multiply again with this value
	ECSENGINE_API float DistanceToTriangle(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point);

	// This function produces the distance fast by default. If you want the squared distance, just perform
	// The multiply again with this value
	ECSENGINE_API float DistanceToTriangleWithNormal(float3 triangle_point, float3 triangle_normal_normalized, float3 point);

	// If the projection point lies outside the triangle, it will clamp the point to one
	// of the triangle edges and return that distance. Basically a closest point calculation
	ECSENGINE_API float SquaredDistanceToTriangleClamped(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point);

	// If the projection point lies outside the triangle, it will clamp the point to one
	// of the triangle edges and return that distance. Basically, a closest point calculation
	ECSENGINE_API float DistanceToTriangleClamped(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point);

	// If the projection point lies outside the triangle, it will clamp the point to one
	// of the triangle edges. Basically, a closest point calculation
	ECSENGINE_API float3 ProjectPointOnTriangleClamped(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point);

	// Returns true if both the point and the reference point are on the same triangle side
	ECSENGINE_API bool ArePointsSameTriangleSide(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point, float3 reference_point);

	// Returns true if the ABC and DEF triangles are intersecting. It assumes that the triangles are
	// Coplanar or close to being coplanar
	ECSENGINE_API bool AreCoplanarTrianglesIntersecting(float3 A, float3 B, float3 C, float3 D, float3 E, float3 F);

	// Returns true if the point is inside the triangle ABC. The point must be coplanar with the triangle
	// You can use the other overload that takes as parameter 2 points, since the performance is the same
	// Since it uses SIMD to speed it up
	ECSENGINE_API bool TestCoplanarPointContainedInTriangle(float3 A, float3 B, float3 C, float3 point);

	ECSENGINE_API bool TestCoplanarPointContainedInTriangle(float3 A, float3 B, float3 C, float3 point0, float3 point1);

}