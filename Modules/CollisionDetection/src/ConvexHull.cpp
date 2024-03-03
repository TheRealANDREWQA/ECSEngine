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

void ConvexHull::AddTriangleToEdge(unsigned int edge_index, unsigned int point_index, float3 hull_center, AllocatorPolymorphic face_allocator) {
	ConvexHullEdge& edge = edges[edge_index];

	// Add or find the "new" triangle edges
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
		// TODO: Is it worth implementing the collapsing on the go
		// For close enough edges?

		//PlaneScalar existing_face = faces[edge.face_1_index].plane;
		//if (ComparePlaneDirectionsByAngle(existing_face, normalized_normal, 4.0f)) {
		//	// We can collapse this existing edge
		//	remove_edge = true;
		//	// The face to be assigned to the newly added edges should be this one
		//	face_index = edge.face_1_index;
		//	faces[face_index].AddPoint(point_index, face_allocator);
		//}
		//else {
		//	// Create a new face
		//	face_index = faces.AddAssert({ PlaneScalar::FromNormalized(normalized_normal, edge_point_1) });
		//	faces[face_index].points.Initialize(face_allocator, 0, 4);
		//	faces[face_index].AddPoint(edge.point_1, face_allocator);
		//	faces[face_index].AddPoint(edge.point_2, face_allocator);
		//	faces[face_index].AddPoint(point_index, face_allocator);
		//	// Need to assign to the edge this face index
		//	edge.face_2_index = face_index;
		//}

		// Create a new face
		face_index = faces.AddAssert({ PlaneScalar::FromNormalized(normalized_normal, edge_point_1) });
		faces[face_index].points.Initialize(face_allocator, 0, 4);
		faces[face_index].AddPoint(edge.point_1, face_allocator);
		faces[face_index].AddPoint(edge.point_2, face_allocator);
		faces[face_index].AddPoint(point_index, face_allocator);
		ECS_ASSERT(edge.face_2_index == USHORT_MAX, "Invalid ConvexHull state");
		// Need to assign to the edge this face index
		edge.face_2_index = face_index;
	}
	else {
		// Create a new face
		face_index = faces.AddAssert({ PlaneScalar::FromNormalized(normalized_normal, edge_point_1) });
		faces[face_index].points.Initialize(face_allocator, 0, 4);
		faces[face_index].AddPoint(edge.point_1, face_allocator);
		faces[face_index].AddPoint(edge.point_2, face_allocator);
		faces[face_index].AddPoint(point_index, face_allocator);
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

void ConvexHull::AddTriangle(unsigned int vertex_A, unsigned int vertex_B, unsigned int vertex_C, float3 hull_center, AllocatorPolymorphic face_allocator)
{
	float3 A = GetPoint(vertex_A);
	float3 B = GetPoint(vertex_B);
	float3 C = GetPoint(vertex_C);
	PlaneScalar triangle_plane = ComputePlaneAway(A, B, C, hull_center);
	unsigned short face_index = faces.AddAssert({ triangle_plane });
	faces[face_index].points.Initialize(face_allocator, 0, 4);
	faces[face_index].AddPoint(vertex_A, face_allocator);
	faces[face_index].AddPoint(vertex_B, face_allocator);
	faces[face_index].AddPoint(vertex_C, face_allocator);

	auto add_edge = [this, face_index](unsigned int edge_1, unsigned int edge_2) {
		unsigned int edge_index = AddOrFindEdge(edge_1, edge_2);
		ConvexHullEdge* edge = &edges[edge_index];
		edge->AddFaceAssert(face_index);
	};
	
	add_edge(vertex_A, vertex_B);
	add_edge(vertex_A, vertex_C);
	add_edge(vertex_B, vertex_C);
}

void ConvexHull::CalculateAndAssignCenter()
{
	center = float3::Splat(0.0f);
	for (unsigned int index = 0; index < vertex_size; index++) {
		center += GetPoint(index);
	}
	center *= float3::Splat(1.0f / (float)vertex_size);
}

void ConvexHull::ClipFace(
	unsigned int face_index, 
	const ConvexHull* incident_hull, 
	unsigned int incident_hull_face_index, 
	CapacityStream<float3>* points
) const {
	// To clip a face against another face, we need to construct the side
	// Planes of the current face, and clip the other face against them
	// There 2 ways to SIMDize this. Test a single segment against multiple
	// Planes, or test a plane against multiple segments. At the moment, using
	// The plane against multiple segments seems to be easier. There is also
	// An engine function for this to help out

	const ConvexHullFace& face = faces[face_index];
	const ConvexHullFace& incident_face = incident_hull->faces[incident_hull_face_index];

	// TODO: Delay the SIMD implementation until we have a better picture of the conditions
	
	// Clip each edge from the incident face against all side planes of the reference face
	// We can early exit in case the line segment becomes degenerate (it is complitely clipped)
	for (unsigned int index = 0; index < incident_face.EdgeCount(); index++) {
		float t_min = InitializeClipTMin<float3>();
		float t_max = InitializeClipTMax<float3>();

		Line3D incident_edge = incident_hull->GetFaceEdge(incident_hull_face_index, index);
		float3 incident_edge_normalized_direction = Normalize(incident_edge.B - incident_edge.A);
		float t_factor = InitializeClipTFactor(incident_edge.A, incident_edge.B);

		for (unsigned int side_plane_index = 0; side_plane_index < face.EdgeCount(); side_plane_index++) {
			PlaneScalar side_plane = GetFaceSidePlane(face_index, side_plane_index);
			if (!ClipSegmentAgainstPlane(side_plane, incident_edge.A, incident_edge_normalized_direction, t_factor, t_min, t_max)) {
				// Early exit if the edge is completely culled
				break;
			}
		}

		// Check the t_min and t_max factors
		if (ClipSegmentsValidStatus(t_min, t_max)) {
			// The edge still has points, add them to the output
			float3 first_point = ClipSegmentCalculatePoint(incident_edge.A, incident_edge_normalized_direction, t_min, t_factor);
			float3 second_point = ClipSegmentCalculatePoint(incident_edge.A, incident_edge_normalized_direction, t_max, t_factor);
			// If the points are close enough, weld them
			points->AddAssert(first_point);
			if (!CompareMask(first_point, second_point)) {
				points->AddAssert(second_point);
			}
		}
	}

	//// We need to preallocate some values for the incident face segments
	//size_t incident_face_vector_count = SlotsFor(incident_face.points.size, Vector3::ElementCount());
	//
	//ECS_STACK_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32);
	//// We can use stack allocations for this
	//Vector3* incident_point_start = (Vector3*)stack_allocator.Allocate(sizeof(Vector3) * incident_face_vector_count);
	//Vector3* incident_normalized_directions = (Vector3*)stack_allocator.Allocate(sizeof(Vector3) * incident_face_vector_count);
	//Vec8f* incident_t_factors = (Vec8f*)stack_allocator.Allocate(sizeof(Vec8f) * incident_face_vector_count);
	//Vec8f* incident_t_min = (Vec8f*)stack_allocator.Allocate(sizeof(Vec8f) * incident_face_vector_count);
	//Vec8f* incident_t_max = (Vec8f*)stack_allocator.Allocate(sizeof(Vec8f) * incident_face_vector_count);
	//
	//// Initialize these values
	//// We can reinterpret these pointers to write the values
	//// And then continue using them as SIMD
	//float3* incident_point_start_scalar = (float3*)incident_point_start;
	//float3* incident_normalized_directions_scalar = (float3*)incident_normalized_directions;
	//for (unsigned int index = 0; index < incident_face.points.size; index++) {
	//	
	//}

	//for (unsigned int edge_index = 0; edge_index < face.EdgeCount(); edge_index++) {
	//	Plane current_side_plane = GetFaceSidePlane(face_index, )
	//}
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
	center = float3::Splat(0.0f);
}

float3 ConvexHull::GetFaceCenter(unsigned int face_index) const {
	float3 center = float3::Splat(0.0f);
	
	const ConvexHullFace& face = faces[face_index];
	for (unsigned int index = 0; index < face.points.size; index++) {
		center += GetPoint(face.points[index]);
	}

	center *= float3::Splat(1.0f / (float)face.points.size);
	return center;
}

PlaneScalar ConvexHull::GetFaceSidePlane(unsigned int face_index, unsigned int face_edge_index) const {
	const ConvexHullFace& face = faces[face_index];
	Line3D line = GetFaceEdge(face_index, face_edge_index);

	// To construct the normal for the side plane, we can cross the segment direction and the face normal
	// And it should point towards the center of the mesh
	float3 side_normal = Cross(line.B - line.A, face.plane.normal);
	return PlaneScalar{ side_normal, line.A };
}

Line3D ConvexHull::GetFaceEdge(unsigned int face_index, unsigned int face_edge_index) const {
	const ConvexHullFace& face = faces[face_index];
	unsigned short first_point_index = face.points[face_edge_index];
	unsigned short second_point_index = face_edge_index == face.points.size - 1 ? face.points[0] : face.points[face_edge_index + 1];
	float3 first_point = GetPoint(first_point_index);
	float3 second_point = GetPoint(second_point_index);
	return { first_point, second_point };
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

unsigned int ConvexHull::SupportFace(float3 direction) const
{
	float dot = -FLT_MAX;
	unsigned int face_index = -1;
	for (unsigned int index = 0; index < faces.size; index++) {
		float current_dot = Dot(faces[index].plane.normal, direction);
		if (current_dot > dot) {
			dot = current_dot;
			face_index = index;
		}
	}

	return face_index;
}

void ConvexHull::ReallocateFaces(AllocatorPolymorphic allocator) {
	StreamCoalescedInplaceDeepCopy(faces, allocator);
}

void ConvexHull::ReallocateFaces(AllocatorPolymorphic allocator, AllocatorPolymorphic face_allocator) {
	StreamCoalescedInplaceDeepCopyWithDeallocate(faces, allocator, face_allocator);
}

void ConvexHull::RedirectEdges()
{
	for (unsigned int index = 0; index < edges.size; index++) {
		ConvexHullEdge& edge = edges[index];
		float3 first_normal = faces[edge.face_1_index].plane.normal;
		float3 second_normal = faces[edge.face_2_index].plane.normal;
		float3 cross_product = Cross(first_normal, second_normal);
		float3 edge_direction = GetPoint(edge.point_2) - GetPoint(edge.point_1);
		// Include the 0.0f case, that can happen when the cross poduct is close to 0.0f
		// (for almost parallel normals. This should be a rare occurence, since that would
		// Mean that we have nearly coplanar faces, and these should be merged)
		bool same_direction = Dot(edge_direction, cross_product) >= 0.0f;
		if (!same_direction) {
			std::swap(edge.point_1, edge.point_2);
		}
	}
}

void ConvexHull::RemoveDegenerateEdges(AllocatorPolymorphic allocator) {
	// Go through the vertices. If there is a single
	// Edge that connects to it, we can remove that edge and that point as well

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);

	// In order to perform this fast, create a parallel array to the vertices
	// Where we hold this extra information
	struct VertexConnection {
		CapacityStream<unsigned short> edges;
	};
	Stream<VertexConnection> connections;
	connections.Initialize(&stack_allocator, vertex_size);
	for (unsigned int index = 0; index < vertex_size; index++) {
		// Preallocate a small number of edge for each entry
		connections[index].edges.Initialize(&stack_allocator, 0, 5);
	}

	for (unsigned int index = 0; index < edges.size; index++) {
		const unsigned int GROWTH_FACTOR = 4;
		connections[edges[index].point_1].edges.AddResize(index, &stack_allocator, GROWTH_FACTOR);
		connections[edges[index].point_2].edges.AddResize(index, &stack_allocator, GROWTH_FACTOR);
	}

	for (unsigned int index = 0; index < vertex_size; index++) {
		if (connections[index].edges.size <= 1) {
			// Remove any edges that refer to this point
			// Do this before removing the point
			if (connections[index].edges.size == 1) {
				unsigned int edge_to_be_removed = connections[index].edges[0];
				edges.RemoveSwapBack(edge_to_be_removed);
				unsigned int edge_to_be_restored = edges.size;

				for (unsigned int subindex = 0; subindex < vertex_size; subindex++) {
					for (unsigned int connection_index = 0; connection_index < connections[subindex].edges.size; connection_index++) {
						if (connections[subindex].edges[connection_index] == edge_to_be_removed) {
							connections[subindex].edges.RemoveSwapBack(connection_index);
						}
						else if (connections[subindex].edges[connection_index] == edge_to_be_restored) {
							connections[subindex].edges[connection_index] = edge_to_be_removed;
						}
					}
				}
			}

			// Remove this point
			SoARemoveSwapBack(vertex_size, index, vertices_x, vertices_y, vertices_z);

			// We need to restore the references for the edges that referenced the last point
			unsigned int invalid_referenced_index = vertex_size;
			for (unsigned int subindex = 0; subindex < connections[invalid_referenced_index].edges.size; subindex++) {
				ConvexHullEdge& edge = edges[connections[invalid_referenced_index].edges[subindex]];
				if (edge.point_1 == invalid_referenced_index) {
					edge.point_1 = index;
				}
				else if (edge.point_2 == invalid_referenced_index) {
					edge.point_2 = index;
				}
			}
			connections.RemoveSwapBack(index);
			index--;
		}
	}

	if (allocator.allocator != nullptr) {
		Resize(allocator, vertex_size, edges.size, faces.size);
	}
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

ConvexHull ECS_VECTORCALL ConvexHull::TransformToTemporary(Matrix matrix, AllocatorPolymorphic allocator) const
{
	ConvexHull hull;

	// Allocate the positions
	hull.Initialize(allocator, vertex_size, 0, faces.size);
	hull.vertex_size = vertex_size;
	hull.edges = edges;
	ApplySIMDConstexpr(vertex_size, Vector3::ElementCount(), [matrix, this, &hull](auto is_normal_iteration, size_t index, size_t count) {
		Vector3 elements;
		if constexpr (is_normal_iteration) {
			elements = Vector3().LoadAdjacent(vertices_x, index, vertex_capacity);
		}
		else {
			elements = Vector3().LoadPartialAdjacent(vertices_x, index, vertex_capacity, count);
		}

		Vector3 transformed_elements = TransformPoint(elements, matrix).xyz();
		if constexpr (is_normal_iteration) {
			transformed_elements.StoreAdjacent(hull.vertices_x, index, vertex_capacity);
		}
		else {
			transformed_elements.StorePartialAdjacent(hull.vertices_x, index, vertex_capacity, count);
		}
	});

	float4 matrix_values[4];
	matrix.Store(matrix_values);
	float3 translation = matrix_values[3].xyz();
	for (unsigned int index = 0; index < faces.size; index++) {
		hull.faces[index] = faces[index];
		hull.faces[index].plane.dot += Dot(faces[index].plane.normal, translation);
	}
	hull.faces.size = faces.size;
	hull.center = center + translation;

	return hull;
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
		//ECS_ASSERT(face_2_index == USHORT_MAX, "Invalid ConvexHull state");
		// Here is a case
		//    A - E
		//   /|\  |
		//  / D \ |
		// / / \ \|
		// B      C
		// It can happen that the edge
		// AC already has 2 faces attached to it
		// But when we enclose BC by pivoting around AB,
		// We would assign a new face to AC again. So to
		// Account for this case, ignore edge that have both
		// Of their faces set
		if (face_2_index == USHORT_MAX) {
			face_2_index = face_index;
		}
	}
}

//unsigned int ConvexHullBuilder::AddOrFindVertex(float3 point)
//{
//	// Check first the remapped vertices. If we find it there, we can
//	// Return that index, else use the normal add or find
//	for (unsigned int index = 0; index < remapped_vertices.size; index++) {
//		if (remapped_vertices[index].previous_position == point) {
//			return remapped_vertices[index].index;
//		}
//	}
//	return hull.AddOrFindVertex(point);
//}
//
//void ConvexHullBuilder::DeallocateBuilderOnly() {
//	remapped_vertices.FreeBuffer();
//}
//
//void ConvexHullBuilder::Initialize(AllocatorPolymorphic allocator, unsigned int vertex_size, unsigned int edge_size, unsigned int face_size) {
//	hull.Initialize(allocator, vertex_size, edge_size, face_size);
//	// Initialize with a reasonable value such that we avoid many small resizes
//	remapped_vertices.Initialize(allocator, 128);
//}
