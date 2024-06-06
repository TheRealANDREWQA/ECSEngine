#pragma once
#include "../Utilities/BasicTypes.h"
#include "Vector.h"

namespace ECSEngine {

	// TODO: Make a triangle struct?

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

	// PERFORMANCE TODO:
	// If you need to repeadetly to do, a new function that takes multiple points
	// Should be implemented, since some information can be cached
	
	ECSENGINE_API float3 ProjectPointOnTriangle(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point);

	ECSENGINE_API Vector3 ECS_VECTORCALL ProjectPointOnTriangle(Vector3 triangle_a, Vector3 triangle_b, Vector3 triangle_c, Vector3 point);

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

	// This version is slightly faster since it doesn't need to compute
	// The triangle normal. It returns true if the segment intersects the
	// Triangle, considering all points to be coplanar
	ECSENGINE_API bool IsCoplanarSegmentIntersectingTriangleWithNormal(
		float3 triangle_a,
		float3 triangle_b,
		float3 triangle_c,
		float3 triangle_normal,
		float3 segment_0,
		float3 segment_1
	);

	// It returns true if the segment intersects the
	// Triangle, considering all points to be coplanar
	ECS_INLINE bool IsCoplanarSegmentIntersectingTriangle(
		float3 triangle_a,
		float3 triangle_b,
		float3 triangle_c,
		float3 segment_0,
		float3 segment_1
	) {
		return IsCoplanarSegmentIntersectingTriangleWithNormal(
			triangle_a,
			triangle_b,
			triangle_c,
			TriangleNormal(triangle_a, triangle_b, triangle_c),
			segment_0,
			segment_1
		);
	}

	// Returns true if the ABC and DEF triangles are intersecting. It assumes that the triangles are
	// Coplanar or close to being coplanar. The enlargement factor is used to enlarge/shrink the triangles
	// This is useful for the case when you want triangles that have collinear edges to be included or excluded
	ECSENGINE_API bool AreCoplanarTrianglesIntersecting(float3 A, float3 B, float3 C, float3 D, float3 E, float3 F, float enlargement_factor = 1.0f);

	// Returns if the projected point is inside the triangle, else false
	ECSENGINE_API bool IsPointContainedInTriangle(float3 A, float3 B, float3 C, float3 point);

	// Returns true if the projected point is inside the triangle, else false
	// The projected point needs to be coplanar with ABC
	ECSENGINE_API bool IsProjectedPointContainedInTriangle(float3 A, float3 B, float3 C, float3 projected_point);

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL AreProjectedPointsContainedInTriangle(Vector3 A, Vector3 B, Vector3 C, Vector3 projected_point);

	// The status pointer must have at least points.size entries
	// This version is more efficient than the other one for multiple points
	ECSENGINE_API void ArePointsContainedInTriangle(float3 A, float3 B, float3 C, Stream<float3> points, bool* status);

	// The status pointer must have at least points.size entries
	// This version is more efficient than the other one for multiple points
	ECSENGINE_API void ArePointsContainedInTriangle(
		float3 A,
		float3 B,
		float3 C,
		size_t count,
		const float* points_x,
		const float* points_y,
		const float* points_z,
		bool* status
	);

	// Returns true if the line from the projection of line_point on the triangle to triangle_corner
	// Is intersecting the triangle, else false
	ECSENGINE_API bool IsPointToTriangleCornerIntersecting(float3 triangle_corner, float3 line_point, float3 triangle_B, float3 triangle_C);
	
	// Returns true if the line from the projection of line_point on the triangle to triangle_corner
	// Is intersecting the triangle, else false
	ECSENGINE_API bool IsProjectedPointToTriangleCornerIntersecting(float3 triangle_corner, float3 projected_line_point, float3 triangle_B, float3 triangle_C);
	
	// It represents the edge that is sliver, i.e. the smallest
	enum ECS_TRIANGLE_SLIVER_TYPE : unsigned char {
		ECS_TRIANGLE_SLIVER_NONE,
		ECS_TRIANGLE_SLIVER_AB,
		ECS_TRIANGLE_SLIVER_AC,
		ECS_TRIANGLE_SLIVER_BC
	};

	// For a triangle to be considered sliver, it needs to have an edge that by dividing the
	// Other two by this edge squared length, it results in a ratio greater or equal to the one given
	ECSENGINE_API ECS_TRIANGLE_SLIVER_TYPE IsTriangleSliver(float3 A, float3 B, float3 C, float squared_edge_ratio);

	// For a triangle to be considered sliver, it needs to have an edge that by dividing the
	// Other two by this edge squared length, it results in a ratio greater or equal to the one given
	ECSENGINE_API Vec8ui ECS_VECTORCALL IsTriangleSliver(Vector3 A, Vector3 B, Vector3 C, Vec8f squared_edge_ratio);

	// For a triangle to be considered sliver, it needs to have one of the angles smaller than the one given
	ECSENGINE_API ECS_TRIANGLE_SLIVER_TYPE IsTriangleSliverByAngle(float3 A, float3 B, float3 C, float degrees);

	// For a triangle to be considered sliver, it needs to have one of the angles smaller than the one given
	ECSENGINE_API Vec8ui ECS_VECTORCALL IsTriangleSliverByAngle(Vector3 A, Vector3 B, Vector3 C, Vec8f degrees);

	// TODO: Does this work for CW order?
	// It will fill in the indices of the triangles that make up one triangulation
	// Of the face. The points must be given in CCW order
	// The output triangles will have the vertices in CCW order
	ECSENGINE_API void TriangulateFace(Stream<unsigned int> point_indices, AdditionStream<uint3> triangle_indices);

	// TODO: Does this work for CW order?
	// It will fill in the indices of the triangles that make up one triangulation
	// Of the face. The points must be given in CCW order
	// The output triangles will have the vertices in CCW order
	ECSENGINE_API void TriangulateFace(Stream<unsigned short> point_indices, AdditionStream<ushort3> triangle_indices);

}