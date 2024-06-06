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

	// Computes the edges for each vertex. This is a parallel array to the vertices
	// Inside this instance. Changing the order of the vertices will have to be reflected here as well
	// Each entry is separately allocated
	Stream<CapacityStream<unsigned int>> ComputeVertexToEdgeTable(AllocatorPolymorphic allocator) const;

	// Computes the connected vertices for each vertex. This is a parallel array to the vertices
	// Inside this instance. Changing the order of the vertices will have to be reflected here as well
	// Each entry is separately allocated
	Stream<CapacityStream<unsigned int>> ComputeVertexToVertexTableFromEdges(AllocatorPolymorphic allocator) const;

	// Computes the connected vertices for each vertex. This is a parallel array to the vertices
	// Inside this instance. Changing the order of the vertices will have to be reflected here as well
	// Each entry is separately allocated. For this function to work, it assumes that the vertices
	// Are in CCW order
	Stream<CapacityStream<unsigned int>> ComputeVertexToVertexTableFromFaces(AllocatorPolymorphic allocator) const;

	// The other data should not be used afterwards if the deallocate existent is set to true
	void Copy(const ConvexHull* other, AllocatorPolymorphic allocator, bool deallocate_existent);

	void Deallocate(AllocatorPolymorphic allocator);

	// Returns the index of the edge or -1 if it doesn't find it. The order of the vertices doesn't matter.
	unsigned int FindEdge(unsigned int edge_point_1, unsigned int edge_point_2) const;

	// Returns -1 if it doesn't find it. It will match a face regardless of the
	// Order of the face_points
	unsigned int FindFace(Stream<unsigned int> face_points) const;

	float3 FurthestFrom(float3 direction) const;

	void Initialize(AllocatorPolymorphic allocator, unsigned int vertex_size, unsigned int edge_size, unsigned int face_size);

	// Returns true if all of its faces are triangles
	bool IsTriangular() const;

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

	uint2 GetFaceEdgeIndices(unsigned int face_index, unsigned int face_edge_index) const;

	// It merges coplanar triangles into quads. It does not merge quads further
	void MergeCoplanarTriangles(float coplanarity_degrees, AllocatorPolymorphic allocator, AllocatorPolymorphic previous_face_allocator = { nullptr });

	ECS_INLINE void SetPoint(float3 point, unsigned int index) {
		vertices_x[index] = point.x;
		vertices_y[index] = point.y;
		vertices_z[index] = point.z;
	}

	// In case this convex hull is made of only triangles and quads, you can call this
	// Function to simplify it. It will try to reduce the complexity while
	// Minimizing the loss. It does not reallocate the buffers afterwards
	// You can provide the allocator if you want the face buffers to be deallocated
	// The reduction factor can be controlled to increase or decrease the area of the
	// Triangles being considered. Increasing the area_factor will result in more
	// Triangles being eliminated, while reducing it will result in fewer triangles
	// Being collapsed. The center of the hull needs to be set beforehand!
	void SimplifyTrianglesAndQuads(float area_factor = 1.0f, AllocatorPolymorphic allocator = { nullptr });

	// Returns the support point for that face in the given direction
	// It returns the index inside the face, not inside the vertex array!
	unsigned int SupportFacePoint(unsigned int face_index, float3 direction) const;

	// Finds the face that is the closest to the given direction
	unsigned int SupportFace(float3 direction) const;

	// It will maintain the reference integrity
	void RemoveSwapBackVertex(unsigned int index);

	void RemoveSwapBackEdge(unsigned int edge_index);

	// It will maintain the reference integrity. It does not deallocate
	// The face buffers
	void RemoveSwapBackFace(unsigned int face_index);

	// It will maintain the reference integrity. It will deallocate the
	// Face buffers
	void RemoveSwapBackFace(unsigned int face_index, AllocatorPolymorphic allocator);

	// Reorders the points of the faces such that they respect
	// The CCW order, otherwise the edges will be malformed
	// The vertex to edge table should have the connections as
	// Edge indices for each entry
	void ReorderFacePointsByEdges(unsigned int face_index, Stream<CapacityStream<unsigned int>> vertex_to_edge_table);

	// Reorders the points of the faces such that they respect
	// The CCW order, otherwise the edges will be malformed
	// The vertex to vertex table should have the connetions as
	// Vertex indices for each entry
	void ReorderFacePointsByVertices(unsigned int face_index, Stream<CapacityStream<unsigned int>> vertex_to_vertex_table);

	// It will reoder the points based on the angle from the face center
	void ReorderFacePointsByAngles(unsigned int face_index);

	// Reorders the points of the faces such that they respect
	// The CCW order, otherwise the edges will be malformed
	void ReorderFacePoints();

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

	// In case some algorithm was applied to the faces that did not
	// Correctly collapse the edges, this will recompletely construct
	// The edges such that they match the face. If the allocator is specified,
	// It will reallocate the buffer if the edge capacity is exceeded. Else, it
	// Will assert that there are enough slots
	void RegenerateEdges(AllocatorPolymorphic allocator = { nullptr });

	// It will copy the existing data
	void Resize(AllocatorPolymorphic allocator, unsigned int new_vertex_capacity, unsigned int new_edge_capacity, unsigned int new_face_capacity);

	void ReserveVertices(AllocatorPolymorphic allocator, unsigned int count = 1);

	void ReserveEdges(AllocatorPolymorphic allocator, unsigned int count = 1);

	void ReserveFaces(AllocatorPolymorphic allocator, unsigned int count = 1);

	// Fills the buffer with the triangles that would make up this hull
	// It does not create full connectivity, it is helpful in case only
	// The triangles are needed, without associated connectivity information
	void RetrieveTriangulatedFaces(AdditionStream<ushort3> triangles) const;

	// Transform the current points by the given transform matrix and returns
	// A new convex hull. The edges and faces will references the ones from here. 
	// So don't modify them!
	ConvexHull ECS_VECTORCALL TransformToTemporary(Matrix matrix, AllocatorPolymorphic allocator) const;

	// It will scale the convex hull with the given factor
	void Scale(float3 factor);

	// Returns true if the hull is valid, else false
	bool Validate() const;

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