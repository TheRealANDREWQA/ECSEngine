#pragma once
#include "ECSEngineMath.h"
#include "Export.h"

using namespace ECSEngine;

struct ConvexHull {
	void Initialize(float* storage, size_t _size);

	void Initialize(AllocatorPolymorphic allocator, size_t _size);

	void Copy(const ConvexHull* other, AllocatorPolymorphic allocator, bool deallocate_existent);

	void Deallocate(AllocatorPolymorphic allocator);

	ECS_INLINE void SetPoint(float3 point, size_t index) {
		vertices_x[index] = point.x;
		vertices_y[index] = point.y;
		vertices_z[index] = point.z;
	}

	float3 FurthestFrom(float3 direction) const;

	// Transform the current points by the given transform matrix and returns
	// A new convex hull. The storage can be determined with MemoryOf
	ConvexHull ECS_VECTORCALL Transform(Matrix matrix, float* storage) const;

	// Returns the memory needed for a new copy of this
	ECS_INLINE size_t MemoryOf() const {
		return sizeof(float) * 3 * size;
	}

	// Use a SoA layout to use SIMD
	float* vertices_x;
	float* vertices_y;
	float* vertices_z;
	size_t size;
};

COLLISIONDETECTION_API ConvexHull CreateConvexHullFromMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator);