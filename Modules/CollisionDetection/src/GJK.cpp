#include "pch.h"
#include "GJK.h"

// This can be at max a 3D Simplex - a tetrahedron

float3 GJK_FIRST[4];
float3 GJK_SECOND[4];
int GJK_COUNT = 0;

float3 GJK_MK[50];
int GJK_MK_COUNT = 0;

GJKSimplex GJK_SIMPLICES[50];
int GJK_SIMPLICES_COUNT;

float3 GJK_A_INTERNAL;
float3 GJK_B_INTERNAL;

float3 GJK_A;
float3 GJK_B;

// Returns true if the directions are in the same orientation,
// That meaning their dot product is positive
static bool SameOrientation(float3 a, float3 b) {
	return Dot(a, b) > 0.0f;
}

// Returns a point on the Minkowski difference based on the most distant point in that direction
static float3 SupportFunction(const ConvexHull* collider_a, const ConvexHull* collider_b, float3 direction) {
	float3 a_point = collider_a->FurthestFrom(direction);
	float3 b_point = collider_b->FurthestFrom(-direction);
	return a_point - b_point;
}

static float3 SupportFunction(const ConvexHull* convex_hull, const float3* point, float3 direction) {
	float3 convex_point = convex_hull->FurthestFrom(direction);
	return convex_point - *point;
}

static float3 SupportFunction(const TriangleMesh* collider_a, const TriangleMesh* collider_b, float3 direction) {
	float3 a_point = collider_a->FurthestFrom(direction);
	float3 b_point = collider_b->FurthestFrom(-direction);
	if (GJK_COUNT < 4) {
		GJK_FIRST[GJK_COUNT] = a_point;
		GJK_SECOND[GJK_COUNT++] = b_point;
	}
	GJK_A_INTERNAL = a_point;
	GJK_B_INTERNAL = b_point;
	return a_point - b_point;
}

// For all functions, use the squared distance such that we don't have to calculate the square root
// Every single time, which saves time in case they are intersecting, and for the non intersecting
// Case, the distance can be sqrt'ed before returning it

#pragma region Boolean Functions

static bool LineSimplex(GJKSimplex* simplex, float3* new_direction) {
	// The 2 points are B and A (A is the last one added)
	// We need to check to see if we are in the AB region, or in the A region only
	float3 A = simplex->points[1];
	float3 B = simplex->points[0];

	float3 AB = B - A;
	float3 AO = -A;
	if (SameOrientation(AB, AO)) {
		// Edge case
		// There is a special case here. It can happen that the AB points contain
		// The origin and when the triple product is calculated, it will result in
		// a zero direction vector. This is an extremely edge case, but for the sake
		// Of completeness include it. We have 2 ways of checking it. If the new direction
		// Is 0.0f or if the cross product between AB and AO is 0.0f. Use the cross product
		// Version just in case we get really small valid directions that would be falsely catalogued
		// As 0.0f when they are not. We need that cross product anyway
		float3 first_cross = Cross(AB, AO);
		if (CompareMask(first_cross, float3::Splat(0.0f), float3::Splat(0.0000001f))) {
			return true;
		}
		*new_direction = Cross(first_cross, AB);
	}
	else {
		// Point A case
		// Need to eliminate B
		simplex->points[0] = simplex->points[1];
		simplex->point_count = 1;
		*new_direction = AO;
	}

	return false;
}

// For the length version, we need the complete regions. For a boolean version, we don't need that
static bool TriangleSimplex(GJKSimplex* simplex, float3* new_direction, float3 triangle_normal) {
	// 3 points, A, B, C with A and B are the last ones added
	// We must test the AB and AC edges. If it is not in one of them,
	// Then we need to see if it is above or bellow the triangle.
	// In case it is an edge case, we need to remove the 3rd point from
	// The simplex and continue with the line case
	float3 A = simplex->points[2];
	float3 B = simplex->points[1];
	float3 C = simplex->points[0];

	float3 AB = B - A;
	float3 AC = C - A;
	float3 AO = -A;

	float3 ABC_normal = triangle_normal;

	// Test the AC edge
	if (SameOrientation(Cross(ABC_normal, AC), AO)) {
		if (SameOrientation(AC, AO)) {
			// Remove B since it doesn't help
			simplex->points[1] = A;
			simplex->point_count = 2;
			*new_direction = TripleProduct(AC, AO);
		}
		else {
			// Use the AB edge case
			simplex->points[0] = B;
			simplex->points[1] = A;
			simplex->point_count = 2;
			return LineSimplex(simplex, new_direction);
		}
	}
	else {
		// Test the AB edge, if we on its side, we can return the same
		// AB simplex case
		if (SameOrientation(Cross(AB, ABC_normal), AO)) {
			simplex->points[0] = B;
			simplex->points[1] = A;
			simplex->point_count = 2;
			return LineSimplex(simplex, new_direction);
		}
		else {
			// We are inside the triangle
			// Branch based on if the origin is "bellow" or "above" the triangle plane
			if (SameOrientation(ABC_normal, AO)) {
				*new_direction = ABC_normal;
			}
			else {
				// We must invert the triangle order here - swap B and C
				simplex->points[0] = B;
				simplex->points[1] = C;
				*new_direction = -ABC_normal;
			}
		}
	}

	return false;
}

static bool TriangleSimplex(GJKSimplex* simplex, float3* new_direction) {
	float3 A = simplex->points[2];
	float3 B = simplex->points[1];
	float3 C = simplex->points[0];
	return TriangleSimplex(simplex, new_direction, Cross(B - A, C - A));
}

static bool TetrahedronSimplex(GJKSimplex* simplex, float3* new_direction) {
	// ABCD tetrahedron. Determine which face the origin is facing and then
	// Return the triangle simplex case. Otherwise we are inside the tetrahedron
	// And we know that there is an intersection
	float3 A = simplex->points[3];
	float3 B = simplex->points[2];
	float3 C = simplex->points[1];
	float3 D = simplex->points[0];

	float3 AB = B - A;
	float3 AC = C - A;
	float3 AD = D - A;
	float3 AO = -A;

	float3 ABC_normal = Cross(AB, AC);
	float3 ACD_normal = Cross(AC, AD);
	float3 ADB_normal = Cross(AD, AB);

	// Test the ABC face
	if (SameOrientation(ABC_normal, AO)) {
		simplex->points[0] = C;
		simplex->points[1] = B;
		simplex->points[2] = A;
		simplex->point_count = 3;
		return TriangleSimplex(simplex, new_direction, ABC_normal);
	}

	// Test the ACD face
	if (SameOrientation(ACD_normal, AO)) {
		simplex->points[0] = D;
		simplex->points[1] = C;
		simplex->points[2] = A;
		simplex->point_count = 3;
		return TriangleSimplex(simplex, new_direction, ACD_normal);
	}

	// Test the ADB face
	if (SameOrientation(ADB_normal, AO)) {
		simplex->points[0] = B;
		simplex->points[1] = D;
		simplex->points[2] = A;
		simplex->point_count = 3;
		return TriangleSimplex(simplex, new_direction, ADB_normal);
	}

	// We don't have to test the BCD face since we know the origin cannot be there
	return true;
}

// Returns true if the simplex encloses the origin, that is there is an intersection,
// Else false
static bool HandleSimplex(GJKSimplex* simplex, float3* new_direction) {
	switch (simplex->point_count) {
	case 2:
	{
		return LineSimplex(simplex, new_direction);
	}
	break;
	case 3:
	{
		return TriangleSimplex(simplex, new_direction);
	}
	break;
	case 4:
	{
		return TetrahedronSimplex(simplex, new_direction);
	}
	break;
	}

	return false;
}

template<typename FirstCollider, typename SecondCollider>
static bool GJKImpl(const FirstCollider* collider_a, const SecondCollider* collider_b) {
	// Record all the points we have been through
	// Once we reach a point that it was previously seen and we detected
	// A point that indicates a non intersection, we can stop and return
	// The minimum length
	ECS_STACK_CAPACITY_STREAM(float3, checked_points, 250);

	// Initialize the simplex with a random point from the minkowski difference
	GJKSimplex simplex;
	simplex.points[0] = SupportFunction(collider_a, collider_b, float3{ 1.0f, 0.0f, 0.0f });
	simplex.point_count = 1;

	checked_points.Add(simplex.points[0]);

	// The first search direction is from the initial point to the origin
	bool found_invert_orientation = false;
	float3 search_direction = -simplex.points[0];
	while (true) {
		bool zero_direction = CompareMask(search_direction, float3::Splat(0.0f));
		if (zero_direction) {
			// If we ever get to a zero direction, we can't determine what to do
			// So terminate with intersection since this happens mostly when the
			// Point is on an edge
			return true;
		}

		// Get the point on the MD (Minkowski difference) using the support function
		// In the search direction
		float3 new_point = SupportFunction(collider_a, collider_b, search_direction);

		// Check to see if this new point is past the origin in the direction of the search direction
		// Use the dot product between the search direction and the direction from the point to the origin
		if (!SameOrientation(search_direction, new_point)) {
			// The point is not past the origin, we can exit now
			return false;
		}
		else {
			// The point is past the origin
			// We must handle the simplex now
			// Add the point
			simplex.Add(new_point);
			if (HandleSimplex(&simplex, &search_direction)) {
				// They are intersecting
				return true;
			}
		}
	}

	// Shouldn't be reached - exiting from the while
	return false;
}

#pragma endregion

#pragma region Distance functions

template<int simplex_dimension, typename SimplexRegion>
static float SimplexDistanceImpl(GJKSimplex* simplex, float3* new_direction, const SimplexRegion& voronoi_region) {
	float3 origin = float3::Splat(0.0f);
	if (CompareMask(voronoi_region.projection, origin)) {
		// Signal an intersection
		return 0.0f;
	}

	// Only for the vertex case we need to do something
	switch (voronoi_region.type) {
	case ECS_SIMPLEX_VORONOI_VERTEX:
	{
		simplex->points[0] = voronoi_region.points[0];
		simplex->point_count = 1;
	}
	break;
	case ECS_SIMPLEX_VORONOI_EDGE:
	{
		if constexpr (simplex_dimension >= 2) {
			simplex->points[0] = voronoi_region.points[0];
			simplex->points[1] = voronoi_region.points[1];
			simplex->point_count = 2;
		}
	}
	break;
	case ECS_SIMPLEX_VORONOI_FACE:
	{
		if constexpr (simplex_dimension >= 3) {
			simplex->points[0] = voronoi_region.points[0];
			simplex->points[1] = voronoi_region.points[1];
			simplex->points[2] = voronoi_region.points[2];
			simplex->point_count = 3;
		}
	}
	break;
	}

	*new_direction = -Normalize(voronoi_region.projection);
	return SquareLength(voronoi_region.projection);
}

// Returns the squared distance
static float LineSimplexDistance(GJKSimplex* simplex, float3* new_direction) {
	float3 origin = float3::Splat(0.0f);
	Simplex1DVoronoiRegion voronoi_region = CalculateSimplex1DVoronoiRegion(simplex->points[0], simplex->points[1], origin);
	return SimplexDistanceImpl<1>(simplex, new_direction, voronoi_region);
}

static float TriangleSimplexDistance(GJKSimplex* simplex, float3* new_direction) {
	float3 origin = float3::Splat(0.0f);
	Simplex2DVoronoiRegion voronoi_region = CalculateSimplex2DVoronoiRegion(simplex->points[0], simplex->points[1], simplex->points[2], origin);
	return SimplexDistanceImpl<2>(simplex, new_direction, voronoi_region);
}

static float TetrahedronSimplexDistance(GJKSimplex* simplex, float3* new_direction) {
	float3 origin = float3::Splat(0.0f);
	Simplex3DVoronoiRegion voronoi_region = CalculateSimplex3DVoronoiRegion(simplex->points[0], simplex->points[1], simplex->points[2], simplex->points[3], origin);
	return SimplexDistanceImpl<3>(simplex, new_direction, voronoi_region);
}

// Returns the squared distance from the simplex to the origin
static float HandleSimplexDistance(GJKSimplex* simplex, float3* new_direction) {
	switch (simplex->point_count) {
	case 2:
	{
		return LineSimplexDistance(simplex, new_direction);
	}
	break;
	case 3:
	{
		return TriangleSimplexDistance(simplex, new_direction);
	}
	break;
	case 4:
	{
		return TetrahedronSimplexDistance(simplex, new_direction);
	}
	break;
	}

	// Shouldn't be reached
	return FLT_MAX;
}

template<typename FirstCollider, typename SecondCollider>
static float GJKDistanceImpl(const FirstCollider* collider_a, const SecondCollider* collider_b) {
	// Initialize the simplex with a random point from the minkowski difference
	GJKSimplex simplex;
	simplex.points[0] = SupportFunction(collider_a, collider_b, float3{ 1.0f, 0.0f, 0.0f });
	simplex.point_count = 1;

	// Use at most 50 iterations. If after so many iterations we couldn't decide the outcome,
	// It most likely is not intersecting but the distance might not be totally accurate. But
	// This should never happen in practice
	ECS_STACK_CAPACITY_STREAM(float3, verified_points, 50);
	verified_points.Add(simplex.points[0]);

	// The first search direction is from the initial point to the origin
	float3 search_direction = -simplex.points[0];
	bool degenerate_case = false;
	unsigned int degenerate_case_retries = 5;
	float minimum_squared_distance = SquareLength(simplex.points[0]);
	while (verified_points.size < verified_points.capacity || degenerate_case_retries == 0) {
		bool zero_direction = CompareMask(search_direction, float3::Splat(0.0f));
		if (zero_direction) {
			// This is a degenerate case when it is contained on an edge
			// We can simply return as colliding
			return -1.0f;
		}

		// Get the point on the MD (Minkowski difference) using the support function
		// In the search direction
		float3 new_point = SupportFunction(collider_a, collider_b, search_direction);
		bool exists = simplex.Exists(new_point);
		if (!exists) {
			if (!degenerate_case) {
				for (unsigned int index = 0; index < verified_points.size; index++) {
					if (verified_points[index] == new_point) {
						degenerate_case = true;
						break;
					}
				}
			}
		}
		if (degenerate_case) {
			degenerate_case_retries--;
		}

		if (exists) {
			return sqrt(minimum_squared_distance);
		}
		else {
			// The point is past the origin
			// We must handle the simplex now

			// Handle degenerate cases. If the simplex has 2 points, test if these 3 are collinear,
			// And in that case, replace one of the points with the current one. For the 3 point case,
			// Test if the points are coplanar. This degenerate case should trigger found_existing, since
			// This indicates that we cannot make further progress and that most likely we should end soon
			if (simplex.point_count == 2) {
				if (IsPointCollinear(simplex.points[0], simplex.points[1], new_point)) {
					degenerate_case = true;
					simplex.points[0] = new_point;
				}
				else {
					simplex.Add(new_point);
				}
			}
			else if (simplex.point_count == 3) {
				if (ArePointsCoplanar(simplex.points[0], simplex.points[1], simplex.points[2], new_point)) {
					PlaneScalar plane = ComputePlane(simplex.points[0], simplex.points[1], simplex.points[2]);
					float distance_to_plane = DistanceToPlane(plane, new_point);

					degenerate_case = true;
					simplex.points[0] = new_point;
				}
				else {
					simplex.Add(new_point);
				}
			}
			else {
				simplex.Add(new_point);
			}
			verified_points.Add(new_point);
			float current_squared_distance = HandleSimplexDistance(&simplex, &search_direction);
			if (current_squared_distance == 0.0f) {
				// They are intersecting
				return -1.0f;
			}
				
			// If this distance stops getting smaller, then we cannot enclose the origin
			// With our simplex. But due to errors, we cannot return right now. We need
			// To signal the degenerate case
			if (current_squared_distance > minimum_squared_distance) {
				degenerate_case = true;
			}

			minimum_squared_distance = std::min(current_squared_distance, minimum_squared_distance);
		}
	}

	// Shouldn't be reached
	return sqrt(minimum_squared_distance);
}

#pragma endregion

float3 GJK_SIMPLEX[4];
int GJK_SIMPLEX_COUNT = 0;

bool GJKBool(const ConvexHull* collider_a, const ConvexHull* collider_b) {
	return GJKImpl(collider_a, collider_b);
}

bool GJKPointBool(const ConvexHull* collider_a, float3 point) {
	return GJKImpl(collider_a, &point);
}

bool GJKBool(const TriangleMesh* collider_a, const TriangleMesh* collider_b) {
	return GJKImpl(collider_a, collider_b);
}

float GJK(const ConvexHull* collider_a, const ConvexHull* collider_b) {
	return GJKDistanceImpl(collider_a, collider_b);
}

float GJKPoint(const ConvexHull* convex_hull, float3 point) {
	return GJKDistanceImpl(convex_hull, &point);
}

float GJK(const TriangleMesh* collider_a, const TriangleMesh* collider_b)
{
	return GJKDistanceImpl(collider_a, collider_b);
}
