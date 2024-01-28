#include "ecspch.h"
#include "TriangleMesh.h"
#include "../Utilities/PointerUtilities.h"
#include "MathHelpers.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::AddTriangleWithPoints(float3 point_a, float3 point_b, float3 point_c)
	{
		unsigned int point_index = position_count;
		AddPoint(point_a);
		AddPoint(point_b);
		AddPoint(point_c);
		AddTriangle({ point_index, point_index + 1, point_index + 2 });
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::AddPointCollapseFace(float3 point, unsigned int face_index)
	{
		AddPoint(point);
		uint3 triangle = triangles[face_index];
		triangle_count--;
		triangles[face_index] = triangles[triangle_count];
		AddTriangle({ triangle.x, triangle.y, position_count - 1 });
		AddTriangle({ triangle.x, position_count - 1, triangle.z });
		AddTriangle({ position_count - 1, triangle.y, triangle.z });
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Copy(const TriangleMesh* other, AllocatorPolymorphic allocator, bool deallocate_existent) {
		Initialize(allocator, other->position_capacity, other->triangle_capacity);
		other->Positions().CopyTo(positions);
		other->Triangles().CopyTo(triangles);

		position_count = other->position_count;
		triangle_count = other->triangle_count;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int TriangleMesh::FindPoint(float3 point) const
	{
		for (unsigned int index = 0; index < position_count; index++) {
			if (point == positions[index]) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int TriangleMesh::FindOrAddPoint(float3 point)
	{
		unsigned int point_index = FindPoint(point);
		if (point_index == -1) {
			return AddPoint(point);
		}
		return point_index;
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Initialize(AllocatorPolymorphic allocator, unsigned int _position_capacity, unsigned int _triangle_capacity) {
		position_capacity = _position_capacity;
		triangle_capacity = _triangle_capacity;

		size_t allocation_size = sizeof(*positions) * position_capacity + sizeof(*triangles) * triangle_capacity;
		void* allocation = AllocateEx(allocator, allocation_size);
		positions = (float3*)allocation;
		triangles = (uint3*)OffsetPointer(positions, sizeof(*positions) * position_capacity);

		position_count = 0;
		triangle_count = 0;
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Deallocate(AllocatorPolymorphic allocator) {
		if (position_capacity > 0 || triangle_capacity > 0) {
			DeallocateEx(allocator, positions);
		}
		memset(this, 0, sizeof(*this));
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::RetargetNormals(float3 center) {
		for (unsigned int index = 0; index < triangle_count; index++) {
			uint3 indices = triangles[index];
			float3 a = positions[indices.x];
			float3 b = positions[indices.y];
			float3 c = positions[indices.z];
			if (IsTriangleFacing(a, b, c, center)) {
				// Reverse the 1 and 2 indices
				triangles[index] = { indices.x, indices.z, indices.y };
			}
		}
	}
	
	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::ReservePositions(AllocatorPolymorphic allocator, unsigned int count, bool resize_triangles) {
		if (position_count + count > position_capacity) {
			unsigned int resize_count = std::max((unsigned int)(position_capacity * 1.5f), position_count + count);
			unsigned int new_triangle_capacity = triangle_capacity;
			if (resize_triangles) {
				new_triangle_capacity = triangle_capacity * 1.5f;
			}
			Resize(allocator, resize_count, new_triangle_capacity);
		}
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::ReserveTriangles(AllocatorPolymorphic allocator, unsigned int count, bool resize_positions) {
		if (triangle_count + count > triangle_capacity) {
			unsigned int resize_count = std::max((unsigned int)(triangle_capacity * 1.5f), triangle_count + count);
			unsigned int new_position_capacity = position_capacity;
			if (resize_positions) {
				new_position_capacity = position_capacity * 1.5f;
			}
			Resize(allocator, new_position_capacity, resize_count);
		}
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Resize(AllocatorPolymorphic allocator, unsigned int new_position_capacity, unsigned int new_triangle_capacity) {
		size_t new_size = sizeof(*positions) * new_position_capacity + sizeof(*triangles) * new_triangle_capacity;

		if (position_capacity > 0 || triangle_capacity > 0) {
			float3* new_positions = (float3*)ReallocateEx(allocator, positions, new_size);
			uint3* new_triangles = (uint3*)OffsetPointer(new_positions, sizeof(*positions) * new_position_capacity);
			
			if (positions != new_positions) {
				// Do this only if the buffer is changed
				unsigned int copy_size = std::min(position_count, new_position_capacity);
				memcpy(new_positions, positions, sizeof(*positions) * copy_size);
			}

			// We must use memmove here since if the block is resized, the triangles
			// Can alias themselves
			unsigned int copy_size = std::min(triangle_count, new_triangle_capacity);
			memmove(new_triangles, triangles, sizeof(*triangles) * copy_size);
			positions = new_positions;
			triangles = new_triangles;
		}
		else {
			positions = (float3*)AllocateEx(allocator, new_size);
			triangles = (uint3*)OffsetPointer(positions, sizeof(*positions) * new_position_capacity);
		}

		position_capacity = new_position_capacity;
		triangle_capacity = new_triangle_capacity;
		position_count = std::min(position_count, new_position_capacity);
		triangle_count = std::min(triangle_count, new_triangle_capacity);
	}

	// --------------------------------------------------------------------------------------------------

	//TriangleMesh TriangleMeshFromAABBExtremePoints(
	//	Stream<float3> values,
	//	AllocatorPolymorphic allocator,
	//	unsigned int initial_position_capacity,
	//	unsigned int initial_triangle_capacity
	//) {
	//	ECS_ASSERT(values.size <= 6);

	//	TriangleMesh triangle_mesh;

	//	// Determine if there are any duplicates
	//	float3 temp_values[6];
	//	values.CopyTo(temp_values);
	//	size_t valid_count = values.size;
	//	for (size_t index = 0; index < valid_count; index++) {
	//		size_t subindex = index + 1;
	//		for (; subindex < valid_count; subindex++) {
	//			if (temp_values[index] == temp_values[subindex]) {
	//				break;
	//			}
	//		}

	//		if (subindex < valid_count) {
	//			// This is a duplicate, remove the entry
	//			valid_count--;
	//			temp_values[subindex] = temp_values[valid_count];
	//		}
	//	}

	//	if (valid_count == 2) {
	//		return { nullptr, nullptr, 0, 0 };
	//	}

	//	initial_position_capacity = initial_position_capacity != 0 ? std::max(initial_position_capacity, (unsigned int)valid_count) : 0;
	//	unsigned int conservative_index_count = (unsigned int)valid_count * 2 * 3;
	//	// For 3 points, there need to be 3 indices. For 4 points, there are 4 faces, so 12 indices
	//	// For 5 points, there are 6 faces, so 18 indices. For 6 points, there are 8 faces, so 24 indices
	//	// So, for all cases, we can cover them all with a conservative
	//	initial_triangle_capacity = initial_triangle_capacity != 0 ? std::max(initial_triangle_capacity, conservative_index_count) : 0;
	//	triangle_mesh.Initialize(
	//		allocator,
	//		initial_position_capacity == 0 ? valid_count : initial_position_capacity,
	//		initial_triangle_capacity == 0 ? conservative_index_count : initial_triangle_capacity
	//	);

	//	memcpy(triangle_mesh.positions, temp_values, sizeof(float3) * valid_count);
	//	triangle_mesh.position_count = valid_count;
	//	if (valid_count == 3) {
	//		// A single triangle
	//		triangle_mesh.AddTriangle({ 0, 1, 2 });
	//	}
	//	else {
	//		// For the rest, we can initialize a tetrahedron,
	//		// And add the points to the corresponding faces
	//		// Such that we maintain the proper vertex winding
	//		triangle_mesh.AddTriangle({ 0, 1, 2 });
	//		triangle_mesh.AddTriangle({ 0, 2, 3 });
	//		triangle_mesh.AddTriangle({ 1, 2, 3 });
	//		triangle_mesh.AddTriangle({ 0, 1, 3 });

	//		if (valid_count == 5) {

	//		}
	//		else if (valid_count == 6) {

	//		}
	//	}

	//	return triangle_mesh;
	//}

	// --------------------------------------------------------------------------------------------------

}