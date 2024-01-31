// ECS_REFLECT
#pragma once
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"
#include <float.h>
#include "../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	struct ECSENGINE_API ECS_REFLECT TriangleMesh {
		ECS_INLINE unsigned int AddPoint(float3 point) {
			return positions.AddAssert(point);
		}

		ECS_INLINE unsigned int AddTriangle(uint3 triangle_indices) {
			return triangles.AddAssert(triangle_indices);
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

		// These are coalesced
		CapacityStream<float3> positions;
		CapacityStream<uint3> triangles;
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