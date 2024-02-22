#pragma once
#include "ECSEngineMath.h"
#include "Export.h"
#include "ConvexHull.h"

COLLISIONDETECTION_API TriangleMesh GiftWrappingTriangleMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator);

COLLISIONDETECTION_API ConvexHull GiftWrappingConvexHull(Stream<float3> vertex_positions, AllocatorPolymorphic allocator);