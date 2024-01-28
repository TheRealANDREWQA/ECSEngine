#pragma once
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"
#include <float.h>

namespace ECSEngine {

	struct ECSENGINE_API TriangleMesh {
		ECS_INLINE unsigned int AddPoint(float3 point) {
			ECS_ASSERT(position_count < position_capacity);
			positions[position_count++] = point;
			return position_count - 1;
		}

		ECS_INLINE unsigned int AddTriangle(uint3 triangle_indices) {
			ECS_ASSERT(triangle_count < triangle_capacity);
			triangles[triangle_count++] = triangle_indices;
			return triangle_count - 1;
		}

		void AddTriangleWithPoints(float3 point_a, float3 point_b, float3 point_c);

		// Adds a new point, which will collapse the given face and create new ones to complement the mesh
		void AddPointCollapseFace(float3 point, unsigned int face_index);

		void Copy(const TriangleMesh* other, AllocatorPolymorphic allocator, bool deallocate_existent);

		unsigned int FindPoint(float3 point) const;

		unsigned int FindOrAddPoint(float3 point);

		void Initialize(AllocatorPolymorphic allocator, unsigned int position_capacity, unsigned int triangle_capacity);

		void Deallocate(AllocatorPolymorphic allocator);

		// Changes the winding of the triangles such that they face away from the center (like a normal mesh)
		void RetargetNormals(float3 center);

		// If the resize triangles is set to true, then if a resize happens, it will increase the triangle
		// Capacity as well
		void ReservePositions(AllocatorPolymorphic allocator, unsigned int count = 1, bool resize_triangles = false);

		// If the resize positions is set to true, then if a resize happens, it will increase the position
		// Capacity as well
		void ReserveTriangles(AllocatorPolymorphic allocator, unsigned int count = 1, bool resize_positions = false);

		void Resize(AllocatorPolymorphic allocator, unsigned int new_position_capacity, unsigned int new_triangle_capacity);

		ECS_INLINE Stream<float3> Positions() const {
			return { positions, position_count };
		}

		ECS_INLINE Stream<uint3> Triangles() const {
			return { triangles, triangle_count };
		}

		// These are coalesced
		float3* positions;
		uint3* triangles;
		unsigned int position_count;
		unsigned int position_capacity;
		unsigned int triangle_count;
		unsigned int triangle_capacity;
	};

	//ECSENGINE_API 

	//// It will calculate a triangle mesh from the given extreme points (there should be at maximum 6). 
	//// It can happen that the mesh is degenerate, if there are only 2 vertices. In that case, it returns an empty mesh. 
	//// You can choose a different initial capacity if you want to later on add entries to this mesh. 
	//ECSENGINE_API TriangleMesh TriangleMeshFromAABBExtremePoints(
	//	Stream<float3> values,
	//	AllocatorPolymorphic allocator,
	//	unsigned int initial_position_capacity = 0,
	//	unsigned int initial_triangle_capacity = 0
	//);

}