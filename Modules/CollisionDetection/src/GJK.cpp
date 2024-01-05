#include "pch.h"
#include "GJK.h"

// This can be at max a 3D Simplex - a tetrahedron
struct GJKSimplex {
	ECS_INLINE void Add(float3 point) {
		points[point_count++] = point;
	}

	float3 points[4];
	unsigned int point_count;
};

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

static bool TriangleSimplex(GJKSimplex* simplex, float3* new_direction) {
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

	float3 ABC_normal = Cross(AB, AC);

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
		return TriangleSimplex(simplex, new_direction);
	}

	// Test the ACD face
	if (SameOrientation(ACD_normal, AO)) {
		simplex->points[0] = D;
		simplex->points[1] = C;
		simplex->points[2] = A;
		simplex->point_count = 3;
		return TriangleSimplex(simplex, new_direction);
	}

	// Test the ADB face
	if (SameOrientation(ADB_normal, AO)) {
		simplex->points[0] = B;
		simplex->points[1] = D;
		simplex->points[2] = A;
		simplex->point_count = 3;
		return TriangleSimplex(simplex, new_direction);
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

float GJK(const ConvexHull* collider_a, const ConvexHull* collider_b) {
	// Initialize the simplex with a random point from the minkowski difference
	GJKSimplex simplex;
	simplex.points[0] = SupportFunction(collider_a, collider_b, float3{ 1.0f, 0.0f, 0.0f });
	simplex.point_count = 1;

	// The first search direction is from the initial point to the origin
	float3 search_direction = -simplex.points[0];
	while (true) {
		// Get the point on the MD (Minkowski difference) using the support function
		// In the search direction
		float3 new_point = SupportFunction(collider_a, collider_b, search_direction);

		// Check to see if this new point is past the origin in the direction of the search direction
		// Use the dot product between the search direction and the direction from the point to the origin
		if (!SameOrientation(search_direction, new_point)) {
			// The point is not past the origin, we need to return the distance
			// Between the 2 shapes
			return 1.0f;
		}
		else {
			// The point is past the origin
			// We must handle the simplex now
			// Add the point
			simplex.Add(new_point);
			if (HandleSimplex(&simplex, &search_direction)) {
				// They are intersecting
				return -1.0f;
			}
		}
	}

	// Shouldn't be reached
	return -1.0f;
}