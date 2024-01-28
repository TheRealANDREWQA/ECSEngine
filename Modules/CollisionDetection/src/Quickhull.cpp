#include "pch.h"
#include "Quickhull.h"

// TODO: Discard this? Quickhull 3D requires a lot of work. Let's start with the simpler
// Gift wrapping which can be accelerated with SIMD

//ConvexHull Quickhull(Stream<float3> vertex_positions, AllocatorPolymorphic allocator) {
//	ConvexHull convex_hull;
//
//	float3 extreme_points_storage[6];
//	DetermineExtremePoints(vertex_positions, extreme_points_storage);
//
//	// Eliminate all points identical points
//	Stream<float3> extreme_points = { extreme_points_storage, std::size(extreme_points_storage) };
//	WeldVertices(extreme_points);
//
//	if (extreme_points.size == 2 || extreme_points.size == 3) {
//		// TODO:
//		// Return empty convex hull at the moment. This is a degenerate case
//		// Where it is only a line or a triangle. If need be, handle these in the future
//		convex_hull.Initialize(nullptr, 0);
//		return convex_hull;
//	}
//
//	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
//	convex_hull.Initialize(&stack_allocator, extreme_points);
//	Stream<float3> potential_vertices = vertex_positions.Copy(allocator);
//
//	// Eliminate all points which lie inside the convex hull estimation
//	for (size_t index = 0; index < potential_vertices.size; index++) {
//		if (IsPointInsideConvexHull(&convex_hull, potential_vertices[index])) {
//			potential_vertices.RemoveSwapBack(index);
//			index--;
//		}
//	}
//
//	return convex_hull;
//}
//
//ConvexHull Quickhull(Stream<float3> vertex_positions, AllocatorPolymorphic allocator, AABBScalar aabb) {
//
//}