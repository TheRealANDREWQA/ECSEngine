// ECS_REFLECT
#pragma once
#include "ECSEngineMath.h"
#include "ECSEngineReflectionMacros.h"
#include "Export.h"

using namespace ECSEngine;

// Use shorts as index size to reduce memory footprint
// And bandwidth requirements since we won't have hulls
// With more vertices. Even thousands of vertices are quite
// A lot.
struct COLLISIONDETECTION_API ECS_REFLECT ConvexHullEdge {
	// It will assert that one of the faces is not set
	void AddFaceAssert(unsigned int face_index);

	unsigned short point_1;
	unsigned short point_2;
	unsigned short face_1_index;
	unsigned short face_2_index;
};

struct ECS_REFLECT ConvexHullFace {
	PlaneScalar plane;
	unsigned short points[32];
	unsigned short point_count = 0;
};

struct COLLISIONDETECTION_API ECS_REFLECT ConvexHull {
	// It asserts that there is enough space
	// Returns the index of the vertex
	unsigned int AddVertex(float3 point);

	// It assumes there is enough space
	unsigned int AddOrFindVertex(float3 point);

	// The faces will initially be both -1 to indicate that they are unassigned
	// Returns the edge index. It asserts that there is enough space
	unsigned int AddEdge(unsigned int edge_point_1, unsigned int edge_point_2);

	// Returns the index of the face
	unsigned int AddOrFindEdge(unsigned int edge_point_1, unsigned int edge_point_2);

	// It will perform edge collapses if it determines that the triangle
	// Plane already exists in the hull. The hull center is used to point
	// The normal in the opposite direction of the center. It will add the necessary
	// Edges to the complete the triangle
	void AddTriangleToEdge(unsigned int edge_index, unsigned int point_index, float3 hull_center);

	// It will add the edges and the face for the triangle. It needs to have space for the edges
	// And the face
	void AddTriangle(unsigned int vertex_A, unsigned int vertex_B, unsigned int vertex_C, float3 hull_center);

	// Returns the index of the edge or -1 if it doesn't find it. The order of the vertices doesn't matter.
	unsigned int FindEdge(unsigned int edge_point_1, unsigned int edge_point_2);

	void Initialize(AllocatorPolymorphic allocator, unsigned int vertex_size, unsigned int edge_size, unsigned int face_size);

	// The other data should not be used afterwards if the deallocate existent is set to true
	void Copy(const ConvexHull* other, AllocatorPolymorphic allocator, bool deallocate_existent);

	void Deallocate(AllocatorPolymorphic allocator);

	ECS_INLINE float3 GetPoint(unsigned int index) const {
		return { vertices_x[index], vertices_y[index], vertices_z[index] };
	}

	ECS_INLINE Line3D GetEdgePoints(unsigned int edge_index) const {
		unsigned int first_index = edges[edge_index].point_1;
		unsigned int second_index = edges[edge_index].point_2;
		return { GetPoint(first_index), GetPoint(second_index) };
	}

	ECS_INLINE void SetPoint(float3 point, unsigned int index) {
		vertices_x[index] = point.x;
		vertices_y[index] = point.y;
		vertices_z[index] = point.z;
	}

	float3 FurthestFrom(float3 direction) const;

	// Transform the current points by the given transform matrix and returns
	// A new convex hull. The edges and faces will references the ones from here. 
	// So don't modify them!
	ConvexHull ECS_VECTORCALL TransformToTemporary(Matrix matrix, AllocatorPolymorphic allocator) const;

	// It will copy the existing data
	void Resize(AllocatorPolymorphic allocator, unsigned int new_vertex_capacity, unsigned int new_edge_capacity, unsigned int new_face_capacity);

	void ReserveVertices(AllocatorPolymorphic allocator, unsigned int count = 1);

	void ReserveEdges(AllocatorPolymorphic allocator, unsigned int count = 1);

	void ReserveFaces(AllocatorPolymorphic allocator, unsigned int count = 1);

	// The representation contains the vertices stored in a SoA manner
	// In order to use SIMD for support function calculation. The faces
	// Are stored in a per edge fashion and separately, to allow fast query 
	// for the SAT test
	// All buffers are separate, they are not coalesced

	float* vertices_x;
	float* vertices_y;
	float* vertices_z;
	unsigned int vertex_size;
	unsigned int vertex_capacity;
	CapacityStream<ConvexHullEdge> edges;
	CapacityStream<ConvexHullFace> faces;

	ECS_SOA_REFLECT(vertices, vertex_size, vertex_capacity, vertices_x, vertices_y, vertices_z);
};

COLLISIONDETECTION_API ConvexHull CreateConvexHullFromMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator);

// This version is faster since it doesn't have to recalculate the AABB for the vertices
COLLISIONDETECTION_API ConvexHull CreateConvexHullFromMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator, AABBScalar aabb);

COLLISIONDETECTION_API bool IsPointInsideConvexHull(const ConvexHull* convex_hull, float3 point);