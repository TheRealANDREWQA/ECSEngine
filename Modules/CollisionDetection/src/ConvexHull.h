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
	ECS_INLINE void AddPoint(unsigned short index, AllocatorPolymorphic allocator) {
		// Grow with a small margin to not trigger many regrows
		points.AddResize(index, allocator, 4);
	}

	ECS_INLINE size_t CopySize() const {
		return points.CopySize();
	}

	ECS_INLINE ConvexHullFace CopyTo(uintptr_t& ptr) const {
		ConvexHullFace copy;
		copy.plane = plane;
		copy.points.InitializeAndCopy(ptr, points);
		return copy;
	}

	ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
		points.Deallocate(allocator);
	}

	ECS_INLINE unsigned int EdgeCount() const {
		return points.size;
	}

	PlaneScalar plane;
	// The points that make up this plane are listed here
	// In the counter clockwise order
	CapacityStream<unsigned short> points;
};

// The face buffers are meant to be temporary. After you finish creating
// The hull, call ReallocateFaces such that the buffers are coalesced into a single
// Allocation from a main allocator
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
	// Edges to the complete the triangle. The allocator is used for the face
	// Stream allocation, if needed
	void AddTriangleToEdge(unsigned int edge_index, unsigned int point_index, float3 hull_center, AllocatorPolymorphic face_allocator);

	// It will add the edges and the face for the triangle. It needs to have space for the edges
	// And the face
	void AddTriangle(unsigned int vertex_A, unsigned int vertex_B, unsigned int vertex_C, float3 hull_center, AllocatorPolymorphic face_allocator);
	
	// Calculates the center of the hull using the point positions
	// And stores it in the member field
	void CalculateAndAssignCenter();

	// Clips the face referenced by face_index with the incident face from the other hull
	// And outputs the 3D points that compose the clipped face
	void ClipFace(unsigned int face_index, const ConvexHull* incident_hull, unsigned int incident_hull_face_index, CapacityStream<float3>* points) const;

	// The other data should not be used afterwards if the deallocate existent is set to true
	void Copy(const ConvexHull* other, AllocatorPolymorphic allocator, bool deallocate_existent);

	void Deallocate(AllocatorPolymorphic allocator);

	// Returns the index of the edge or -1 if it doesn't find it. The order of the vertices doesn't matter.
	unsigned int FindEdge(unsigned int edge_point_1, unsigned int edge_point_2);

	float3 FurthestFrom(float3 direction) const;

	void Initialize(AllocatorPolymorphic allocator, unsigned int vertex_size, unsigned int edge_size, unsigned int face_size);

	ECS_INLINE float3 GetPoint(unsigned int index) const {
		return { vertices_x[index], vertices_y[index], vertices_z[index] };
	}

	ECS_INLINE Line3D GetEdgePoints(unsigned int edge_index) const {
		unsigned int first_index = edges[edge_index].point_1;
		unsigned int second_index = edges[edge_index].point_2;
		return { GetPoint(first_index), GetPoint(second_index) };
	}

	float3 GetFaceCenter(unsigned int face_index) const;

	// The plane is facing towards the center of the mesh
	PlaneScalar GetFaceSidePlane(unsigned int face_index, unsigned int face_edge_index) const;

	Line3D GetFaceEdge(unsigned int face_index, unsigned int face_edge_index) const;

	// It will merge coplanar or near coplanar faces
	// If the previous face allocator is specified, it will deallocate
	// The buffers of the faces that are collapsed
	// The face buffers will be allocated individually from the allocator
	// At the end, it will not resize the buffers. You must do that manually
	void MergeCoplanarFaces(AllocatorPolymorphic allocator, AllocatorPolymorphic previous_face_allocator = { nullptr });

	ECS_INLINE void SetPoint(float3 point, unsigned int index) {
		vertices_x[index] = point.x;
		vertices_y[index] = point.y;
		vertices_z[index] = point.z;
	}

	// Finds the face that is the closest to the given direction
	unsigned int SupportFace(float3 direction) const;

	// Reallocates the face buffers into a single coalesced allocation
	// Without deallocating the previous buffers (this can work for temporary
	// Allocators)
	void ReallocateFaces(AllocatorPolymorphic allocator);

	// Reallocates the face buffers into a single coalesced allocation
	// With deallocating the previous buffers
	void ReallocateFaces(AllocatorPolymorphic allocator, AllocatorPolymorphic face_allocator);

	// This is function will redirect the orientation of the edges such that they correspond
	// To the cross product of the face normals. This is mainly useful for SAT. We need this
	// Step such that we can avoid some computations. The edge direction is parallel to that
	// Cross product, but we need the edge to be in the same direction (aka positive dot product)
	// Since it is necessary for some test, otherwise it would fail
	void RedirectEdges();

	// It will remove any edges that are considered degenerate
	// At the moment, there can be cases where, because of edge
	// Collapsing for close face planes, we can get to a case where
	// We have a vertex with a single incoming edge in the middle
	// Of a face. You can optionally pass an allocator to resize the
	// Buffers to the new smaller size
	void RemoveDegenerateEdges(AllocatorPolymorphic allocator = { nullptr });

	// It will copy the existing data
	void Resize(AllocatorPolymorphic allocator, unsigned int new_vertex_capacity, unsigned int new_edge_capacity, unsigned int new_face_capacity);

	void ReserveVertices(AllocatorPolymorphic allocator, unsigned int count = 1);

	void ReserveEdges(AllocatorPolymorphic allocator, unsigned int count = 1);

	void ReserveFaces(AllocatorPolymorphic allocator, unsigned int count = 1);

	// Transform the current points by the given transform matrix and returns
	// A new convex hull. The edges and faces will references the ones from here. 
	// So don't modify them!
	ConvexHull ECS_VECTORCALL TransformToTemporary(Matrix matrix, AllocatorPolymorphic allocator) const;

	// The representation contains the vertices stored in a SoA manner
	// In order to use SIMD for support function calculation. The faces
	// Are stored in a per edge fashion and separately, to allow fast query 
	// for the SAT test.
	// All buffers are separate, they are not coalesced
	// TODO: Separate the face normals into a separate SoA buffer?
	// Both the contact manifold generation would benefit from this

	float* vertices_x;
	float* vertices_y;
	float* vertices_z;
	unsigned int vertex_size;
	unsigned int vertex_capacity;
	CapacityStream<ConvexHullEdge> edges;
	CapacityStream<ConvexHullFace> faces;
	float3 center;

	ECS_SOA_REFLECT(vertices, vertex_size, vertex_capacity, vertices_x, vertices_y, vertices_z);
};

struct ConvexHullRemappedVertex {
	unsigned short index;
	float3 previous_position;
};

//// This is a helper structure that can help construct a convex hull
//// It contains some additional information to help construct it
//struct COLLISIONDETECTION_API ConvexHullBuilder {
//	unsigned int AddOrFindVertex(float3 point);
//
//	// It deallocates any buffers that are builder related only
//	void DeallocateBuilderOnly();
//
//	// It initializes the underlying hull as well
//	void Initialize(AllocatorPolymorphic allocator, unsigned int vertex_size, unsigned int edge_size, unsigned int face_size);
//
//	ConvexHull hull;
//	// When we encounter an edge that is collapsed, having 2 triangles
//	// Be merged, we need to shift down a vertex such that it matches
//	// The plane face
//	ResizableStream<ConvexHullRemappedVertex> remapped_vertices;
//};

COLLISIONDETECTION_API ConvexHull CreateConvexHullFromMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator);

// This version is faster since it doesn't have to recalculate the AABB for the vertices
COLLISIONDETECTION_API ConvexHull CreateConvexHullFromMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator, AABBScalar aabb);

COLLISIONDETECTION_API bool IsPointInsideConvexHull(const ConvexHull* convex_hull, float3 point);