#include "pch.h"
#include "SAT.h"

// Tests the faces of the first hull against the second using SAT
// It returns the face with the minimum amount of penetration when
// The objects are overlapping, and a positive value if they are separated
static SATFaceQuery SATFace(const ConvexHull* first, const ConvexHull* second) {
	unsigned int final_face_index = -1;
	float largest_distance = -FLT_MAX;

	// We could theoretically SIMDize this (which would require the faces to be SoA), but it is not worth the effort
	// The main duration comes from the support function, which already uses SIMD internally
	for (unsigned int index = 0; index < first->faces.size; index++) {
		PlaneScalar face_plane = first->faces[index].plane;
		// Retrieve the support point in the opposite direction of the normal
		// To determine the separation or penetration distance
		float3 support_point = second->FurthestFrom(-face_plane.normal);
		float distance = DistanceToPlane(face_plane, support_point);

		if (distance >= largest_distance) {
			largest_distance = distance;
			final_face_index = index;
		}
	}

	return { largest_distance, final_face_index };
}

// Tests the all combinations of edges from the first with the second
// It uses Gauss map reduction to rapidly prune edges that are not candidates
// In a Gauss map, the center of a face of a hull becomes a point and the edges between
// Connecting faces become great arcs. The way to visualize this is that an 
// edge describes the possible directions from which a feature could come into contact
// Since the face normals are already normalized, the normal can be seen as a vertex
// On a unit sphere. To calculate if 2 great arcs intersect, we have to perform 2 plane
// Tests. For each test, construct a plane through one of the arcs and see if the points
// Are on different sides. Like in this example, the pairs are AB and CD
//          A
//          |
//   C -----|----- D
//          |
//          B
// There is a catch tho. The 2 arcs need to be on the same hemisphere, otherwise
// The test can fail. You can picture this like the vertical AB arc is on the back
// Side of a ball, while the CD arc is at the front. They will pass the test, but
// They do not intersect. To test if the arcs are on the same hemisphere, we can
// Build a plane through BC. If the 2 points are on the same side of the plane,
// They are on the same hemisphere.

// We can perform multiple of these tests using SIMD
// Don't forget that A, B, C and D are actually normals!
// We can implement an efficient test that combines the first 2 checks with the hemisphere
// Check by carefully choosing the operands.
// The plane through AB has normal Cross(A, B) - call it N_ab
// The plane through CD has normal Cross(C, D) - call it N_cd
// The plane through BC has normal Cross(B, C) - call it N_bc
// * means dot product, x cross product
// The first test is (C * N_ab) * (D * N_ab) < 0.0f (we don't have to subtract the plane dot
// Like we would usually do since the plane is at the origin)
// The second test is (A * N_cd) * (B * N_cd) < 0.0f
// The hemisphere test is (A * N_bc) * (D * N_bc) > 0.0f
// We can rewrite A * N_bc = A * (B x C) = C * (A x B) and
// D * N_bc = D * (B x C) = B * (C x D)
// So this test becomes t2_2 (second term from second test) * t1_1 (first term from first test) > 0.0f
// There is another improvement that we can make tho. For a convex polyhedron, the cross product
// Between the normals of 2 adjacent faces is actually the adjacent edge. So we can use a much simple
// Subtraction to obtain the normals instead of using the cross product
static SIMDVectorMask EdgeGaussMapTest(const Vector3& A, const Vector3& B, const Vector3& C, const Vector3& D, const Vector3& AB_normal, const Vector3& CD_normal) {
	//Vec8f zero_tolerance_vector = -0.0000001f;
	Vec8f zero_tolerance_vector = 0.0f;

	// This section of code is left as commented just in case some issues arise
	// And the crossreferencing of values is desired
	Vector3 N_ab_cross = Normalize(Cross(B, A));
	Vector3 N_cd_cross = Normalize(Cross(D, C));
	SIMDVectorMask almost_parallel = IsParallelAngleMask(Normalize(A), Normalize(B), DegToRad(3.0f));
	SIMDVectorMask almost_parallel2 = IsParallelAngleMask(Normalize(C), Normalize(D), DegToRad(3.0f));
	if (horizontal_or(almost_parallel)) {
		N_ab_cross = Select(almost_parallel, Normalize(AB_normal), N_ab_cross);
	}
	if (horizontal_or(almost_parallel2)) {
		N_cd_cross = Select(almost_parallel2, Normalize(CD_normal), N_cd_cross);
	}
	
	Vec8f t1_1_cross = Dot(C, N_ab_cross);
	Vec8f t1_2_cross = Dot(D, N_ab_cross);
	Vec8f t2_1_cross = Dot(A, N_cd_cross);
	Vec8f t2_2_cross = Dot(B, N_cd_cross);

	Vec8f val0_cross = (t1_1_cross * t1_2_cross);
	Vec8f val1_cross = (t2_1_cross * t2_2_cross);
	Vec8f val2_cross = (t2_2_cross * t1_1_cross);

	SIMDVectorMask t1_mask_cross = val0_cross < zero_tolerance_vector;
	SIMDVectorMask t2_mask_cross = val1_cross < zero_tolerance_vector;
	SIMDVectorMask hemisphere_mask_cross = val2_cross > -zero_tolerance_vector;
	SIMDVectorMask result_cross = t1_mask_cross && t2_mask_cross && hemisphere_mask_cross;
	//SIMDVectorMask result = result_cross;

	// We still perform the normalization here since
	// We can get some small values for the normals
	// That would result in faulty values
	Vector3 N_ab = Normalize(AB_normal);
	Vector3 N_cd = Normalize(CD_normal);

	Vec8f t1_1 = Dot(C, N_ab);
	Vec8f t1_2 = Dot(D, N_ab);
	Vec8f t2_1 = Dot(A, N_cd);
	Vec8f t2_2 = Dot(B, N_cd);

	Vec8f t1_val = t1_1 * t1_2;
	Vec8f t2_val = t2_1 * t2_2;
	Vec8f hemisphere_val = t2_2 * t1_1;

	SIMDVectorMask t1_mask = t1_val < zero_tolerance_vector;
	SIMDVectorMask t2_mask = t2_val < zero_tolerance_vector;
	SIMDVectorMask hemisphere_mask = hemisphere_val > -zero_tolerance_vector;
	SIMDVectorMask result = t1_mask && t2_mask && hemisphere_mask;

	return result;
}

// The distance between the 2 edges can be found out by constructing a plane through one of the edges
// That has the normal as the cross product of the 2 edges. The distance is simply the distance
// From one of the points of the second edge to that plane, since we know that the second edge is
// A support edge
struct EdgeDistanceResult {
	Vec8f distance;
	Vector3 separating_axis;
};

static EdgeDistanceResult ECS_VECTORCALL EdgeDistance(
	const Vector3& first_edge_1, 
	const Vector3& first_edge_normalized_direction, 
	const Vector3& second_edge_1, 
	const Vector3& second_edge_2, 
	const Vector3& first_hull_center
) {
	// Test parallel edges and "skip" them by assigning the distance
	// -FLT_MAX such that they won't get considered
	Vec8f parallel_value = -FLT_MAX;

	// PERFORMANCE TODO: Use the angle version of is parallel? The epsilon based
	// One is faster but it is hard to find a correct epsilon
	Vector3 second_direction = second_edge_2 - second_edge_1;
	Vector3 normalized_second_direction = Normalize(second_direction);

	SIMDVectorMask is_parallel_mask = IsParallelMask(first_edge_normalized_direction, normalized_second_direction);

	Vector3 plane_normal = Normalize(Cross(first_edge_normalized_direction, normalized_second_direction));
	// Redirect the normal away from the first hull center
	Vec8f dot_value = Dot(plane_normal, first_edge_1 - first_hull_center);
	SIMDVectorMask is_facing_hull_center = Dot(plane_normal, first_edge_1 - first_hull_center) < SingleZeroVector<Vec8f>();
	plane_normal = Select(is_facing_hull_center, -plane_normal, plane_normal);

	Vec8f distance = Dot(plane_normal, second_edge_1 - first_edge_1);
	return { SelectSingle(is_parallel_mask, parallel_value, distance), plane_normal };
}

// Combines maximum update with the EdgeDistance call
static void EdgeDistanceUpdate(
	const Vector3& first_edge_1,
	const Vector3& first_edge_normalized_direction,
	const Vector3& second_edge_1,
	const Vector3& second_edge_2,
	const Vector3& first_hull_center,
	const unsigned int* second_edge_indices,
	float& maximum_distance,
	unsigned int& maximum_distance_index,
	float3& maximum_separating_axis
) {
	EdgeDistanceResult result = EdgeDistance(first_edge_1, first_edge_normalized_direction, second_edge_1, second_edge_2, first_hull_center);
	
	// We need to record the largest distance
	Vec8f current_maximum_distance = HorizontalMax8(result.distance);
	float current_maximum_distance_scalar = VectorLow(current_maximum_distance);
	if (current_maximum_distance_scalar > maximum_distance) {
		size_t current_maximum_distance_index = HorizontalMax8Index(result.distance, current_maximum_distance);
		maximum_distance = current_maximum_distance_scalar;
		maximum_distance_index = second_edge_indices[current_maximum_distance_index];
		maximum_separating_axis = result.separating_axis.At(current_maximum_distance_index);
	}
}

static SATEdgeQuery SATEdge(const ConvexHull* first, const ConvexHull* second) {
	constexpr size_t SIMD_COUNT = Vector3::ElementCount();
	static_assert(SIMD_COUNT == 8, "The SAT bit mask needs to be adjusted");

	unsigned int first_edge_count = first->edges.size;
	unsigned int second_edge_count = second->edges.size;
	unsigned int second_simd_count = GetSimdCount(second_edge_count, SIMD_COUNT);
	if (second_simd_count < second_edge_count) {
		// Add another iteration, we will deal with the count in the loop
		second_simd_count += SIMD_COUNT;
	}

	// We write into these using a scalar loop
	Vector3 second_edge_face_1_normal;
	Vector3 second_edge_face_2_normal;
	Vector3 second_edge_cross;

	Vector3 second_edge_point_1;
	Vector3 second_edge_point_2;
	
	// We can speed this up by using SIMD to test multiple edges at once
	// What we can do is to run a loop where we generate a compressed
	// Bit mask of the edges that should be tested. Then we can
	// Perform the distance check for those edges that actually need it
	// PERFORMANCE TODO: Perform this secondary check using SIMD?
	// At the moment we are using scalar code
	ECS_STACK_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16);
	size_t bit_mask_size = SlotsFor(second_edge_count, 8);
	unsigned char* bit_mask = (unsigned char*)stack_allocator.Allocate(sizeof(unsigned char) * bit_mask_size);

	Vector3 first_hull_center = Vector3::Splat(first->center);

	unsigned int global_first_max_index = -1;
	unsigned int global_second_max_index = -1;
	float global_max_distance = -FLT_MAX;
	float3 global_separation_axis = float3::Splat(0.0f);
	for (unsigned int first_index = 0; first_index < first_edge_count; first_index++) {
		const ConvexHullEdge& first_edge = first->edges[first_index];
		// Get the face normals for this first edge
		Vector3 first_edge_face_1_normal = Vector3::Splat(first->faces[first_edge.face_1_index].plane.normal);
		Vector3 first_edge_face_2_normal = Vector3::Splat(first->faces[first_edge.face_2_index].plane.normal);
		Vector3 first_edge_cross = Vector3::Splat(first->GetPoint(first_edge.point_2) - first->GetPoint(first_edge.point_1));

		for (unsigned int second_index = 0; second_index < second_simd_count; second_index += SIMD_COUNT) {
			// Create the SIMD values in the preallocated buffers
			unsigned int iterate_count = ClampMax(second_edge_count - second_index, (unsigned int)SIMD_COUNT);
			for (unsigned int write_simd_index = 0; write_simd_index < iterate_count; write_simd_index++) {
				const ConvexHullEdge& second_edge = second->edges[second_index + write_simd_index];
				
				// Here we need to negate the normals because we want to construct
				// The Minkowski difference. Leaving the normals as is will result
				// In the Minkowski sum. Negating the normals is like negating the
				// Positions of a convex hull, since the normals represent locations
				// On the unit sphere
				float3 second_edge_face_1_normal_scalar = second->faces[second_edge.face_1_index].plane.normal;
				second_edge_face_1_normal_scalar = -second_edge_face_1_normal_scalar;
				WriteToVector3StorageSoA(&second_edge_face_1_normal, second_edge_face_1_normal_scalar, write_simd_index);

				float3 second_edge_face_2_normal_scalar = second->faces[second_edge.face_2_index].plane.normal;
				second_edge_face_2_normal_scalar = -second_edge_face_2_normal_scalar;
				WriteToVector3StorageSoA(&second_edge_face_2_normal, second_edge_face_2_normal_scalar, write_simd_index);

				// Here, we don't need to negate the cross since the cross of -C x -D = C x D, which
				// Needs to be calibrated by the hull before this test is applied (at build time)
				float3 second_edge_cross_scalar = second->GetPoint(second_edge.point_2) - second->GetPoint(second_edge.point_1);
				WriteToVector3StorageSoA(&second_edge_cross, second_edge_cross_scalar, write_simd_index);
			}

			// Perform the gauss test
			VectorMask mask = EdgeGaussMapTest(
				first_edge_face_1_normal, 
				first_edge_face_2_normal, 
				second_edge_face_1_normal, 
				second_edge_face_2_normal, 
				first_edge_cross, 
				second_edge_cross
			);

			// Write the compressed mask to the bit mask array
			mask.WriteCompressedMask(bit_mask + second_index / SIMD_COUNT);
		}

		// Cache the values for the edge 1
		Vector3 first_edge_point_1 = Vector3::Splat(first->GetPoint(first_edge.point_1));
		Vector3 first_edge_point_2 = Vector3::Splat(first->GetPoint(first_edge.point_2));
		Vector3 first_edge_normalized_direction = Normalize(first_edge_point_2 - first_edge_point_1);

		// We can check the masks now and construct the values that are actually needed
		unsigned char mask_bit_index = 1 << 0;
		unsigned int mask_byte_index = 0;
		unsigned int current_count = 0;
		float edge_maximum_separation = -FLT_MAX;
		unsigned int edge_maximum_separation_index = -1;
		unsigned int second_edge_indices[Vector3::ElementCount()];
		float3 edge_separation_axis;
		for (unsigned int second_index = 0; second_index < second_edge_count; second_index++) {
			bool perform_check = (bit_mask[mask_byte_index] & mask_bit_index) != 0;
			if (perform_check) {
				const ConvexHullEdge& second_edge = second->edges[second_index];
				WriteToVector3StorageSoA(&second_edge_point_1, second->GetPoint(second_edge.point_1), current_count);
				WriteToVector3StorageSoA(&second_edge_point_2, second->GetPoint(second_edge.point_2), current_count);
				second_edge_indices[current_count] = second_index;
				current_count++;
				if (current_count == Vector3::ElementCount()) {
					// We can make a call
					EdgeDistanceUpdate(
						first_edge_point_1, 
						first_edge_normalized_direction, 
						second_edge_point_1, 
						second_edge_point_2, 
						first_hull_center, 
						second_edge_indices, 
						edge_maximum_separation, 
						edge_maximum_separation_index,
						edge_separation_axis
					);
					current_count = 0;
				}
			}
			if (mask_bit_index == ECS_BIT(7)) {
				mask_bit_index = 1;
				mask_byte_index++;
			}
			else {
				mask_bit_index <<= 1;
			}
		}

		// If we have remainder edges, we need to test them again
		// Padd the remainder elements with edge 1 values such that it
		// Will fail the test as parallel, which means they won't get considered
		if (current_count > 0) {
			Vec8fb padd_mask = CastToFloat(SelectMaskLast<unsigned int>(Vector3::ElementCount() - current_count));
			second_edge_point_1 = Select(padd_mask, first_edge_point_1, second_edge_point_1);
			second_edge_point_2 = Select(padd_mask, first_edge_point_2, second_edge_point_2);
			EdgeDistanceUpdate(
				first_edge_point_1,
				first_edge_normalized_direction,
				second_edge_point_1,
				second_edge_point_2,
				first_hull_center,
				second_edge_indices,
				edge_maximum_separation,
				edge_maximum_separation_index,
				edge_separation_axis
			);
		}

		// Update the global if needed
		if (edge_maximum_separation > global_max_distance) {
			global_max_distance = edge_maximum_separation;
			global_first_max_index = first_index;
			global_second_max_index = edge_maximum_separation_index;
			global_separation_axis = edge_separation_axis;
		}
	}

	// No edge was found or parallel ones
	return { global_max_distance, global_first_max_index, global_second_max_index, global_separation_axis };
}

// This the brute force method of finding out the edge separation values
// for the SAT. It is left here as a tool for debugging in case there is
// A doubt against the values generated by the other method to cross reference
// Those values
static SATEdgeQuery SATEdgeProjection(
	const ConvexHull* first,
	const ConvexHull* second
) {
	float global_max_distance = -FLT_MAX;
	float global_squared_edge_distance = FLT_MAX;
	unsigned int global_first_index = -1;
	unsigned int global_second_index = -1;
	for (size_t index = 0; index < first->edges.size; index++) {
		Line3D first_line = first->GetEdgePoints(index);
		float3 edge = first_line.B - first_line.A;
		for (size_t subindex = 0; subindex < second->edges.size; subindex++) {
			Line3D second_line = second->GetEdgePoints(subindex);
			float3 second_edge = second_line.B - second_line.A;

			float3 cross = Cross(Normalize(edge), Normalize(second_edge));
			if (!CompareMask(cross, float3::Splat(0.0f), float3::Splat(0.000001f))) {
				float3 normalized_cross = Normalize(cross);
				// Project first on the cross
				float2 first_projection_range = { FLT_MAX, -FLT_MAX };
				for (size_t first_index = 0; first_index < first->vertex_size; first_index++) {
					float dot = Dot(normalized_cross, first->GetPoint(first_index));
					first_projection_range.x = min(dot, first_projection_range.x);
					first_projection_range.y = max(dot, first_projection_range.y);
				}

				float2 second_projection_range = { FLT_MAX, -FLT_MAX };
				for (size_t second_index = 0; second_index < second->vertex_size; second_index++) {
					float dot = Dot(normalized_cross, second->GetPoint(second_index));
					second_projection_range.x = min(dot, second_projection_range.x);
					second_projection_range.y = max(dot, second_projection_range.y);
				}

				float separation = -FLT_MAX;
				if (IsInRange(first_projection_range.x, second_projection_range.x, second_projection_range.y)) {
					if (IsInRange(first_projection_range.y, second_projection_range.y, second_projection_range.y)) {
						separation = -(first_projection_range.y - first_projection_range.x);
					}
					else {
						separation = -(second_projection_range.y - first_projection_range.x);
					}
				}
				else {
					if (IsInRange(second_projection_range.x, first_projection_range.x, first_projection_range.y)) {
						if (IsInRange(second_projection_range.y, first_projection_range.y, first_projection_range.y)) {
							separation = -(second_projection_range.y - second_projection_range.x);
						}
						else {
							separation = -(first_projection_range.y - second_projection_range.x);
						}
					}
					else {
						if (first_projection_range.y < second_projection_range.x) {
							separation = first_projection_range.y - second_projection_range.x;
						}
						else {
							separation = second_projection_range.y - first_projection_range.x;
						}
					}
				}

				if (separation > global_max_distance) {
					global_max_distance = separation;
					global_first_index = index;
					global_second_index = subindex;
					float squared_distance = SquaredDistanceBetweenSegmentPoints(first_line.A, first_line.B, second_line.A, second_line.B);
					global_squared_edge_distance = squared_distance;
				}
				else if (FloatCompare(separation, global_max_distance, 0.000001f)) {
					// It can happen that multiple edges have a similar or equal value
					// Just to be sure, use a small epsilon to test for this equality
					// In that case, choose the pair that has the least distance between
					// The 2 edges
					float squared_distance = SquaredDistanceBetweenSegmentPoints(first_line.A, first_line.B, second_line.A, second_line.B);
					if (squared_distance < global_squared_edge_distance) {
						global_first_index = index;
						global_second_index = subindex;
						global_squared_edge_distance = squared_distance;
					}
				}
 			}
		}
	}

	return { global_max_distance, global_first_index, global_second_index };
}

SATQuery SAT(const ConvexHull* first, const ConvexHull* second) {
	SATFaceQuery first_face_query = SATFace(first, second);
	// If we have a positive distance, it means that they are separated
	if (first_face_query.distance > 0.0f) {
		return {};
	}

	SATFaceQuery second_face_query = SATFace(second, first);
	if (second_face_query.distance > 0.0f) {
		return {};
	}

	SATEdgeQuery edge_query = SATEdge(first, second);
	
	//SATEdgeQuery projection_edge_query = SATEdgeProjection(first, second);
	//edge_query.edge_2_index = 3;
	//edge_query.edge_1_index = projection_edge_query.edge_1_index;
	//edge_query.edge_2_index = projection_edge_query.edge_2_index;
	//edge_query.distance = projection_edge_query.distance;
	//edge_query = projection_edge_query;
	if (edge_query.distance > 0.0f) {
		return {};
	}
	
	// No separation was found. It means that they are intersecting
	// Prioritize face queries over edge ones
	if (first_face_query.distance >= edge_query.distance /*&& second_face_query.distance >= edge_query.distance*/) {
		SATQuery query;
		query.type = SAT_QUERY_FACE;
		if (first_face_query.distance > second_face_query.distance) {
			query.face = first_face_query;
			query.face.first_collider = true;
		}
		else {
			query.face = second_face_query;
			query.face.first_collider = false;
		}
		return query;
	}

	/*if (second_face_query.distance >= edge_query.distance) {
		SATQuery query;
		query.type = SAT_QUERY_FACE;
		query.face = second_face_query;
		query.face.second_face_index = first_face_query.face_index;
		query.face.first_collider = false;
		return query;
	}*/

	SATQuery query;
	query.type = SAT_QUERY_EDGE;
	query.edge = edge_query;
	return query;
}