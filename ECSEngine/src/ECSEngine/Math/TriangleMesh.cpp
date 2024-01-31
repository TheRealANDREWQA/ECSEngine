#include "ecspch.h"
#include "TriangleMesh.h"
#include "../Utilities/PointerUtilities.h"
#include "MathHelpers.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::AddTriangleWithPoints(float3 point_a, float3 point_b, float3 point_c)
	{
		unsigned int point_index = positions.size;
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
		triangles.RemoveSwapBack(face_index);
		unsigned int last_position_index = positions.size - 1;
		AddTriangle({ triangle.x, triangle.y, last_position_index });
		AddTriangle({ triangle.x, last_position_index, triangle.z });
		AddTriangle({ last_position_index, triangle.y, triangle.z });
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Copy(const TriangleMesh* other, AllocatorPolymorphic allocator, bool deallocate_existent) {
		Initialize(allocator, other->positions.capacity, other->triangles.capacity);
		positions.CopyOther(other->positions);
		triangles.CopyOther(other->triangles);
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int TriangleMesh::FindPoint(float3 point) const
	{
		for (unsigned int index = 0; index < positions.size; index++) {
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
		size_t allocation_size = positions.MemoryOf(_position_capacity) + triangles.MemoryOf(_triangle_capacity);
		void* allocation = AllocateEx(allocator, allocation_size);
		uintptr_t ptr = (uintptr_t)allocation;
		positions.InitializeFromBuffer(ptr, 0, _position_capacity);
		triangles.InitializeFromBuffer(ptr, 0, _triangle_capacity);
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Deallocate(AllocatorPolymorphic allocator) {
		if (positions.capacity > 0 || triangles.capacity > 0 && positions.buffer != nullptr) {
			DeallocateEx(allocator, positions.buffer);
		}
		memset(this, 0, sizeof(*this));
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::RetargetNormals(float3 center) {
		for (unsigned int index = 0; index < triangles.size; index++) {
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
		if (positions.size + count > positions.capacity) {
			unsigned int resize_count = std::max((unsigned int)(positions.capacity * 1.5f), positions.size + count);
			unsigned int new_triangle_capacity = triangles.capacity;
			if (resize_triangles) {
				new_triangle_capacity = triangles.capacity * 1.5f;
			}
			Resize(allocator, resize_count, new_triangle_capacity);
		}
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::ReserveTriangles(AllocatorPolymorphic allocator, unsigned int count, bool resize_positions) {
		if (triangles.size + count > triangles.capacity) {
			unsigned int resize_count = std::max((unsigned int)(triangles.capacity * 1.5f), triangles.size + count);
			unsigned int new_position_capacity = positions.capacity;
			if (resize_positions) {
				new_position_capacity = positions.capacity * 1.5f;
			}
			Resize(allocator, new_position_capacity, resize_count);
		}
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Resize(AllocatorPolymorphic allocator, unsigned int new_position_capacity, unsigned int new_triangle_capacity) {
		size_t new_size = positions.MemoryOf(new_position_capacity) + triangles.MemoryOf(new_triangle_capacity);

		if (positions.capacity > 0 || triangles.capacity > 0) {
			float3* new_positions = (float3*)ReallocateEx(allocator, positions.buffer, new_size);
			uint3* new_triangles = (uint3*)OffsetPointer(new_positions, positions.MemoryOf(new_position_capacity));
			
			if (positions.buffer != new_positions) {
				// Do this only if the buffer is changed
				unsigned int copy_size = std::min(positions.size, new_position_capacity);
				memcpy(new_positions, positions.buffer, positions.MemoryOf(copy_size));
			}

			// We must use memmove here since if the block is resized, the triangles
			// Can alias themselves
			unsigned int copy_size = std::min(triangles.size, new_triangle_capacity);
			memmove(new_triangles, triangles.buffer, triangles.MemoryOf(copy_size));
			positions.buffer = new_positions;
			triangles.buffer = new_triangles;
		}
		else {
			positions.buffer = (float3*)AllocateEx(allocator, new_size);
			triangles.buffer = (uint3*)OffsetPointer(positions.buffer, positions.MemoryOf(new_position_capacity));
		}

		positions.capacity = new_position_capacity;
		triangles.capacity = new_triangle_capacity;
		positions.size = std::min(positions.size, new_position_capacity);
		triangles.size = std::min(triangles.size, new_triangle_capacity);
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