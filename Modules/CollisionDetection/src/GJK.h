#pragma once
#include "Export.h"
#include "ECSEngineContainers.h"
#include "ConvexHull.h"

// Returns the distance between the convex hulls if they are separate, else
// A negative value if they are intersecting
COLLISIONDETECTION_API float GJK(const ConvexHull* collider_a, const ConvexHull* collider_b);