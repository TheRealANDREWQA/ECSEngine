#include "ecspch.h"
#include "Triangle.h"
#include "Vector.h"
#include "Plane.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	float3 TriangleNormal(float3 a, float3 b, float3 c)
	{
		return Cross(b - a, c - a);
	}

	// --------------------------------------------------------------------------------------------------

	float3 TriangleNormal(float3 a, float3 b, float3 c, float3 look_point)
	{
		// Calculate the normal as usual and if the dot product between the normal
		// And the direction vector of the look point with one of the triangle corners
		// Is negative, we need to flip it
		float3 normal = TriangleNormal(a, b, c);
		if (Dot(normal, look_point - c) < 0.0f) {
			return -normal;
		}
		return normal;
	}

	// --------------------------------------------------------------------------------------------------

	float3 TriangleNormalNormalized(float3 a, float3 b, float3 c) {
		return Normalize(TriangleNormal(a, b, c));
	}

	float3 TriangleNormalNormalized(float3 a, float3 b, float3 c, float3 look_point) {
		return Normalize(TriangleNormal(a, b, c, look_point));
	}

	// --------------------------------------------------------------------------------------------------

	bool IsTriangleFacing(float3 a, float3 b, float3 c, float3 d)
	{
		float3 normal = TriangleNormal(a, b, c);
		return Dot(normal, d - c) >= 0.0f;
	}

	// --------------------------------------------------------------------------------------------------

	float TriangleArea(float3 point_a, float3 point_b, float3 point_c)
	{
		// length(a X b) / 2
		return Length(Cross(point_a, point_b)) / 2;
	}

	// --------------------------------------------------------------------------------------------------

	float DistanceToTriangle(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point) {
		float3 normal = TriangleNormalNormalized(triangle_a, triangle_b, triangle_c);
		return DistanceToTriangleWithNormal(triangle_a, normal, point);
	}

	float DistanceToTriangleWithNormal(float3 triangle_point, float3 triangle_normal_normalized, float3 point) {
		PlaneScalar plane = PlaneScalar::FromNormalized(triangle_normal_normalized, triangle_point);
		return fabsf(DistanceToPlane(plane, point));
	}

	// --------------------------------------------------------------------------------------------------

	float SquaredDistanceToTriangleClamped(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point)
	{
		return SquareLength(ProjectPointOnTriangleClamped(triangle_a, triangle_b, triangle_c, point));
	}

	// --------------------------------------------------------------------------------------------------

	float DistanceToTriangleClamped(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point)
	{
		return Length(ProjectPointOnTriangleClamped(triangle_a, triangle_b, triangle_c, point));
	}

	// --------------------------------------------------------------------------------------------------

	float3 ProjectPointOnTriangleClamped(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point)
	{
		float3 projection = ProjectPointOnPlane(ComputePlane(triangle_a, triangle_b, triangle_c), point);
		// Determine if the projection lies inside each of the triangle's edge. If it fails for one edge,
		// Then we know we need to clamp the point to that edge
		float3 ab_normalized = Normalize(triangle_b - triangle_a);
		float3 ab_projected_point = ProjectPointOnLineDirectionNormalized(triangle_a, ab_normalized, projection);
		if (!PointSameLineHalfPlaneNormalizedEx(triangle_a, triangle_c, projection, ab_projected_point)) {
			// Clamp the point to the AB edge
			return ClampPointToSegment(triangle_a, triangle_b, ab_projected_point);
		}
		
		float3 ac_normalized = Normalize(triangle_c - triangle_a);
		float3 ac_projected_point = ProjectPointOnLineDirectionNormalized(triangle_a, ac_normalized, projection);
		if (!PointSameLineHalfPlaneNormalizedEx(triangle_a, triangle_b, projection, ac_projected_point)) {
			// Clamp the point to the AC edge
			return ClampPointToSegment(triangle_a, triangle_c, ac_projected_point);
		}
		
		float3 bc_normalized = Normalize(triangle_c - triangle_b);
		float3 bc_projected_point = ProjectPointOnLineDirectionNormalized(triangle_b, bc_normalized, projection);
		if (!PointSameLineHalfPlaneNormalizedEx(triangle_b, triangle_a, projection, bc_projected_point)) {
			// Clamp the point to the AC edge
			return ClampPointToSegment(triangle_b, triangle_c, ac_projected_point);
		}

		return projection;
	}

	// --------------------------------------------------------------------------------------------------

	bool ArePointsSameTriangleSide(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point, float3 reference_point)
	{
		// This function is similar to that of ArePointsSamePlaneSide, with the exception that we don't
		// Need the normalized triangle normal

		float3 triangle_normal = TriangleNormal(triangle_a, triangle_b, triangle_c);
		// They are on the same side if their dot products from a triangle point with the normal have the same sign
		float point_dot = Dot(triangle_normal, point - triangle_a);
		float reference_point_dot = Dot(triangle_normal, reference_point - triangle_a);
		return (point_dot * reference_point_dot) > 0.0f;
	}

	// --------------------------------------------------------------------------------------------------

}