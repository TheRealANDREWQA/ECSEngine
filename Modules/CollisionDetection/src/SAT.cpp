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

		if (distance > largest_distance) {
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
	Vec8f zero_vector = SingleZeroVector<Vec8f>();

	//Vector3 N_ab = Cross(A, B);
	//Vector3 N_cd = Cross(C, D);
	
	Vector3 N_ab = AB_normal;
	Vector3 N_cd = CD_normal;

	Vec8f t1_1 = Dot(C, N_ab);
	Vec8f t1_2 = Dot(D, N_ab);
	Vec8f t2_1 = Dot(A, N_cd);
	Vec8f t2_2 = Dot(B, N_cd);

	SIMDVectorMask t1_mask = (t1_1 * t1_2) < zero_vector;
	SIMDVectorMask t2_mask = (t2_1 * t2_2) < zero_vector;
	SIMDVectorMask hemisphere_mask = (t2_2 * t1_1) > zero_vector;

	return t1_mask && t2_mask && hemisphere_mask;
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

	// Preallocate these buffers such that we can write into them
	// Using a scalar loop
	float second_edge_face_1_normal_x[SIMD_COUNT];
	float second_edge_face_1_normal_y[SIMD_COUNT];
	float second_edge_face_1_normal_z[SIMD_COUNT];

	float second_edge_face_2_normal_x[SIMD_COUNT];
	float second_edge_face_2_normal_y[SIMD_COUNT];
	float second_edge_face_2_normal_z[SIMD_COUNT];

	float second_edge_cross_x[SIMD_COUNT];
	float second_edge_cross_y[SIMD_COUNT];
	float second_edge_cross_z[SIMD_COUNT];
	
	// We can speed this up by using SIMD to test multiple edges at once
	// What we can do is to run a loop where we generate a compressed
	// Bit mask of the edges that should be tested. Then we can
	// Perform the distance check for those edges that actually need it
	// PERFORMANCE TODO: Perform this secondary check using SIMD?
	// At the moment we are using scalar code
	ECS_STACK_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16);
	size_t bit_mask_size = SlotsFor(second_edge_count, 8);
	unsigned char* bit_mask = (unsigned char*)stack_allocator.Allocate(sizeof(unsigned char) * bit_mask_size);

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
				float3 second_edge_face_1_normal = second->faces[second_edge.face_1_index].plane.normal;
				second_edge_face_1_normal = -second_edge_face_1_normal;
				second_edge_face_1_normal_x[write_simd_index] = second_edge_face_1_normal.x;
				second_edge_face_1_normal_y[write_simd_index] = second_edge_face_1_normal.y;
				second_edge_face_1_normal_z[write_simd_index] = second_edge_face_1_normal.z;

				float3 second_edge_face_2_normal = second->faces[second_edge.face_2_index].plane.normal;
				second_edge_face_2_normal = -second_edge_face_2_normal;
				second_edge_face_2_normal_x[write_simd_index] = second_edge_face_2_normal.x;
				second_edge_face_2_normal_y[write_simd_index] = second_edge_face_2_normal.y;
				second_edge_face_2_normal_z[write_simd_index] = second_edge_face_2_normal.z;

				float3 second_edge_cross = second->GetPoint(second_edge.point_2) - second->GetPoint(second_edge.point_1);
				second_edge_cross_x[write_simd_index] = second_edge_cross.x;
				second_edge_cross_y[write_simd_index] = second_edge_cross.y;
				second_edge_cross_z[write_simd_index] = second_edge_cross.z;
			}

			Vector3 second_edge_face_1_normal = Vector3().Load(second_edge_face_1_normal_x, second_edge_face_1_normal_y, second_edge_face_1_normal_z);
			Vector3 second_edge_face_2_normal = Vector3().Load(second_edge_face_2_normal_x, second_edge_face_2_normal_y, second_edge_face_2_normal_z);
			Vector3 second_edge_cross = Vector3().Load(second_edge_cross_x, second_edge_cross_y, second_edge_cross_z);
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
			mask.WriteCompressedMask(bit_mask + second_index / 8);
		}
	}
}

SATQuery SAT(const ConvexHull* first, const ConvexHull* second) {

}