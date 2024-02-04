// ECS_REFLECT
#pragma once
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"
#include <float.h>
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "Matrix.h"

namespace ECSEngine {

	struct ECSENGINE_API ECS_REFLECT TriangleMesh {
		ECS_INLINE unsigned int AddPoint(float3 point) {
			ECS_ASSERT(position_size < position_capacity);
			position_x[position_size] = point.x;
			position_y[position_size] = point.y;
			position_z[position_size] = point.z;
			position_size++;
			return position_size - 1;
		}

		ECS_INLINE unsigned int AddTriangle(uint3 triangle_indices) {
			return triangles.AddAssert(triangle_indices);
		}

		void AddTriangleWithPoints(float3 point_a, float3 point_b, float3 point_c);

		// Adds a new point, which will collapse the given face and create new ones to complement the mesh
		void AddPointCollapseFace(float3 point, unsigned int face_index);

		void Copy(const TriangleMesh* other, AllocatorPolymorphic allocator, bool deallocate_existent);

		void Deallocate(AllocatorPolymorphic allocator);

		unsigned int FindPoint(float3 point) const;

		unsigned int FindOrAddPoint(float3 point);

		float3 FurthestFrom(float3 direction) const;

		void Initialize(AllocatorPolymorphic allocator, unsigned int position_capacity, unsigned int triangle_capacity);

		ECS_INLINE float3 GetPoint(unsigned int index) const {
			return { position_x[index], position_y[index], position_z[index] };
		}

		// Changes the winding of the triangles such that they face away from the center (like a normal mesh)
		void RetargetNormals(float3 center);

		// If the resize triangles is set to true, then if a resize happens, it will increase the triangle
		// Capacity as well
		void ReservePositions(AllocatorPolymorphic allocator, unsigned int count = 1);

		// If the resize positions is set to true, then if a resize happens, it will increase the position
		// Capacity as well
		void ReserveTriangles(AllocatorPolymorphic allocator, unsigned int count = 1);

		void Resize(AllocatorPolymorphic allocator, unsigned int new_position_capacity, unsigned int new_triangle_capacity);

		// Changes the positions SoA buffer
		void SetPositionsBuffer(float* buffer);

		// Only the positions are changed. The triangle indices will be referenced
		TriangleMesh ECS_VECTORCALL Transform(Matrix matrix, float* position_storage) const;

		// Only the positions are allocated. The triangle indices will be referenced
		TriangleMesh ECS_VECTORCALL Transform(Matrix matrix, AllocatorPolymorphic allocator) const;

		float* position_x;
		float* position_y;
		float* position_z;
		unsigned int position_size;
		unsigned int position_capacity;
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