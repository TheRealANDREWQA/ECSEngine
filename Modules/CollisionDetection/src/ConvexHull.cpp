#include "pch.h"
#include "ConvexHull.h"
#include "GJK.h"
#include "Quickhull.h"

void ConvexHull::Initialize(float* storage, size_t _size)
{
	size = _size;
	uintptr_t ptr = (uintptr_t)storage;
	SoAInitializeFromBuffer(size, ptr, &vertices_x, &vertices_y, &vertices_z);
}

void ConvexHull::Initialize(AllocatorPolymorphic allocator, size_t _size)
{
	size = _size;
	float* allocation = (float*)Allocate(allocator, MemoryOf());
	Initialize(allocation, size);
}

void ConvexHull::Initialize(AllocatorPolymorphic allocator, Stream<float3> entries)
{
	Initialize(allocator, entries.size);
	for (size_t index = 0; index < entries.size; index++) {
		SetPoint(entries[index], index);
	}
}

void ConvexHull::Copy(const ConvexHull* other, AllocatorPolymorphic allocator, bool deallocate_existent)
{
	void* current_soa_data = vertices_x;
	vertices_x = other->vertices_x;
	vertices_y = other->vertices_y;
	vertices_z = other->vertices_z;

	if (deallocate_existent) {
		SoACopyReallocate(allocator, other->size, other->size, current_soa_data, &vertices_x, &vertices_y, &vertices_z);
	}
	else {
		SoACopy(allocator, other->size, other->size, &vertices_x, &vertices_y, &vertices_z);
	}
	size = other->size;
}

void ConvexHull::Deallocate(AllocatorPolymorphic allocator)
{
	if (size > 0 && vertices_x != nullptr) {
		ECSEngine::Deallocate(allocator, vertices_x);
	}
	memset(this, 0, sizeof(*this));
}

float3 ConvexHull::FurthestFrom(float3 direction) const
{
	Vec8f max_distance = -FLT_MAX;
	Vector3 max_points;
	Vector3 vector_direction = Vector3().Splat(direction);
	size_t simd_count = GetSimdCount(size, Vector3::ElementCount());
	for (size_t index = 0; index < simd_count; index += Vector3::ElementCount()) {
		Vector3 values = Vector3().LoadAdjacent(vertices_x, index, size);
		Vec8f distance = Dot(values, vector_direction);
		SIMDVectorMask is_greater = distance > max_distance;
		max_distance = SelectSingle(is_greater, distance, max_distance);
		max_points = Select(is_greater, values, max_points);
	}

	// Consider the remaining
	if (simd_count < size) {
		Vector3 values = Vector3().LoadAdjacent(vertices_x, simd_count, size);
		Vec8f distance = Dot(values, vector_direction);
		// Mask the distance such that we don't consider elements which are out of bounds
		distance.cutoff(size - simd_count);
		SIMDVectorMask is_greater = distance > max_distance;
		max_distance = SelectSingle(is_greater, distance, max_distance);
		max_points = Select(is_greater, values, max_points);
	}

	Vec8f max = HorizontalMax8(max_distance);
	size_t index = HorizontalMax8Index(max_distance, max);
	return max_points.At(index);
}

ConvexHull ECS_VECTORCALL ConvexHull::Transform(Matrix matrix, float* storage) const
{
	ConvexHull hull;

	hull.Initialize(storage, size);
	ApplySIMDConstexpr(size, Vector3::ElementCount(), [matrix, this, &hull](auto is_normal_iteration, size_t index, size_t count) {
		Vector3 elements;
		if constexpr (is_normal_iteration) {
			elements = Vector3().LoadAdjacent(vertices_x, index, size);
		}
		else {
			elements = Vector3().LoadPartialAdjacent(vertices_x, index, size, count);
		}

		Vector3 transformed_elements = TransformPoint(elements, matrix).AsVector3();
		if constexpr (is_normal_iteration) {
			transformed_elements.StoreAdjacent(hull.vertices_x, index, size);
		}
		else {
			transformed_elements.StorePartialAdjacent(hull.vertices_x, index, size, count);
		}
		});

	return hull;
}

ConvexHull CreateConvexHullFromMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator)
{
	/*ConvexHull hull;

	float3 corners[8];
	AABBScalar scalar_aabb = GetAABBFromPoints(vertex_positions);
	GetAABBCorners(scalar_aabb, corners);
	hull.Initialize(allocator, Stream<float3>(corners, std::size(corners)));

	return hull;*/
	return {};
}

ConvexHull CreateConvexHullFromMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator, AABBScalar aabb) {
	//return Quickhull(vertex_positions, allocator, aabb);
	return {};
}

bool IsPointInsideConvexHull(const ConvexHull* convex_hull, float3 point) {
	return GJKPoint(convex_hull, point) < 0.0f;
}
