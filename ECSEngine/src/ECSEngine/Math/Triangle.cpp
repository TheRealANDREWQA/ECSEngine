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
		return IsTriangleFacing(normal, a, d);
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
		if (!PointSameLineHalfPlaneProjected(triangle_a, triangle_c, projection, ab_projected_point)) {
			// Clamp the point to the AB edge
			return ClampPointToSegment(triangle_a, triangle_b, ab_projected_point);
		}
		
		float3 ac_normalized = Normalize(triangle_c - triangle_a);
		float3 ac_projected_point = ProjectPointOnLineDirectionNormalized(triangle_a, ac_normalized, projection);
		if (!PointSameLineHalfPlaneProjected(triangle_a, triangle_b, projection, ac_projected_point)) {
			// Clamp the point to the AC edge
			return ClampPointToSegment(triangle_a, triangle_c, ac_projected_point);
		}
		
		float3 bc_normalized = Normalize(triangle_c - triangle_b);
		float3 bc_projected_point = ProjectPointOnLineDirectionNormalized(triangle_b, bc_normalized, projection);
		if (!PointSameLineHalfPlaneProjected(triangle_b, triangle_a, projection, bc_projected_point)) {
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

	bool AreCoplanarTrianglesIntersecting(float3 A, float3 B, float3 C, float3 D, float3 E, float3 F) {
		// We need to test each point to see if it is contained in the other triangle
		// We can use SIMD to perform the 2 containment tests at once
		float x_values[Vector3::ElementCount()];
		float y_values[Vector3::ElementCount()];
		float z_values[Vector3::ElementCount()];

		auto set_value = [&](float3 value, unsigned int index) {
			x_values[index] = value.x;
			y_values[index] = value.y;
			z_values[index] = value.z;
		};

		auto set_values = [&](float3 first, float3 second, float3 third, float3 fourth, float3 fifth, float3 sixth) {
			set_value(first, 0);
			set_value(second, 1);
			set_value(third, 2);
			set_value(fourth, 4);
			set_value(fifth, 5);
			set_value(sixth, 6);

			return Vector3().Load(x_values, y_values, z_values);
		};

		// In order to catch the case that the triangles have collinear points, we need to enlarge the triangles
		// By a bit such that a point will be caught as belonging to the interior of the other. The enlarged triangle
		// Point values need to be used only for the line origin and reference point. Both triangles need to be enlarged
		const float ENLARGEMENT_FACTOR = 1.0001f;
		float3 enlargement_factor = float3::Splat(ENLARGEMENT_FACTOR);

		float3 ABC_center = (A + B + C) * float3::Splat(1 / 3.0f);
		float3 enlarged_A = ABC_center + (A - ABC_center) * enlargement_factor;
		float3 enlarged_B = ABC_center + (B - ABC_center) * enlargement_factor;
		float3 enlarged_C = ABC_center + (C - ABC_center) * enlargement_factor;

		float3 DEF_center = (D + E + F) * float3::Splat(1 / 3.0f);
		float3 enlarged_D = DEF_center + (D - DEF_center) * enlargement_factor;
		float3 enlarged_E = DEF_center + (E - DEF_center) * enlargement_factor;
		float3 enlarged_F = DEF_center + (F - DEF_center) * enlargement_factor;

		// We need to use the function PointSameLineHalfPlaneNormalized
		// Where we need a line point, the normalized line direction, the reference point and the test point
		// Firstly, calculate the normalized line directions since we need it
		// We need the AB, AC, BC, DE, DF and EF directions
		Vector3 line_origin = set_values(enlarged_A, enlarged_A, enlarged_B, enlarged_D, enlarged_D, enlarged_E);
		Vector3 line_target = set_values(enlarged_B, enlarged_C, enlarged_C, enlarged_E, enlarged_F, enlarged_F);
		Vector3 direction = line_target - line_origin;
		Vector3 normalized_direction = Normalize(direction);

		// Test the A and B points against D, E and F
		Vector3 ab_line_points = set_values(enlarged_D, enlarged_D, enlarged_E, enlarged_D, enlarged_D, enlarged_E);
		// We can splat the lanes in order to use a more efficient permute
		Vector3 def_line_direction = { SplatHighLane(normalized_direction.x), SplatHighLane(normalized_direction.y), SplatHighLane(normalized_direction.z) };
		Vector3 ab_reference_points = set_values(enlarged_F, enlarged_E, enlarged_D, enlarged_F, enlarged_E, enlarged_D);
		Vector3 ab_test_points = set_values(A, A, A, B, B, B);
		VectorMask ab_are_contained = PointSameLineHalfPlaneNormalized(ab_line_points, def_line_direction, ab_reference_points, ab_test_points);
		// Check to see if we have 3 bits set for each lane
		bool a_contained = ab_are_contained.GetRange<3, 0>();
		if (a_contained) {
			return true;
		}
		bool b_contained = ab_are_contained.GetRange<3, 4>();
		if (b_contained) {
			return true;
		}
		
		// Test the C and D points. The C point will be in the high lane in order to use the normalized_direction as is
		Vector3 cd_line_points = set_values(enlarged_A, enlarged_A, enlarged_B, enlarged_D, enlarged_D, enlarged_E); // It is equal to line_origin, but it unlikely to be kept
		// In the registers to have any performance impact
		Vector3 cd_line_direction = normalized_direction;
		Vector3 cd_reference_points = set_values(enlarged_C, enlarged_B, enlarged_A, enlarged_F, enlarged_E, enlarged_D);
		Vector3 cd_test_points = set_values(D, D, D, C, C, C);
		VectorMask cd_are_contained = PointSameLineHalfPlaneNormalized(cd_line_points, cd_line_direction, cd_reference_points, cd_test_points);
		bool d_contained = cd_are_contained.GetRange<3, 0>();
		if (d_contained) {
			return true;
		}
		bool c_contained = cd_are_contained.GetRange<3, 4>();
		if (c_contained) {
			return true;
		}

		// Test the E and F points
		Vector3 ef_line_points = { SplatLowLane(cd_line_points.x), SplatLowLane(cd_line_points.y), SplatLowLane(cd_line_points.z) };
		Vector3 ef_line_direction = { SplatLowLane(normalized_direction.x), SplatLowLane(normalized_direction.y), SplatLowLane(normalized_direction.z) };
		// We could splat the low lane again, but here use the memory operations since
		// The splatting should use an execution port and we can interleave memory operations in the meantime
		Vector3 ef_reference_points = set_values(enlarged_C, enlarged_B, enlarged_A, enlarged_C, enlarged_B, enlarged_A);
		Vector3 ef_test_points = set_values(E, E, E, F, F, F);
		VectorMask ef_are_contained = PointSameLineHalfPlaneNormalized(ef_line_points, ef_line_direction, ef_reference_points, ef_test_points);
		bool e_contained = ef_are_contained.GetRange<3, 0>();
		if (e_contained) {
			return true;
		}
		bool f_contained = ef_are_contained.GetRange<3, 4>();
		if (f_contained) {
			return true;
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------

}