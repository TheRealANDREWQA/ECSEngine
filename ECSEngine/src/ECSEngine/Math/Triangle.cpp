#include "ecspch.h"
#include "Triangle.h"
#include "Vector.h"
#include "Plane.h"
#include "../Utilities/Utilities.h"

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
		// length(AB X AC) / 2
		return Length(Cross(point_b - point_a, point_c - point_a)) / 2;
	}

	// --------------------------------------------------------------------------------------------------

	template<typename Vector>
	static ECS_INLINE Vector ECS_VECTORCALL ProjectPointOnTriangleImpl(Vector triangle_a, Vector triangle_b, Vector triangle_c, Vector point) {
		auto plane = ComputePlane(triangle_a, triangle_b, triangle_c);
		return ProjectPointOnPlane(plane, point);
	}

	float3 ProjectPointOnTriangle(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point) {
		return ProjectPointOnTriangleImpl(triangle_a, triangle_b, triangle_c, point);
	}

	Vector3 ECS_VECTORCALL ProjectPointOnTriangle(Vector3 triangle_a, Vector3 triangle_b, Vector3 triangle_c, Vector3 point) {
		return ProjectPointOnTriangleImpl(triangle_a, triangle_b, triangle_c, point);
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

	struct InsideTriangleActionOuptut {
		float3 side_1;
		float3 side_2;
		float3 point_projection_on_side;
	};

	// Returns true if it is inside the triangle, else false. When it returns false, it will fill in the output
	// Structure such that you can do something else with that information
	static bool InsideTriangleAction(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 projected_point, InsideTriangleActionOuptut* output) {
		// Determine if the projection lies inside each of the triangle's edge. If it fails for one edge,
		// Then we know we need to clamp the point to that edge
		float3 ab_normalized = Normalize(triangle_b - triangle_a);
		float3 ab_projected_point = ProjectPointOnLineDirectionNormalized(triangle_a, ab_normalized, projected_point);
		if (!PointSameLineHalfPlaneProjected(triangle_a, triangle_c, projected_point, ab_projected_point)) {
			*output = { triangle_a, triangle_b, ab_projected_point };
			return false;
		}

		float3 ac_normalized = Normalize(triangle_c - triangle_a);
		float3 ac_projected_point = ProjectPointOnLineDirectionNormalized(triangle_a, ac_normalized, projected_point);
		if (!PointSameLineHalfPlaneProjected(triangle_a, triangle_b, projected_point, ac_projected_point)) {
			*output = { triangle_a, triangle_c, ac_projected_point };
			return false;
		}

		float3 bc_normalized = Normalize(triangle_c - triangle_b);
		float3 bc_projected_point = ProjectPointOnLineDirectionNormalized(triangle_b, bc_normalized, projected_point);
		if (!PointSameLineHalfPlaneProjected(triangle_b, triangle_a, projected_point, bc_projected_point)) {
			*output = { triangle_b, triangle_c, bc_projected_point };
			return false;
		}

		return true;
	}

	// --------------------------------------------------------------------------------------------------

	float3 ProjectPointOnTriangleClamped(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point)
	{
		float3 projection = ProjectPointOnPlane(ComputePlane(triangle_a, triangle_b, triangle_c), point);
		InsideTriangleActionOuptut inside_output;
		if (!InsideTriangleAction(triangle_a, triangle_b, triangle_c, projection, &inside_output)) {
			// Clamp the side projection to the edge
			return ClampPointToSegment(inside_output.side_1, inside_output.side_2, inside_output.point_projection_on_side);
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

	bool IsCoplanarSegmentIntersectingTriangleWithNormal(
		float3 triangle_a,
		float3 triangle_b,
		float3 triangle_c,
		float3 triangle_normal,
		float3 segment_0,
		float3 segment_1
	) {
		// PERFORMANCE TODO: Internally SIMDize this?

		// We can treat this as a 2D SAT problem
		// We can use the normals to the 3 sides of the triangle
		// As separating axes

		auto test_edge = [triangle_normal, segment_0, segment_1](float3 edge_0, float3 edge_1, float3 other_triangle_point) {
			float3 edge_normal = Cross(edge_1 - edge_0, triangle_normal);
			// Since ab_normal is perpendicular to ab, we don't have to do a dot
			// For both endpoints since it will be the same
			float dot0 = Dot(edge_0, edge_normal);
			float min_T = dot0;
			float max_T = dot0;
			float other_point_dot = Dot(other_triangle_point, edge_normal);
			min_T = min(min_T, other_point_dot);
			max_T = max(max_T, other_point_dot);

			float dot_segment_0 = Dot(segment_0, edge_normal);
			float dot_segment_1 = Dot(segment_1, edge_normal);

			float segment_min = min(dot_segment_0, dot_segment_1);
			float segment_max = max(dot_segment_0, dot_segment_1);
			return AreIntervalsOverlapping(min_T, max_T, segment_min, segment_max);
		};

		if (!test_edge(triangle_a, triangle_b, triangle_c)) {
			return false;
		}
		if (!test_edge(triangle_a, triangle_c, triangle_b)) {
			return false;
		}
		if (!test_edge(triangle_b, triangle_c, triangle_a)) {
			return false;
		}

		// At last, test the perpendicular to the edge
		float3 edge_perpendicular = Cross(segment_1 - segment_0, triangle_normal);
		// For the segment, we have a single value to compute
		float segment_dot = Dot(segment_0, edge_perpendicular);
		float a_dot = Dot(triangle_a, edge_perpendicular);
		float b_dot = Dot(triangle_b, edge_perpendicular);
		float c_dot = Dot(triangle_c, edge_perpendicular);

		float triangle_min = min(a_dot, min(b_dot, c_dot));
		float triangle_max = max(a_dot, max(b_dot, c_dot));
		return IsInRange(segment_dot, triangle_min, triangle_max);
	}

	// --------------------------------------------------------------------------------------------------

	bool AreCoplanarTrianglesIntersecting(float3 A, float3 B, float3 C, float3 D, float3 E, float3 F, float enlargement_factor) {
		// This test can be summarized as testing the edges of one of the triangles
		// Against the other to the intersection test
		// HIGH PERFORMANCE TODO: This can be made massively faster by applying SIMD
		// Instead of making 3 calls to the function, since we can cache some calculations
		// As well. Use the normalized triangle normal in case the magnitude of this normal
		// Is small and can introduce very small errors
		float3 triangle_normal = TriangleNormalNormalized(A, B, C);
		
		if (enlargement_factor != 1.0f) {
			float3 DEF_center = (D + E + F) * float3::Splat(1 / 3.0f);
			D = DEF_center + (D - DEF_center) * enlargement_factor;
			E = DEF_center + (E - DEF_center) * enlargement_factor;
			F = DEF_center + (F - DEF_center) * enlargement_factor;
		}

		if (IsCoplanarSegmentIntersectingTriangleWithNormal(A, B, C, triangle_normal, D, E)) {
			return true;
		}
		if (IsCoplanarSegmentIntersectingTriangleWithNormal(A, B, C, triangle_normal, D, F)) {
			return true;
		}
		if (IsCoplanarSegmentIntersectingTriangleWithNormal(A, B, C, triangle_normal, E, F)) {
			return true;
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsPointContainedInTriangle(float3 A, float3 B, float3 C, float3 point) {
		return IsProjectedPointContainedInTriangle(A, B, C, ProjectPointOnTriangle(A, B, C, point));
	}

	// --------------------------------------------------------------------------------------------------

	bool IsProjectedPointContainedInTriangle(float3 A, float3 B, float3 C, float3 projected_point) {
		InsideTriangleActionOuptut output;
		return InsideTriangleAction(A, B, C, projected_point, &output);
	}

	// --------------------------------------------------------------------------------------------------

	SIMDVectorMask ECS_VECTORCALL AreProjectedPointsContainedInTriangle(Vector3 A, Vector3 B, Vector3 C, Vector3 projected_point) {
		// Determine if the projection lies inside each of the triangle's edge. If it fails for one edge,
		// Then we know we need to clamp the point to that edge
		Vector3 ab_normalized = Normalize(B - A);
		Vector3 ab_projected_point = ProjectPointOnLineDirectionNormalized(A, ab_normalized, projected_point);
		SIMDVectorMask ab_mask = PointSameLineHalfPlaneProjected(A, C, projected_point, ab_projected_point);
		if (!IsAnySet(ab_mask)) {
			return ab_mask;
		}

		Vector3 ac_normalized = Normalize(C - A);
		Vector3 ac_projected_point = ProjectPointOnLineDirectionNormalized(A, ac_normalized, projected_point);
		SIMDVectorMask ac_mask = PointSameLineHalfPlaneProjected(A, B, projected_point, ac_projected_point);
		SIMDVectorMask ab_ac_mask = ac_mask & ab_mask;
		if (!IsAnySet(ab_ac_mask)) {
			return ab_ac_mask;
		}

		Vector3 bc_normalized = Normalize(C - B);
		Vector3 bc_projected_point = ProjectPointOnLineDirectionNormalized(B, bc_normalized, projected_point);
		SIMDVectorMask bc_mask = PointSameLineHalfPlaneProjected(B, A, projected_point, bc_projected_point);
		return bc_mask & ab_ac_mask;
	}

	// --------------------------------------------------------------------------------------------------

	void ArePointsContainedInTriangle(float3 A, float3 B, float3 C, Stream<float3> points, bool* status) {
		// We can accelerate this using SIMD
		PlaneScalar scalar_triangle_plane = ComputePlane(A, B, C);
		Plane triangle_plane = scalar_triangle_plane;
		Vector3 vector_A = Vector3::Splat(A);
		Vector3 vector_B = Vector3::Splat(B);
		Vector3 vector_C = Vector3::Splat(C);
		
		ApplySIMDConstexpr(points.size, Vector3::ElementCount(), [&](auto is_full_iteration, size_t index, size_t count) {
			Vector3 current_points;
			if constexpr (is_full_iteration) {
				current_points.Gather(points.buffer + index);
			}
			else {
				current_points.GatherPartial(points.buffer + index, count);
			}

			// Project the points at once
			Vector3 projected_points = ProjectPointOnPlane(triangle_plane, current_points);
			VectorMask mask = AreProjectedPointsContainedInTriangle(vector_A, vector_B, vector_C, projected_points);
			mask.WriteBooleans(status + index, count);
		});
	}

	// --------------------------------------------------------------------------------------------------

	void ArePointsContainedInTriangle(
		float3 A, 
		float3 B, 
		float3 C, 
		size_t count, 
		const float* points_x, 
		const float* points_y, 
		const float* points_z, 
		bool* status
	)
	{
		// We can accelerate this using SIMD
		PlaneScalar scalar_triangle_plane = ComputePlane(A, B, C);
		Plane triangle_plane = scalar_triangle_plane;
		Vector3 vector_A = Vector3::Splat(A);
		Vector3 vector_B = Vector3::Splat(B);
		Vector3 vector_C = Vector3::Splat(C);

		ApplySIMDConstexpr(count, Vector3::ElementCount(), [&](auto is_full_iteration, size_t index, size_t count) {
			Vector3 current_points;
			if constexpr (is_full_iteration) {
				current_points.Load(points_x + index, points_y + index, points_z + index);
			}
			else {
				current_points.LoadPartial(points_x + index, points_y + index, points_z + index, count);
			}

			// Project the points at once
			Vector3 projected_points = ProjectPointOnPlane(triangle_plane, current_points);
			VectorMask mask = AreProjectedPointsContainedInTriangle(vector_A, vector_B, vector_C, projected_points);
			mask.WriteBooleans(status + index, count);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool IsPointToTriangleCornerIntersecting(float3 triangle_corner, float3 line_point, float3 triangle_B, float3 triangle_C) {
		float3 projected_line_point = ProjectPointOnTriangle(triangle_corner, triangle_B, triangle_C, line_point);
		return IsProjectedPointToTriangleCornerIntersecting(triangle_corner, projected_line_point, triangle_B, triangle_C);
	}

	bool IsProjectedPointToTriangleCornerIntersecting(float3 triangle_corner, float3 projected_line_point, float3 triangle_B, float3 triangle_C) {
		// Here is an example
		//        C
		//       /|
		//      / |
		//     /  |
		//    /   |
		//   /    |
		//  A --- B
		//
		//           D
		// AD intersects the triangle if and only if D is on the same side of AB as C
		// And on the same side of AC as B, such that it crosses the BC edge
		bool is_same_side_AB = PointSameLineHalfPlane(triangle_corner, triangle_B, triangle_C, projected_line_point);
		bool is_same_side_AC = PointSameLineHalfPlane(triangle_corner, triangle_C, triangle_B, projected_line_point);
		return is_same_side_AB && is_same_side_AC;
	}

	// --------------------------------------------------------------------------------------------------

	// TODO: Handle the case of very obtuse triangles, for both sliver functions

	template<typename ReturnType, typename Vector>
	static ECS_INLINE ReturnType ECS_VECTORCALL IsTriangleSliverImpl(Vector A, Vector B, Vector C, typename Vector::T squared_edge_ratio) {
		Vector AB = B - A;
		Vector AC = C - A;
		Vector BC = C - B;
		
		auto AB_sq_length = SquareLength(AB);
		auto AC_sq_length = SquareLength(AC);
		auto BC_sq_length = SquareLength(BC);

		// Start with the AB edge
		auto AB_sq_length_inverse = OneDividedVector(AB_sq_length);
		auto AB_AC_ratio = AC_sq_length * AB_sq_length_inverse;
		auto AB_BC_ratio = BC_sq_length * AB_sq_length_inverse;
		auto AB_mask = AB_AC_ratio >= squared_edge_ratio && AB_BC_ratio >= squared_edge_ratio;
		if constexpr (std::is_same_v<Vector, float3>) {
			if (AB_mask) {
				return ECS_TRIANGLE_SLIVER_AB;
			}
		}

		auto AC_sq_length_inverse = OneDividedVector(AC_sq_length);
		auto AC_AB_ratio = AB_sq_length * AC_sq_length_inverse;
		auto AC_BC_ratio = BC_sq_length * AC_sq_length_inverse;
		auto AC_mask = AC_AB_ratio >= squared_edge_ratio && AC_BC_ratio >= squared_edge_ratio;
		if constexpr (std::is_same_v<Vector, float3>) {
			if (AC_mask) {
				return ECS_TRIANGLE_SLIVER_AC;
			}
		}

		auto BC_sq_length_inverse = OneDividedVector(BC_sq_length);
		auto BC_AB_ratio = AB_sq_length * BC_sq_length_inverse;
		auto BC_AC_ratio = AC_sq_length * BC_sq_length_inverse;
		auto BC_mask = BC_AB_ratio >= squared_edge_ratio && BC_AC_ratio >= squared_edge_ratio;
		if constexpr (std::is_same_v<Vector, float3>) {
			if (BC_mask) {
				return ECS_TRIANGLE_SLIVER_BC;
			}
		}

		if constexpr (std::is_same_v<Vector, float3>) {
			return ECS_TRIANGLE_SLIVER_NONE;
		}
		else {
			// Combine all the masks
			Vec8ui none_splat = ECS_TRIANGLE_SLIVER_NONE;
			Vec8ui AB_splat = ECS_TRIANGLE_SLIVER_AB;
			Vec8ui AC_splat = ECS_TRIANGLE_SLIVER_AC;
			Vec8ui BC_splat = ECS_TRIANGLE_SLIVER_BC;

			Vec8ui none_ab_result = select(AB_mask, AB_splat, none_splat);
			Vec8ui none_ab_ac_result = select(AC_mask, AC_splat, none_ab_result);
			return select(BC_mask, BC_splat, none_ab_ac_result);
		}
	}

	ECS_TRIANGLE_SLIVER_TYPE IsTriangleSliver(float3 A, float3 B, float3 C, float squared_edge_ratio) {
		return IsTriangleSliverImpl<ECS_TRIANGLE_SLIVER_TYPE>(A, B, C, squared_edge_ratio);
	}

	Vec8ui ECS_VECTORCALL IsTriangleSliver(Vector3 A, Vector3 B, Vector3 C, Vec8f squared_edge_ratio) {
		return IsTriangleSliverImpl<Vec8ui>(A, B, C, squared_edge_ratio);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename ReturnType, typename Vector>
	static ECS_INLINE ReturnType ECS_VECTORCALL IsTriangleSliverByAngleImpl(Vector A, Vector B, Vector C, typename Vector::T degrees) {
		// PERFORMANCE TODO: Should we use fast normalization? Probably worth

		// In order to avoid using acos, we can calculate the degrees cos and then compare against it
		// All cosines that are larger than this one signal a sliver triangle. It works for obtuse angles
		// Also, have some early exists for the scalar version
		auto degrees_cos = cos(degrees);

		// In order to calculate the angles, we need to have the edges normalized
		Vector AB_normalized = Normalize(B - A);
		Vector AC_normalized = Normalize(C - A);
		// Calculate the angle here
		auto AB_AC_angle_cos = Dot(AB_normalized, AC_normalized);
		auto BC_mask = AB_AC_angle_cos > degrees_cos;
		if constexpr (std::is_same_v<Vector, float3>) {
			if (BC_mask) {
				return ECS_TRIANGLE_SLIVER_BC;
			}
		}

		Vector BC_normalized = Normalize(C - B);
		auto AB_BC_angle_cos = Dot(AB_normalized, BC_normalized);
		auto AC_mask = AB_BC_angle_cos > degrees_cos;
		if constexpr (std::is_same_v<Vector, float3>) {
			if (AC_mask) {
				return ECS_TRIANGLE_SLIVER_AC;
			}
		}

		auto AC_BC_angle_cos = Dot(AC_normalized, BC_normalized);
		auto AB_mask = AC_BC_angle_cos > degrees_cos;
		if constexpr (std::is_same_v<Vector, float3>) {
			if (AB_mask) {
				return ECS_TRIANGLE_SLIVER_AB;
			}
			else {
				return ECS_TRIANGLE_SLIVER_NONE;
			}
		}
		else {
			// Combine all the masks
			Vec8ui none_splat = ECS_TRIANGLE_SLIVER_NONE;
			Vec8ui AB_splat = ECS_TRIANGLE_SLIVER_AB;
			Vec8ui AC_splat = ECS_TRIANGLE_SLIVER_AC;
			Vec8ui BC_splat = ECS_TRIANGLE_SLIVER_BC;

			Vec8ui none_ab_result = select(AB_mask, AB_splat, none_splat);
			Vec8ui none_ab_ac_result = select(AC_mask, AC_splat, none_ab_result);
			return select(BC_mask, BC_splat, none_ab_ac_result);
		}
	}

	ECS_TRIANGLE_SLIVER_TYPE IsTriangleSliverByAngle(float3 A, float3 B, float3 C, float degrees) {
		return IsTriangleSliverByAngleImpl<ECS_TRIANGLE_SLIVER_TYPE>(A, B, C, degrees);
	}

	Vec8ui ECS_VECTORCALL IsTriangleSliverByAngle(Vector3 A, Vector3 B, Vector3 C, Vec8f degrees) {
		return IsTriangleSliverByAngleImpl<Vec8ui>(A, B, C, degrees);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename Integer, typename Integer3>
	static void TriangulateFaceImpl(Stream<Integer> point_indices, AdditionStream<Integer3> triangle_indices) {
		//    4 - 3      |       5 - 4      |       4 - 3
		//   /     \     |      /      \    |      /     \
 		//  5       2    |     6        3   |     5       \
		//   \     /     |     |        |   |     |        2
		//    0 - 1      |     7        2   |     6       /
		//						\      /    |      \     /
		//                        0 - 1     |       0 - 1
		// An easy way to triangulate any planar face is by creating a face
		// for each 3 consecutive points in the original array. The first and
		// Last points are added to a new candidate list from which additional
		// Triangles will be later on generated
		// For the hexagon, the algorithm will generate the triangle
		// 012, then 234 and then 450, finishing the triangulation
		// For the octagon, the algorithm will generate 012, 234,
		// 456, 670, then a parallelogram formed by 0246 will remain
		// In the center. This shape will be recursively triangulated
		// For the septagon, the triangles are 012, 234, 456 and then 460,
		// which is a special case. The remaining zone, 024, will be recursively
		// Triangulated like in octagon case

		// Use special cases for 3, 4 and 5 points, since it is faster
		// Than a general algorithm
		if (point_indices.size == 3) {
			triangle_indices.Add({ point_indices[0], point_indices[1], point_indices[2] });
		}
		else if (point_indices.size == 4) {
			triangle_indices.Add({ point_indices[0], point_indices[1], point_indices[2] });
			triangle_indices.Add({ point_indices[2], point_indices[3], point_indices[0] });
		}
		else if (point_indices.size == 5) {
			triangle_indices.Add({ point_indices[0], point_indices[1], point_indices[2] });
			triangle_indices.Add({ point_indices[2], point_indices[3], point_indices[4] });
			triangle_indices.Add({ point_indices[4], point_indices[0], point_indices[2] });
		}
		else if (point_indices.size > 5) {
			ECS_STACK_CAPACITY_STREAM(Integer, internal_points, 512);
			for (size_t index = 2; index < point_indices.size; index += 2) {
				triangle_indices.Add({ point_indices[index - 2], point_indices[index - 1], point_indices[index] });
				internal_points.Add(point_indices[index - 2]);
			}
			if (point_indices.size % 2 == 1) {
				// There is an extra triangle to be added
				size_t count = point_indices.size;
				triangle_indices.Add({ point_indices[count - 3], point_indices[count - 1], point_indices[0] });
			}
			// Internal points is going to be larger than 3, since for the hexagon we have 3 internal points
			TriangulateFaceImpl<Integer>(internal_points, triangle_indices);
		}
	}

	void TriangulateFace(Stream<unsigned int> point_indices, AdditionStream<uint3> triangle_indices) {
		return TriangulateFaceImpl(point_indices, triangle_indices);
	}

	void TriangulateFace(Stream<unsigned short> point_indices, AdditionStream<ushort3> triangle_indices) {
		return TriangulateFaceImpl(point_indices, triangle_indices);
	}

	// --------------------------------------------------------------------------------------------------

}