#include "pch.h"
#include "ConvexHull.h"
#include "GJK.h"
#include "Quickhull.h"

unsigned int ConvexHull::AddVertex(float3 point) {
	ECS_ASSERT(vertex_size < vertex_capacity, "Insufficient ConvexHull vertex capacity");
	SetPoint(point, vertex_size);
	vertex_size++;
	return vertex_size - 1;
}

unsigned int ConvexHull::AddOrFindVertex(float3 point) {
	for (unsigned int index = 0; index < vertex_size; index++) {
		if (GetPoint(index) == point) {
			return index;
		}
	}
	// Need to add another point
	return AddVertex(point);
}

unsigned int ConvexHull::AddEdge(unsigned int edge_point_1, unsigned int edge_point_2) {
	return edges.AddAssert({ (unsigned short)edge_point_1, (unsigned short)edge_point_2, USHORT_MAX, USHORT_MAX });
}

unsigned int ConvexHull::AddOrFindEdge(unsigned int edge_point_1, unsigned int edge_point_2) {
	unsigned int existing_edge = FindEdge(edge_point_1, edge_point_2);
	if (existing_edge == -1) {
		existing_edge = AddEdge(edge_point_1, edge_point_2);
	}
	return existing_edge;
}

void ConvexHull::AddTriangleToEdge(unsigned int edge_index, unsigned int point_index, float3 hull_center) {
	// Add or find the "new" triangle edges
	ConvexHullEdge edge = edges[edge_index];

	unsigned int first_new_edge = AddOrFindEdge(edge.point_1, point_index);
	unsigned int second_new_edge = AddOrFindEdge(edge.point_2, point_index);
	ConvexHullEdge* first_new_edge_ptr = &edges[first_new_edge];
	ConvexHullEdge* second_new_edge_ptr = &edges[second_new_edge];
	
	// We defer the edge removal after we assign all values since the swap back
	// Can interfer with one of the newly added edges
	bool remove_edge = false;
	unsigned short face_index = USHORT_MAX;
	// Determine if the face on the other side of the edge has the same normal
	// If they do, we can collapse the edge
	float3 edge_point_1 = GetPoint(edge.point_1);
	float3 edge_point_2 = GetPoint(edge.point_2);
	float3 triangle_point = GetPoint(point_index);
	float3 normalized_normal = -TriangleNormalNormalized(edge_point_1, edge_point_2, triangle_point, hull_center);
	if (edge.face_1_index != USHORT_MAX) {
		PlaneScalar existing_face = faces[edge.face_1_index].plane;
		if (ComparePlaneDirectionsByAngle(existing_face, normalized_normal, 3.0f)) {
			// We can collapse this existing edge
			remove_edge = true;
			// The face to be assigned to the newly added edges should be this one
			face_index = edge.face_1_index;
			faces[face_index].points[faces[face_index].point_count++] = point_index;
			ECS_ASSERT(faces[face_index].point_count <= ECS_COUNTOF(faces[face_index].points));
		}
		else {
			// Create a new face
			face_index = faces.AddAssert({ PlaneScalar::FromNormalized(normalized_normal, edge_point_1) });
			faces[face_index].point_count = 3;
			faces[face_index].points[0] = edge.point_1;
			faces[face_index].points[1] = edge.point_2;
			faces[face_index].points[2] = point_index;
			ECS_ASSERT(edge.face_2_index == USHORT_MAX, "Invalid ConvexHull state");
			// Need to assign to the edge this face index
			edge.face_2_index = face_index;
		}
	}
	else {
		// Create a new face
		face_index = faces.AddAssert({ PlaneScalar::FromNormalized(normalized_normal, edge_point_1) });
		faces[face_index].point_count = 3;
		faces[face_index].points[0] = edge.point_1;
		faces[face_index].points[1] = edge.point_2;
		faces[face_index].points[2] = point_index;
		// Need to assign the face to this first slot
		edge.face_1_index = face_index;
	}

	// For the "new" 2 triangle edges, set a new face entry
	first_new_edge_ptr->AddFaceAssert(face_index);
	second_new_edge_ptr->AddFaceAssert(face_index);

	if (remove_edge) {
		edges.RemoveSwapBack(edge_index);
	}
}

void ConvexHull::AddTriangle(unsigned int vertex_A, unsigned int vertex_B, unsigned int vertex_C, float3 hull_center)
{
	float3 A = GetPoint(vertex_A);
	float3 B = GetPoint(vertex_B);
	float3 C = GetPoint(vertex_C);
	PlaneScalar triangle_plane = ComputePlaneAway(A, B, C, hull_center);
	unsigned short face_index = faces.AddAssert({ triangle_plane });
	faces[face_index].points[0] = vertex_A;
	faces[face_index].points[1] = vertex_B;
	faces[face_index].points[2] = vertex_C;
	faces[face_index].point_count = 3;

	auto add_edge = [this, face_index](unsigned int edge_1, unsigned int edge_2) {
		unsigned int edge_index = AddOrFindEdge(edge_1, edge_2);
		ConvexHullEdge* edge = &edges[edge_index];
		edge->AddFaceAssert(face_index);
	};
	
	add_edge(vertex_A, vertex_B);
	add_edge(vertex_A, vertex_C);
	add_edge(vertex_B, vertex_C);
}

unsigned int ConvexHull::FindEdge(unsigned int edge_point_1, unsigned int edge_point_2)
{
	for (unsigned int index = 0; index < edges.size; index++) {
		if ((edges[index].point_1 == edge_point_1 && edges[index].point_2 == edge_point_2)
			|| (edges[index].point_1 == edge_point_2 && edges[index].point_2 == edge_point_1)) {
			return index;
		}
	}
	return -1;
}

void ConvexHull::Initialize(AllocatorPolymorphic allocator, unsigned int _vertex_size, unsigned int edge_size, unsigned int face_size)
{
	ECS_ASSERT(_vertex_size < USHORT_MAX, "Convex hulls cannot have more than 65k vertices");
	ECS_ASSERT(face_size < USHORT_MAX, "Convex hulls cannot have more than 65k faces");

	vertex_size = 0;
	vertex_capacity = _vertex_size;
	SoAInitialize(allocator, _vertex_size, &vertices_x, &vertices_y, &vertices_z);
	edges.Initialize(allocator, 0, edge_size);
	faces.Initialize(allocator, 0, face_size);
}

void ConvexHull::Copy(const ConvexHull* other, AllocatorPolymorphic allocator, bool deallocate_existent)
{
	vertices_x = other->vertices_x;
	vertices_y = other->vertices_y;
	vertices_z = other->vertices_z;

	if (deallocate_existent) {
		SoACopyReallocate(allocator, other->vertex_size, other->vertex_size, &vertices_x, &vertices_y, &vertices_z);
		edges = other->edges;
		edges.Resize(allocator, other->edges.size);
		faces = other->faces;
		faces.Resize(allocator, other->faces.size);
	}
	else {
		SoACopy(allocator, other->vertex_size, other->vertex_size, &vertices_x, &vertices_y, &vertices_z);
		edges = other->edges.Copy(allocator);
		faces = other->faces.Copy(allocator);
	}

	vertex_size = other->vertex_size;
	vertex_capacity = other->vertex_size;
}

void ConvexHull::Deallocate(AllocatorPolymorphic allocator)
{
	if (vertex_size > 0 && vertices_x != nullptr) {
		ECSEngine::DeallocateEx(allocator, vertices_x);
	}
	edges.DeallocateEx(allocator);
	faces.DeallocateEx(allocator);
	memset(this, 0, sizeof(*this));
}

float3 ConvexHull::FurthestFrom(float3 direction) const
{
	Vec8f max_distance = -FLT_MAX;
	Vector3 max_points;
	Vector3 vector_direction = Vector3().Splat(direction);
	size_t vertex_sizet = vertex_size;
	size_t simd_count = GetSimdCount(vertex_sizet, Vector3::ElementCount());
	for (size_t index = 0; index < simd_count; index += Vector3::ElementCount()) {
		Vector3 values = Vector3().LoadAdjacent(vertices_x, index, vertex_capacity);
		Vec8f distance = Dot(values, vector_direction);
		SIMDVectorMask is_greater = distance > max_distance;
		max_distance = SelectSingle(is_greater, distance, max_distance);
		max_points = Select(is_greater, values, max_points);
	}

	// Consider the remaining
	if (simd_count < vertex_sizet) {
		Vector3 values = Vector3().LoadAdjacent(vertices_x, simd_count, vertex_capacity);
		Vec8f distance = Dot(values, vector_direction);
		// Mask the distance such that we don't consider elements which are out of bounds
		distance.cutoff(vertex_sizet - simd_count);
		SIMDVectorMask is_greater = distance > max_distance;
		max_distance = SelectSingle(is_greater, distance, max_distance);
		max_points = Select(is_greater, values, max_points);
	}

	Vec8f max = HorizontalMax8(max_distance);
	size_t index = HorizontalMax8Index(max_distance, max);
	return max_points.At(index);
}

ConvexHull ECS_VECTORCALL ConvexHull::TransformToTemporary(Matrix matrix, AllocatorPolymorphic allocator) const
{
	ConvexHull hull;

	// Allocate the positions
	hull.Initialize(allocator, vertex_size, 0, 0);
	hull.vertex_size = vertex_size;
	hull.edges = edges;
	hull.faces = faces;
	ApplySIMDConstexpr(vertex_size, Vector3::ElementCount(), [matrix, this, &hull](auto is_normal_iteration, size_t index, size_t count) {
		Vector3 elements;
		if constexpr (is_normal_iteration) {
			elements = Vector3().LoadAdjacent(vertices_x, index, vertex_capacity);
		}
		else {
			elements = Vector3().LoadPartialAdjacent(vertices_x, index, vertex_capacity, count);
		}

		Vector3 transformed_elements = TransformPoint(elements, matrix).AsVector3();
		if constexpr (is_normal_iteration) {
			transformed_elements.StoreAdjacent(hull.vertices_x, index, vertex_capacity);
		}
		else {
			transformed_elements.StorePartialAdjacent(hull.vertices_x, index, vertex_capacity, count);
		}
	});

	return hull;
}

void ConvexHull::Resize(AllocatorPolymorphic allocator, unsigned int new_vertex_capacity, unsigned int new_edge_capacity, unsigned int new_face_capacity) {
	if (new_vertex_capacity != vertex_capacity) {
		// It handles correctly all cases of vertex capacities
		SoACopyReallocate(allocator, vertex_size, new_vertex_capacity, &vertices_x, &vertices_y, &vertices_z);
		vertex_size = std::min(new_vertex_capacity, vertex_size);
		vertex_capacity = new_vertex_capacity;
	}
	
	if (new_edge_capacity != edges.capacity) {
		edges.Resize(allocator, new_edge_capacity);
	}

	if (new_face_capacity != faces.capacity) {
		faces.Resize(allocator, new_face_capacity);
	}
}

void ConvexHull::ReserveVertices(AllocatorPolymorphic allocator, unsigned int count) {
	SoAReserve(allocator, vertex_size, vertex_capacity, count, &vertices_x, &vertices_y, &vertices_z);
}

void ConvexHull::ReserveEdges(AllocatorPolymorphic allocator, unsigned int count) {
	edges.Reserve(allocator, count);
}

void ConvexHull::ReserveFaces(AllocatorPolymorphic allocator, unsigned int count) {
	faces.Reserve(allocator, count);
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
	return GJKPointBool(convex_hull, point);
}

void ConvexHullEdge::AddFaceAssert(unsigned int face_index)
{
	if (face_1_index == USHORT_MAX) {
		face_1_index = face_index;
	}
	else {
		ECS_ASSERT(face_2_index == USHORT_MAX, "Invalid ConvexHull state");
		face_2_index = face_index;
	}
}
