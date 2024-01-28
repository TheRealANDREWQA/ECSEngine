#pragma once
#include "ConvexHull.h"

COLLISIONDETECTION_API ConvexHull Quickhull(Stream<float3> vertex_positions, AllocatorPolymorphic allocator);

// This version is faster since it doesn't need to recalculate the AABB for the vertex positions
COLLISIONDETECTION_API ConvexHull Quickhull(Stream<float3> vertex_positions, AllocatorPolymorphic allocator, AABBScalar aabb);