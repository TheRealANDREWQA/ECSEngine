#include "ecspch.h"
#include "TriangleMesh.h"
#include "../Utilities/PointerUtilities.h"
#include "MathHelpers.h"
#include "Vector.h"
#include "../Containers/SoA.h"
#include "../Utilities/Utilities.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::AddTriangleWithPoints(float3 point_a, float3 point_b, float3 point_c)
	{
		unsigned int point_index = position_size;
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
		unsigned int last_position_index = position_size - 1;
		AddTriangle({ triangle.x, triangle.y, last_position_index });
		AddTriangle({ triangle.x, last_position_index, triangle.z });
		AddTriangle({ last_position_index, triangle.y, triangle.z });
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Copy(const TriangleMesh* other, AllocatorPolymorphic allocator, bool deallocate_existent) {
		Initialize(allocator, other->position_capacity, other->triangles.capacity);
		SoACopyDataOnly(other->position_size, position_x, other->position_x, position_y, other->position_y, position_z, other->position_z);
		triangles.CopyOther(other->triangles);
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int TriangleMesh::FindPoint(float3 point) const
	{
		for (unsigned int index = 0; index < position_size; index++) {
			if (point.x == position_x[index] && point.y == position_y[index] && point.z == position_z[index]) {
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

	float3 TriangleMesh::FurthestFrom(float3 direction) const
	{
		Vec8f max_distance = -FLT_MAX;
		Vector3 max_points;
		Vector3 vector_direction = Vector3().Splat(direction);

		ApplySIMDConstexpr(position_size, Vector3::ElementCount(), [this, vector_direction, &max_distance, &max_points](auto is_full_iteration, size_t index, size_t count) {
			Vector3 values = Vector3().LoadAdjacent(position_x, index, position_capacity);
			Vec8f distance = Dot(values, vector_direction);
			if constexpr (!is_full_iteration) {
				Vec8fb select_mask = CastToFloat(SelectMaskLast<unsigned int>(Vec8f::size() - count));
				distance = select(select_mask, Vec8f(-FLT_MAX), distance);
			}
			SIMDVectorMask is_greater = distance > max_distance;
			max_distance = SelectSingle(is_greater, distance, max_distance);
			max_points = Select(is_greater, values, max_points);
		});

		Vec8f max = HorizontalMax8(max_distance);
		size_t index = HorizontalMax8Index(max_distance, max);
		return max_points.At(index);
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Initialize(AllocatorPolymorphic allocator, unsigned int _position_capacity, unsigned int _triangle_capacity) {
		if (_position_capacity > 0) {
			SoAInitialize(allocator, _position_capacity, &position_x, &position_y, &position_z);
		}
		else {
			position_x = nullptr;
			position_y = nullptr;
			position_z = nullptr;
		}
		position_capacity = _position_capacity;
		position_size = 0;
		triangles.Initialize(allocator, 0, _triangle_capacity);
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Deallocate(AllocatorPolymorphic allocator) {
		if (position_capacity > 0 && position_x != nullptr) {
			DeallocateEx(allocator, position_x);
		}
		triangles.Deallocate(allocator);
		memset(this, 0, sizeof(*this));
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::RetargetNormals(float3 center) {
		for (unsigned int index = 0; index < triangles.size; index++) {
			uint3 indices = triangles[index];
			float3 a = GetPoint(indices.x);
			float3 b = GetPoint(indices.y);
			float3 c = GetPoint(indices.z);
			if (IsTriangleFacing(a, b, c, center)) {
				// Reverse the 1 and 2 indices
				triangles[index] = { indices.x, indices.z, indices.y };
			}
		}
	}
	
	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::ReservePositions(AllocatorPolymorphic allocator, unsigned int count) {
		SoAReserve(allocator, position_size, position_capacity, count, &position_x, &position_y, &position_z);
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::ReserveTriangles(AllocatorPolymorphic allocator, unsigned int count) {
		triangles.Reserve(allocator, count);
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::Resize(AllocatorPolymorphic allocator, unsigned int new_position_capacity, unsigned int new_triangle_capacity) {
		SoAResize(allocator, position_size, new_position_capacity, &position_x, &position_y, &position_z);
		position_capacity = new_position_capacity;
		position_size = std::min(position_size, new_position_capacity);

		triangles.Resize(allocator, new_triangle_capacity);
	}

	// --------------------------------------------------------------------------------------------------

	void TriangleMesh::SetPositionsBuffer(float* buffer, unsigned int size, unsigned int capacity)
	{
		position_size = size;
		position_capacity = capacity;

		uintptr_t ptr = (uintptr_t)buffer;
		SoAInitializeFromBuffer(position_capacity, ptr, &position_x, &position_y, &position_z);
	}

	// --------------------------------------------------------------------------------------------------

	TriangleMesh ECS_VECTORCALL TriangleMesh::Transform(Matrix matrix, float* position_storage) const
	{
		TriangleMesh transformed_mesh;

		transformed_mesh.SetPositionsBuffer(position_storage, position_size, position_capacity);
		transformed_mesh.triangles = triangles;

		TransformPoints(matrix, position_x, position_size, position_storage);
		return transformed_mesh;
	}

	TriangleMesh ECS_VECTORCALL TriangleMesh::Transform(Matrix matrix, AllocatorPolymorphic allocator) const
	{
		float* position_storage = (float*)AllocateEx(allocator, sizeof(float3) * position_size);
		return Transform(matrix, position_storage);
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