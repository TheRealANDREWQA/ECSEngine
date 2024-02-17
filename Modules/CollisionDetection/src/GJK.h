#pragma once
#include "Export.h"
#include "ECSEngineContainers.h"
#include "ConvexHull.h"

extern float3 GJK_SIMPLEX[4];
extern int GJK_SIMPLEX_COUNT;
extern float3 GJK_FIRST[4];
extern float3 GJK_SECOND[4];
extern float3 GJK_MK[50];
extern int GJK_MK_COUNT;
extern float3 GJK_A;
extern float3 GJK_B;

struct GJKSimplex {
	ECS_INLINE void Add(float3 point) {
		points[point_count++] = point;
	}

	ECS_INLINE bool Exists(float3 point) const {
		for (unsigned int index = 0; index < point_count; index++) {
			if (points[index] == point) {
				return true;
			}
		}
		return false;
	}

	float3 points[4];
	unsigned int point_count;
};

extern GJKSimplex GJK_SIMPLICES[50];
extern int GJK_SIMPLICES_COUNT;

// Returns true if the objects intersect, else false. This version is quicker than the distance one
COLLISIONDETECTION_API bool GJKBool(const ConvexHull* collider_a, const ConvexHull* collider_b);

// Returns true if the objects intersect, else false. This version is quicker than the distance one
COLLISIONDETECTION_API bool GJKPointBool(const ConvexHull* collider_a, float3 point);

// Returns true if the objects intersect, else false. This version is quicker than the distance one
COLLISIONDETECTION_API bool GJKBool(const TriangleMesh* collider_a, const TriangleMesh* collider_b);

// Returns the distance between the convex hulls if they are separate, else
// A negative value if they are intersecting
COLLISIONDETECTION_API float GJK(const ConvexHull* collider_a, const ConvexHull* collider_b);

// Returns the distance between the convex hull and the point, if the point
// Lies outside the convex hull, else a negative value if they are intersecting
COLLISIONDETECTION_API float GJKPoint(const ConvexHull* collider_a, float3 point);

// Returns the distance between the convex hulls if they are separate, else
// A negative value if they are intersecting
COLLISIONDETECTION_API float GJK(const TriangleMesh* collider_a, const TriangleMesh* collider_b);