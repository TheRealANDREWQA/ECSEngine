#pragma once
#include "Export.h"
#include "ECSEngineContainers.h"
#include "ConvexHull.h"

// Returns the distance between the convex hulls if they are separate, else
// A negative value if they are intersecting
COLLISIONDETECTION_API float GJK(const ConvexHull* collider_a, const ConvexHull* collider_b);

// Returns the distance between the convex hull and the point, if the point
// Lies outside the convex hull, else a negative value if they are intersecting
COLLISIONDETECTION_API float GJKPoint(const ConvexHull* collider_a, float3 point);

// Returns the distance between the convex hulls if they are separate, else
// A negative value if they are intersecting
COLLISIONDETECTION_API float GJK(const TriangleMesh* collider_a, const TriangleMesh* collider_b);