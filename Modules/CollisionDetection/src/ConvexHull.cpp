#include "pch.h"
#include "ConvexHull.h"
#include "GJK.h"

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
	
	// This is needed only if we want collapsing faces on the go
	// We defer the edge removal after we assign all values since the swap back
	// Can interfer with one of the newly added edges
	//bool remove_edge = false;

	unsigned short face_index = USHORT_MAX;
	// Determine if the face on the other side of the edge has the same normal
	// If they do, we can collapse the edge
	float3 edge_point_1 = GetPoint(edge.point_1);
	float3 edge_point_2 = GetPoint(edge.point_2);
	float3 triangle_point = GetPoint(point_index);
	float3 normalized_normal = TriangleNormalNormalized(edge_point_1, edge_point_2, triangle_point);
	ushort3 face_point_order = { edge.point_1, edge.point_2, (unsigned short)point_index };
	if (Dot(normalized_normal, edge_point_1 - hull_center) < 0.0f) {
		normalized_normal = -normalized_normal;
		std::swap(face_point_order.y, face_point_order.z);
	}
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
		faces[face_index].AddPoint(face_point_order.x, face_allocator);
		faces[face_index].AddPoint(face_point_order.y, face_allocator);
		faces[face_index].AddPoint(face_point_order.z, face_allocator);
		ECS_ASSERT(edge.face_2_index == USHORT_MAX, "Invalid ConvexHull state");
		// Need to assign to the edge this face index
		edge.face_2_index = face_index;
	}
	else {
		// Create a new face
		face_index = faces.AddAssert({ PlaneScalar::FromNormalized(normalized_normal, edge_point_1) });
		faces[face_index].points.Initialize(face_allocator, 0, 4);
		faces[face_index].AddPoint(face_point_order.x, face_allocator);
		faces[face_index].AddPoint(face_point_order.y, face_allocator);
		faces[face_index].AddPoint(face_point_order.z, face_allocator);
		// Need to assign the face to this first slot
		edge.face_1_index = face_index;
	}

	// For the "new" 2 triangle edges, set a new face entry
	first_new_edge_ptr->AddFaceAssert(face_index);
	second_new_edge_ptr->AddFaceAssert(face_index);

	//if (remove_edge) {
	//	edges.RemoveSwapBack(edge_index);
	//}
}

void ConvexHull::AddTriangle(unsigned int vertex_A, unsigned int vertex_B, unsigned int vertex_C, float3 hull_center, AllocatorPolymorphic face_allocator)
{
	float3 A = GetPoint(vertex_A);
	float3 B = GetPoint(vertex_B);
	float3 C = GetPoint(vertex_C);
	bool is_facing_away = false;
	PlaneScalar triangle_plane = ComputePlaneAway(A, B, C, hull_center, &is_facing_away);
	ushort3 triangle_order = { (unsigned short)vertex_A, (unsigned short)vertex_B, (unsigned short)vertex_C };
	if (!is_facing_away) {
		std::swap(triangle_order.y, triangle_order.z);
	}
 	unsigned short face_index = faces.AddAssert({ triangle_plane });
	faces[face_index].points.Initialize(face_allocator, 0, 4);
	faces[face_index].AddPoint(triangle_order.x, face_allocator);
	faces[face_index].AddPoint(triangle_order.y, face_allocator);
	faces[face_index].AddPoint(triangle_order.z, face_allocator);

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

Stream<CapacityStream<unsigned int>> ConvexHull::ComputeVertexToEdgeTable(AllocatorPolymorphic allocator) const {
	Stream<CapacityStream<unsigned int>> table;
	table.Initialize(allocator, vertex_size);

	for (unsigned int index = 0; index < vertex_size; index++) {
		// Start with a small number of connections
		table[index].Initialize(allocator, 0, 4);
	}

	for (unsigned int index = 0; index < edges.size; index++) {
		const ConvexHullEdge& edge = edges[index];
		CapacityStream<unsigned int>* first_mapping = &table[edge.point_1];
		first_mapping->AddResize(index, allocator, 2);
		CapacityStream<unsigned int>* second_mapping = &table[edge.point_2];
		second_mapping->AddResize(index, allocator, 2);
	}

	return table;
}

Stream<CapacityStream<unsigned int>> ConvexHull::ComputeVertexToVertexTableFromEdges(AllocatorPolymorphic allocator) const {
	Stream<CapacityStream<unsigned int>> table;
	table.Initialize(allocator, vertex_size);

	for (unsigned int index = 0; index < vertex_size; index++) {
		// Start with a small number of connections
		table[index].Initialize(allocator, 0, 4);
	}

	for (unsigned int index = 0; index < edges.size; index++) {
		const ConvexHullEdge& edge = edges[index];
		CapacityStream<unsigned int>* first_mapping = &table[edge.point_1];
		first_mapping->AddResize(edge.point_2, allocator, 2);
		CapacityStream<unsigned int>* second_mapping = &table[edge.point_2];
		second_mapping->AddResize(edge.point_1, allocator, 2);
	}
	
	return table;
}

Stream<CapacityStream<unsigned int>> ConvexHull::ComputeVertexToVertexTableFromFaces(AllocatorPolymorphic allocator) const {
	Stream<CapacityStream<unsigned int>> table;
	table.Initialize(allocator, vertex_size);

	for (unsigned int index = 0; index < vertex_size; index++) {
		// Start with a small number of connections
		table[index].Initialize(allocator, 0, 4);
	}

	for (unsigned int index = 0; index < faces.size; index++) {
		const ConvexHullFace& face = faces[index];
		for (unsigned int point_index = 0; point_index < face.points.size; point_index++) {
			unsigned int next_point_index = point_index == face.points.size - 1 ? 0 : point_index + 1;
			table[face.points[point_index]].AddResize(face.points[next_point_index], allocator, 2);
		}
	}

	return table;
}

unsigned int ConvexHull::FindEdge(unsigned int edge_point_1, unsigned int edge_point_2) const
{
	for (unsigned int index = 0; index < edges.size; index++) {
		if ((edges[index].point_1 == edge_point_1 && edges[index].point_2 == edge_point_2)
			|| (edges[index].point_1 == edge_point_2 && edges[index].point_2 == edge_point_1)) {
			return index;
		}
	}
	return -1;
}

unsigned int ConvexHull::FindFace(Stream<unsigned int> face_points) const {
	// We can speed this up by searching for an edge, and then checking
	// The associated faces. It needs to be one of them
	unsigned int edge_index = FindEdge(face_points[0], face_points[1]);
	ECS_ASSERT(edge_index != -1);
	const ConvexHullEdge& edge = edges[edge_index];

	// Returns true if it matches the points
	auto test_face = [&](unsigned int face_index) {
		const ConvexHullFace& face = faces[face_index];
		if (face.points.size != face_points.size) {
			return false;
		}
		unsigned int index = 0;
		for (; index < face.points.size; index++) {
			if (face_points.Find(face.points[index]) == -1) {
				break;
			}
		}
		return index == face.points.size;
	};

	if (edge.face_1_index != USHORT_MAX) {
		if (test_face(edge.face_1_index)) {
			return edge.face_1_index;
		}
	}

	if (edge.face_2_index != USHORT_MAX) {
		if (test_face(edge.face_2_index)) {
			return edge.face_2_index;
		}
	}

	// This face could not be found or the edges are degenerate
	// And do not properly represent the hull
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
	// Since the points are in counterclockwise order, the normal will always points towards the center
	// Of the face
	float3 side_normal = Cross(line.B - line.A, face.plane.normal);
	return PlaneScalar{ side_normal, line.A };
}

Line3D ConvexHull::GetFaceEdge(unsigned int face_index, unsigned int face_edge_index) const {
	uint2 indices = GetFaceEdgeIndices(face_index, face_edge_index);
	float3 first_point = GetPoint(indices.x);
	float3 second_point = GetPoint(indices.y);
	return { first_point, second_point };
}

uint2 ConvexHull::GetFaceEdgeIndices(unsigned int face_index, unsigned int face_edge_index) const
{
	const ConvexHullFace& face = faces[face_index];
	unsigned short first_point = face.points[face_edge_index];
	unsigned short second_point = face_edge_index == face.points.size - 1 ? face.points[0] : face.points[face_edge_index + 1];
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
		// We also need to reallocate the faces, since these contain buffers, 
		// such that there is a single allocation made - to conform for the deallocate
		ReallocateFaces(allocator);
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
		Vec8fb select_mask = SelectMaskLast<unsigned int>(vertex_sizet - simd_count);
		Vec8f smallest_distance = -FLT_MAX;
		distance = select(select_mask, smallest_distance, distance);
		SIMDVectorMask is_greater = distance > max_distance;
		max_distance = SelectSingle(is_greater, distance, max_distance);
		max_points = Select(is_greater, values, max_points);
	}

	Vec8f max = HorizontalMax8(max_distance);
	size_t index = HorizontalMax8Index(max_distance, max);
	return max_points.At(index);
}

void ConvexHull::MergeCoplanarTriangles(float coplanarity_degrees, AllocatorPolymorphic allocator, AllocatorPolymorphic previous_face_allocator) {
	float coplanarity_cosine = cos(DegToRad(coplanarity_degrees));
	for (unsigned int index = 0; index < edges.size; index++) {
		const ConvexHullEdge& edge = edges[index];
		ConvexHullFace& first_face = faces[edge.face_1_index];
		ConvexHullFace& second_face = faces[edge.face_2_index];
		if (first_face.points.size == 3 && second_face.points.size == 3) {
			// Both are triangles, test the angle
			if (ComparePlaneDirectionsByCosine(first_face.plane, second_face.plane.normal, coplanarity_cosine)) {
				// Merge these 2 together
				unsigned int first_face_point_1_index = first_face.points.Find(edge.point_1);
				unsigned int first_face_point_2_index = first_face.points.Find(edge.point_2);
				unsigned int second_face_point_1_index = second_face.points.Find(edge.point_1);
				unsigned int second_face_point_2_index = second_face.points.Find(edge.point_2);

				unsigned int first_face_min_index = min(first_face_point_1_index, first_face_point_2_index);
				unsigned int first_face_max_index = max(first_face_point_1_index, first_face_point_2_index);
				unsigned int second_face_min_index = min(second_face_point_1_index, second_face_point_2_index);
				unsigned int second_face_max_index = max(second_face_point_1_index, second_face_point_2_index);

				ECS_STACK_CAPACITY_STREAM(unsigned short, second_face_temporary_points, 3);
				second_face_temporary_points.CopyOther(second_face.points);

				if (previous_face_allocator.allocator != nullptr) {
					ECS_STACK_CAPACITY_STREAM(unsigned short, first_face_temporary_points, 3);
					first_face_temporary_points.CopyOther(first_face.points);

					first_face.points.Deallocate(previous_face_allocator);
					second_face.points.Deallocate(previous_face_allocator);

					first_face.points.Initialize(allocator, 3, 4);
					first_face.points.CopyOther(first_face_temporary_points);
				}
				else {
					first_face.points.Reserve(allocator);
					second_face.points.DeallocateIfBelongs(allocator);
				}
				// Add the next point from the second face after maximum between the first face
				// The next point can be simply deduced as (second_face_max_index + 1
				unsigned short point_to_add = -1;
				if (second_face_min_index == 0) {
					if (second_face_max_index == 2) {
						point_to_add = second_face_temporary_points[1];
					}
					else {
						// The second point must be 1
						point_to_add = second_face_temporary_points[2];
					}
				}
				else {
					// The min index is 1, the other index is 2
					point_to_add = second_face_temporary_points[0];
				}

				unsigned int insert_position = -1;
				if (first_face_min_index == 0) {
					if (first_face_max_index == 2) {
						insert_position = 3;
					}
					else {
						insert_position = 1;
					}
				}
				else {
					insert_position = 2;
				}
				first_face.points.Insert(insert_position, point_to_add);

				auto redirect_edge = [&](unsigned short edge_1, unsigned short edge_2) {
					unsigned int edge_index = FindEdge(edge_1, edge_2);
					ECS_ASSERT(edge_index != -1, "ConvexHull invalid state when trying to merge coplanar triangles");
					if (edges[edge_index].face_1_index == edge.face_2_index) {
						edges[edge_index].face_1_index = edge.face_1_index;
					}
					else {
						ECS_ASSERT(edges[edge_index].face_2_index == edge.face_2_index, "ConvexHull invalid state when trying to merge coplanar triangles");
						edges[edge_index].face_2_index = edge.face_1_index;
					}
				};

				// Redirect the other edges of the triangle to the first face
				redirect_edge(edge.point_1, point_to_add);
				redirect_edge(edge.point_2, point_to_add);

				unsigned short second_face_index = edge.face_2_index;

				// We can delete this current edge as well, before the face
				RemoveSwapBackEdge(index);

				// We need to remove the face after we remove the current edge
				RemoveSwapBackFace(second_face_index);

				index--;
			}
		}
	}
}

void ConvexHull::SimplifyTrianglesAndQuads(float area_factor, AllocatorPolymorphic allocator) {
	// At the moment, collapse small triangles. Sliver triangles are also possible candidates
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 32);

	// We can create a parallel array for each vertex to hold the edges that are connected
	Stream<CapacityStream<unsigned int>> vertex_connections = ComputeVertexToEdgeTable(&stack_allocator);
	Stream<float> face_areas;
	face_areas.Initialize(&stack_allocator, faces.size);

	auto remove_swap_back_edge = [&](unsigned int edge_index) {
		// Update the connections to exclude this edge
		CapacityStream<unsigned int>* first_connections = &vertex_connections[edges[edge_index].point_1];
		first_connections->RemoveSwapBackByValue(edge_index, "ConvexHull simplifying triangles inconsistent state");
		CapacityStream<unsigned int>* second_connections = &vertex_connections[edges[edge_index].point_2];
		second_connections->RemoveSwapBackByValue(edge_index, "ConvxHull simplifying triangles inconsistent state");

		edges.RemoveSwapBack(edge_index);
		if (edge_index != edges.size) {
			// We need to repair the references to the edges inside the connections table
			const ConvexHullEdge& swapped_edge = edges[edge_index];
			CapacityStream<unsigned int>& swapped_edge_first_connections = vertex_connections[swapped_edge.point_1];
			swapped_edge_first_connections.ReplaceByValue(edges.size, edge_index, "ConvexHull simplifying triangles invalid state");
			CapacityStream<unsigned int>& swapped_edge_second_connections = vertex_connections[swapped_edge.point_2];
			swapped_edge_second_connections.ReplaceByValue(edges.size, edge_index, "ConvexHull simplifying triangles invalid state");
		}
	};

	auto repair_edge_face_reference = [&](unsigned int previous_face_index, unsigned int new_face_index) {
		// Since we know that these are triangles, we can stop if the count hits 3
		unsigned int count = 0;
		for (unsigned int index = 0; index < edges.size; index++) {
			if (edges[index].face_1_index == previous_face_index) {
				edges[index].face_1_index = new_face_index;
				count++;
				if (count == 3) {
					break;
				}
			}
			else if (edges[index].face_2_index == previous_face_index) {
				edges[index].face_2_index = new_face_index;
				count++;
				if (count == 3) {
					break;
				}
			}
		}
	};

	// Makes use of the vertex connections to speed this up
	// If the add connections is set to true, it will append
	// The connections of the previous to the new, except the
	// Identity connection and those that would be duplicates
	// If the add connections is set to true, a capacity stream
	// With extra faces to be collapsed will be filled in
	auto repair_vertex_references = [&](
		unsigned int previous_index, 
		unsigned int new_index, 
		bool add_connections_and_recalculate_triangles,
		CapacityStream<unsigned int>* extra_faces_to_be_collapsed
	) {
		Stream<unsigned int> connections = vertex_connections[previous_index];
		CapacityStream<unsigned int>& new_connections = vertex_connections[new_index];
		ECS_STACK_CAPACITY_STREAM(unsigned short, processed_faces, 128);
		ECS_ASSERT(processed_faces.capacity >= connections.size, "ConvexHull simplifying triangles insufficient stack space");

		if (add_connections_and_recalculate_triangles) {
			// We don't need to include the collapsed edge
			new_connections.Reserve(&stack_allocator, connections.size - 1);
		}

		ECS_STACK_CAPACITY_STREAM(unsigned int, edges_to_be_removed, 512);

		for (size_t index = 0; index < connections.size; index++) {
			ConvexHullEdge& current_redirect_edge = edges[connections[index]];

			ConvexHullFace& first_face = faces[current_redirect_edge.face_1_index];
			ConvexHullFace& second_face = faces[current_redirect_edge.face_2_index];
			first_face.points.TryReplaceByValue(previous_index, new_index);
			second_face.points.TryReplaceByValue(previous_index, new_index);

			bool is_edge_going_to_be_collapsed = false;
			if (add_connections_and_recalculate_triangles) {
				// A face may repeat itself since 2 edges will reference it
				// Keep an array of the processed faces
				auto update_face = [&](unsigned int face_index) {
					bool was_processed = processed_faces.Find(face_index) != -1;
					if (!was_processed) {
						ConvexHullFace& face = faces[face_index];

						float3 A = GetPoint(face.points[0]);
						float3 B = GetPoint(face.points[1]);
						float3 C = GetPoint(face.points[2]);

						float3 cross = Cross(B - A, C - A);
						// Redirect the normal outside the hull
						if (Dot(cross, A - center) < 0.0f) {
							cross = -cross;
						}

						float cross_length = Length(cross);
						float new_area = cross_length * 0.5f;
						PlaneScalar new_plane = PlaneScalar::FromNormalized(cross / cross_length, A);
						face.plane = new_plane;
						face_areas[face_index] = fabsf(new_area);
						processed_faces.AddAssert(face_index);
					}
				};

				// If the connection already exists in the new_connections, it means
				// This is one of the edges of the triangles that get collapsed. We shouldn't add the
				// Connection again, and we can skip the face update as well
				unsigned short other_connection_point = current_redirect_edge.point_1 == previous_index ?
					current_redirect_edge.point_2 : current_redirect_edge.point_1;
				unsigned int target_remapped_edge_index = FindEdge(new_index, other_connection_point);
				bool exists_connection = false;
				if (target_remapped_edge_index != -1) {
					exists_connection = new_connections.Find(target_remapped_edge_index) != -1;
				}

				if (!exists_connection) {
					new_connections.Add(connections[index]);

					update_face(current_redirect_edge.face_1_index);
					update_face(current_redirect_edge.face_2_index);
				}
				else {
					is_edge_going_to_be_collapsed = true;

					// We need to remove this edge from existence, it means that the
					// Edge is duplicate. Put these on a stack buffer to commit them
					// At the end since this can cause interference with the loop
					// connections
					edges_to_be_removed.AddAssert(connections[index]);
					// We need to update the target remapped edge with the face index
					// Of the face on the other side of the collapsing edge
					// Determine the face that is being collapsed, by checking to see if
					// The new_index point is found 2 times in it. We have already remapped
					// The previous index to the new index, and this is why we have to check
					// For the new index 2 times
					unsigned int remap_face_new_index = -1;
					unsigned int remap_face_previous_index = -1;
					unsigned int first_face_collapsed_count = (first_face.points[0] == new_index)
						+ (first_face.points[1] == new_index) + (first_face.points[2] == new_index);
					if (first_face_collapsed_count == 2) {
						remap_face_new_index = current_redirect_edge.face_2_index;
						remap_face_previous_index = current_redirect_edge.face_1_index;
					}
					else {
						remap_face_new_index = current_redirect_edge.face_1_index;
						remap_face_previous_index = current_redirect_edge.face_2_index;
					}

					ConvexHullEdge& remap_edge_face = edges[target_remapped_edge_index];
					if (remap_edge_face.face_1_index == remap_face_previous_index) {
						remap_edge_face.face_1_index = remap_face_new_index;
					}
					else {
						if (remap_edge_face.face_2_index == remap_face_previous_index) {
							// It can happen that this is an interior triangle
							// And does not actually share an edge
							remap_edge_face.face_2_index = remap_face_new_index;
						}
						else {
							// This is the case that another extra edge is collapsed,
							// And with it another face must be collapsed. That face is
							// The remap face previous
							extra_faces_to_be_collapsed->AddAssert(remap_face_previous_index);
						}
					}
				}
			}

			// For collapsing edges, don't do this
			if (!is_edge_going_to_be_collapsed) {
				if (current_redirect_edge.point_1 == previous_index) {
					current_redirect_edge.point_1 = new_index;
				}
				else {
					current_redirect_edge.point_2 = new_index;
				}
			}
		}

		for (unsigned int index = 0; index < edges_to_be_removed.size; index++) {
			remove_swap_back_edge(edges_to_be_removed[index]);
			// Update the index of the edges to be removed in case it is the last edge
			for (unsigned int subindex = index + 1; subindex < edges_to_be_removed.size; subindex++) {
				if (edges_to_be_removed[subindex] == edges.size) {
					edges_to_be_removed[subindex] = edges_to_be_removed[index];
					break;
				}
			}
		}
	};

	// Collapsing the first point to the second point
	auto collapse_edge = [&](unsigned short first_point, unsigned short second_point) {
		// Find the edge that corresponds to these points
		unsigned int edge_index = FindEdge(first_point, second_point);
		ECS_ASSERT(edge_index != -1, "ConvexHull simplifying triangles failed");
		ConvexHullEdge edge = edges[edge_index];

		// Remove the edge right now, before the vertex removal
		remove_swap_back_edge(edge_index);

		// We can remove the current vertex
		SoARemoveSwapBack(vertex_size, first_point, vertices_x, vertices_y, vertices_z);
		// We could technically check that second point is different from the last vertex
		// And that would give a speed up in that case. But I kept it like this to keep it simple

		// Redirect the edges and the faces that referenced the first point to the second
		ECS_STACK_CAPACITY_STREAM(unsigned int, faces_to_be_collapsed, 8);
		// Add the current 2 faces to be collapsed
		faces_to_be_collapsed.Add(edge.face_1_index);
		faces_to_be_collapsed.Add(edge.face_2_index);
		repair_vertex_references(first_point, second_point, true, &faces_to_be_collapsed);
		// And for the last vertex to the first point index
		if (vertex_size != first_point) {
			repair_vertex_references(vertex_size, first_point, false, nullptr);
		}
		// Remove the vertex connections now - we need to do this after the vertex repair
		vertex_connections.RemoveSwapBack(first_point);

		// We need to remove the faces that correspond to this edge
		if (allocator.allocator != nullptr) {
			faces[edge.face_1_index].points.Deallocate(allocator);
			faces[edge.face_2_index].points.Deallocate(allocator);
		}

		// Remove the faces - don't forget to update the triangle areas as well
		for (unsigned int index = 0; index < faces_to_be_collapsed.size; index++) {
			faces.RemoveSwapBack(faces_to_be_collapsed[index]);
			face_areas.RemoveSwapBack(faces_to_be_collapsed[index]);
			repair_edge_face_reference(faces.size, faces_to_be_collapsed[index]);
			for (unsigned int subindex = index + 1; subindex < faces_to_be_collapsed.size; subindex++) {
				if (faces_to_be_collapsed[subindex] == faces.size) {
					faces_to_be_collapsed[subindex] = faces_to_be_collapsed[index];
					break;
				}
			}
		}
	};


	// Create a parallel array for the triangle areas
	// And record the total area
	float total_mesh_area = 0.0f;
	for (unsigned int index = 0; index < faces.size; index++) {
		const ConvexHullFace& face = faces[index];
		float3 a = GetPoint(face.points[0]);
		float3 b = GetPoint(face.points[1]);
		float3 c = GetPoint(face.points[2]);
		face_areas[index] = TriangleArea(a, b, c);
		total_mesh_area += face_areas[index];
	}

	const float THRESHOLD_CONSTANT_FACTOR = 1000.0f;

	// To determine the area threshold, we need to divide the total area by a certain factor
	float triangle_threshold = total_mesh_area / THRESHOLD_CONSTANT_FACTOR * area_factor;
	// For each triangle that we find to have an area smaller than the theshold, collapse
	// One of the edge. This ensures that we have points only on the hull, and it makes for
	// A relatively easier implementation
	unsigned int iteration_count = 0;
	for (unsigned int index = 0; index < faces.size; index++) {
		if (face_areas[index] < triangle_threshold) {
			iteration_count++;
			if (iteration_count == 60) {
				break;
			}

			// We need to remove this triangle
			// Choose the smallest edge to use as the collapsing edge
			ConvexHullFace face = faces[index];
			float3 a = GetPoint(face.points[0]);
			float3 b = GetPoint(face.points[1]);
			float3 c = GetPoint(face.points[2]);

			unsigned int ab_edge = FindEdge(face.points[0], face.points[1]);
			unsigned int bc_edge = FindEdge(face.points[1], face.points[2]);
			unsigned int ac_edge = FindEdge(face.points[2], face.points[0]);
			const ConvexHullEdge& ab_edge_ = edges[ab_edge];
			const ConvexHullEdge& bc_edge_ = edges[bc_edge];
			const ConvexHullEdge& ac_edge_ = edges[ac_edge];

			// Determine obtuse angles. Edges that face obtuse
			// Angles should not be collapsed. We can detect them
			// By a negative dot product between the edges
			float3 AB = b - a;
			float3 AC = c - a;
			float3 BC = c - b;

			bool is_a_obtuse = Dot(AB, AC) < 0.0f;
			bool is_b_obtuse = Dot(-AB, BC) < 0.0f;
			bool is_c_obtuse = Dot(-AC, -BC) < 0.0f;

			// Determine the collapse distance for a point
			// As the sum of the absolute distances of the target point 
			// To the planes of the point to be collapsed
			ECS_STACK_CAPACITY_STREAM(unsigned int, verified_planes, 64);

			auto determine_points_errors = [&](Stream<unsigned int> connections, float3 first_point, float3 second_point) {
				verified_planes.size = 0;
				float first_distance = 0.0f;
				float second_distance = 0.0f;

				auto add_distances = [&](unsigned short face_index) {
					verified_planes.AddAssert(face_index);
					float current_first_distance = DistanceToPlane(faces[face_index].plane, first_point);
					float squared_current_first_distance = current_first_distance * current_first_distance;
					first_distance += squared_current_first_distance;
					float current_second_distance = DistanceToPlane(faces[face_index].plane, second_point);
					float squared_current_second_distance = current_second_distance * current_second_distance;
					second_distance += squared_current_second_distance;
				};

				for (unsigned int connection_index = 0; connection_index < connections.size; connection_index++) {
					const ConvexHullEdge& current_edge = edges[connections[connection_index]];
					if (verified_planes.Find(current_edge.face_1_index) == -1) {
						add_distances(current_edge.face_1_index);
					}
					if (verified_planes.Find(current_edge.face_2_index) == -1) {
						add_distances(current_edge.face_2_index);
					}
				}
				return float2{ first_distance, second_distance };
			};

			struct SortPoint {
				ECS_INLINE bool operator < (SortPoint other) const {
					return distance < other.distance;
				}

				ECS_INLINE bool operator <= (SortPoint other) const {
					return distance <= other.distance;
				}

				float distance;
				unsigned short collapse_point;
				unsigned short collapse_target;
			};

			Stream<unsigned int> a_connections = vertex_connections[face.points[0]];
			Stream<unsigned int> b_connections = vertex_connections[face.points[1]];
			Stream<unsigned int> c_connections = vertex_connections[face.points[2]];
			float2 a_distances = determine_points_errors(a_connections, b, c);
			float2 b_distances = determine_points_errors(b_connections, a, c);
			float2 c_distances = determine_points_errors(c_connections, a, b);
			ECS_STACK_CAPACITY_STREAM(SortPoint, errors, 6);
			if (!is_a_obtuse) {
				errors.Add({ b_distances.y, face.points[1], face.points[2] });
				errors.Add({ c_distances.y, face.points[2], face.points[1] });
			}
			if (!is_b_obtuse) {
				errors.Add({ a_distances.y, face.points[0], face.points[2] });
				errors.Add({ c_distances.x, face.points[2], face.points[0] });
			}
			if (!is_c_obtuse) {
				errors.Add({ a_distances.x, face.points[0], face.points[1] });
				errors.Add({ b_distances.x, face.points[1], face.points[0] });
			}

			unsigned short collapse_source = USHORT_MAX;
			unsigned short collapse_target = USHORT_MAX;

			// Determine the collapse point and the target
			// It is the point with the least amount of error
			InsertionSort(errors.buffer, errors.size);

			collapse_source = errors[0].collapse_point;
			collapse_target = errors[0].collapse_target;

			// We must reject a pair that is opposite on the other side to an obtuse
			// Angle. These can create overlapping faces that would be hard to detect
			auto verify_edge = [&](unsigned short collapse_source, unsigned short collapse_target) {
				unsigned int edge_index = FindEdge(collapse_source, collapse_target);
				const ConvexHullEdge& edge = edges[edge_index];
				unsigned short other_face_index = edge.face_1_index == index ? edge.face_2_index : edge.face_1_index;
				const ConvexHullFace& other_face = faces[other_face_index];
				unsigned short other_face_point_index = USHORT_MAX;
				if (other_face.points[0] == collapse_source || other_face.points[0] == collapse_target) {
					if (other_face.points[1] == collapse_source || other_face.points[1] == collapse_target) {
						other_face_point_index = other_face.points[2];
					}
					else {
						other_face_point_index = other_face.points[1];
					}
				}
				else {
					other_face_point_index = other_face.points[0];
				}

				float3 opposing_face_point = GetPoint(other_face_point_index);
				float3 opposing_face_edge_0 = GetPoint(collapse_source) - opposing_face_point;
				float3 opposing_face_edge_1 = GetPoint(collapse_target) - opposing_face_point;
				return Dot(opposing_face_edge_0, opposing_face_edge_1) >= 0.0f;
			};

			unsigned int verified_collapse_index = 0;
			if (!verify_edge(collapse_source, collapse_target)) {
				verified_collapse_index++;
				collapse_source = errors[verified_collapse_index].collapse_point;
				collapse_target = errors[verified_collapse_index].collapse_target;
				while (verified_collapse_index < errors.size && !verify_edge(collapse_source, collapse_target)) {
					verified_collapse_index++;
					collapse_source = errors[verified_collapse_index].collapse_point;
					collapse_target = errors[verified_collapse_index].collapse_target;
				}
			}
			if (verified_collapse_index == errors.size) {
				// Skip this triangle. This shouldn't really happen, but just in case
				continue;
			}

			//float3 AB = b - a;
			//float3 AC = c - a;
			//float3 BC = c - b;

			//float AB_sq_length = SquareLength(AB);
			//float AC_sq_length = SquareLength(AC);
			//float BC_sq_length = SquareLength(BC);

			//bool ab_ac_smaller = AB_sq_length < AC_sq_length;
			//bool ab_bc_smaller = AB_sq_length < BC_sq_length;
			//bool ac_bc_smaller = AC_sq_length < BC_sq_length;

			//unsigned short collapse_edge_1 = USHORT_MAX;
			//unsigned short collapse_edge_2 = USHORT_MAX;
			//if (ab_ac_smaller && ab_bc_smaller) {
			//	// We can collapse the AB edge
			//	collapse_edge_1 = face.points[0];
			//	collapse_edge_2 = face.points[1];
			//}
			//else if (!ab_ac_smaller && ac_bc_smaller) {
			//	// We can collapse the AC edge
			//	collapse_edge_1 = face.points[0];
			//	collapse_edge_2 = face.points[2];
			//}
			//else {
			//	// Collapse the BC edge
			//	collapse_edge_1 = face.points[1];
			//	collapse_edge_2 = face.points[2];
			//}

			//if (collapse_source == 5 && collapse_target == 82) {
			//	//unsigned int face_points[3] = { 96, 115, 53 };
			//	//unsigned int face_index = FindFace({ face_points, 3 });
			//	break;
			//}

			// There is one more thing to be decided, if we collapse from
			// The edge 1 to 2, or from 2 to 1. At the moment, just collapse
			// 1 to 2 until we come up with a metric to decide which direction
			// Is better than the other
			collapse_edge(collapse_source, collapse_target);
			// Decrement the index since we know this face will be removed
			// TODO: We might miss other faces tho, that get swapped before this index
			// Think about a solution
			index--;

			bool validate = Validate();
			if (!validate) {
				validate = validate;
			}
		}
	}
}

unsigned int ConvexHull::SupportFacePoint(unsigned int face_index, float3 direction) const {
	float dot = -FLT_MAX;
	unsigned int vertex_index = -1;
	for (unsigned int index = 0; index < faces[face_index].points.size; index++) {
		float current_dot = Dot(GetPoint(faces[face_index].points[index]), direction);
		if (current_dot > dot) {
			dot = current_dot;
			vertex_index = index;
		}
	}
	return vertex_index;
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


void ConvexHull::RemoveSwapBackVertex(unsigned int index) {
	SoARemoveSwapBack(vertex_size, index, vertices_x, vertices_y, vertices_z);
	
	// Repair the edge and face references for the swapped point
	if (vertex_size != index) {
		// We don't have to iterate the faces, since we know that
		// The edges maintain references to the faces, when we find
		// An edge to be modified, we modify the faces that it is
		// Connected as well. It may result in rechecking of faces,
		ECS_STACK_CAPACITY_STREAM(unsigned short, verified_faces, 128);

		auto remap_face = [&](unsigned int face_index) {
			if (verified_faces.Find(face_index) != -1) {
				verified_faces.AddAssert(face_index);
				ConvexHullFace& face = faces[face_index];
				for (unsigned int subindex = 0; subindex < face.points.size; subindex++) {
					if (face.points[subindex] == vertex_size) {
						face.points[subindex] = index;
						break;
					}
				}
			}
		};

		for (unsigned int edge_index = 0; edge_index < edges.size; edge_index++) {
			if (edges[edge_index].point_1 == vertex_size) {
				edges[edge_index].point_1 = index;
				remap_face(edges[edge_index].face_1_index);
			}
			if (edges[edge_index].point_2 == vertex_size) {
				edges[edge_index].point_2 = index;
				remap_face(edges[edge_index].face_2_index);
			}
		}
	}
}

void ConvexHull::RemoveSwapBackEdge(unsigned int edge_index) {
	edges.RemoveSwapBack(edge_index);
}

void ConvexHull::RemoveSwapBackFace(unsigned int face_index) {
	faces.RemoveSwapBack(face_index);
	// We need to repair the edge references
	if (faces.size != face_index) {
		unsigned int face_edge_count = faces[face_index].EdgeCount();
		// We can exit once the count reaches edge_count
		unsigned int count = 0;
		for (unsigned int index = 0; index < edges.size; index++) {
			if (edges[index].face_1_index == faces.size) {
				edges[index].face_1_index = face_index;
				count++;
				if (count == face_edge_count) {
					break;
				}
			}
			else if (edges[index].face_2_index == faces.size) {
				edges[index].face_2_index = face_index;
				count++;
				if (count == face_edge_count) {
					break;
				}
			}
		}
	}
}

void ConvexHull::RemoveSwapBackFace(unsigned int face_index, AllocatorPolymorphic allocator) {
	faces[face_index].Deallocate(allocator);
	RemoveSwapBackFace(face_index);
}

template<typename Functor>
void ReoderFacePointsImpl(ConvexHull* convex_hull, unsigned int face_index, Stream<CapacityStream<unsigned int>> table, Functor&& functor) {
	// Calculate the center of the face
	float3 face_center = convex_hull->GetFaceCenter(face_index);

	ConvexHullFace& face = convex_hull->faces[face_index];
	auto test_CCW = [convex_hull, face, face_center](unsigned int first_point_index, unsigned int second_point_index) {
		// To test the CCW property, use the cross product of the edge with a displacement
		// From the initial point. If the value is positive, they are in CCW
		float3 first_point = convex_hull->GetPoint(first_point_index);
		float3 second_point = convex_hull->GetPoint(second_point_index);
		float3 cross_product = Cross(first_point - face_center, second_point - first_point);
		float dot = Dot(cross_product, face.plane.normal);
		return dot > 0.0f;
	};

	ECS_STACK_CAPACITY_STREAM(unsigned short, face_points, 512);
	ECS_ASSERT(face.points.size <= face_points.capacity, "ConvexHull too many points for a face");
	face_points.CopyOther(face.points);

	// Choose a support point in a direction
	float3 initial_direction = float3{ 1.0f, 0.0f, 0.0f };
	unsigned int initial_face_vertex_index = convex_hull->SupportFacePoint(face_index, initial_direction);

	unsigned int current_vertex_index = face.points[initial_face_vertex_index];
	face.points[0] = current_vertex_index;
	face.points.size = 1;
	while (face.points.size < face_points.size) {
		Stream<unsigned int> vertex_connections = table[current_vertex_index];
		// Using the connections, determine the vertex that is CCW as this one
		size_t subindex = 0;
		for (; subindex < vertex_connections.size; subindex++) {
			unsigned int other_edge_point = functor(current_vertex_index, vertex_connections[subindex]);
			if (face_points.Find(other_edge_point) != -1 && test_CCW(current_vertex_index, other_edge_point)) {
				face.points.Add(other_edge_point);
				current_vertex_index = other_edge_point;
				break;
			}
		}

		/*if (subindex < vertex_connections.size) {
			float3 a = GetPoint(face.points[0]);
			float3 b = GetPoint(face.points[1]);
			float3 c = GetPoint(face.points[2]);
			float angle = AngleBetweenVectors(b - a, c - a);
			__debugbreak();
		}*/
		//ECS_ASSERT(subindex < vertex_connections.size, "ConvexHull could not reorder faces");
	}

	face_points.CopyTo(face.points.buffer);
}

void ConvexHull::ReorderFacePointsByEdges(unsigned int face_index, Stream<CapacityStream<unsigned int>> vertex_to_edge_table)
{
	ReoderFacePointsImpl(this, face_index, vertex_to_edge_table, [&](unsigned int current_vertex_index, unsigned int entry) {
		const ConvexHullEdge& edge = edges[entry];
		return edge.point_1 == current_vertex_index ? edge.point_2 : edge.point_1;
	});
}

void ConvexHull::ReorderFacePointsByVertices(unsigned int face_index, Stream<CapacityStream<unsigned int>> vertex_to_vertex_table) {
	ReoderFacePointsImpl(this, face_index, vertex_to_vertex_table, [&](unsigned int current_vertex_index, unsigned int entry) {
		return entry;
	});
}

void ConvexHull::ReorderFacePointsByAngles(unsigned int face_index) {
	float3 face_center = GetFaceCenter(face_index);
	ConvexHullFace& face = faces[face_index];

	// Choose a support point in a direction
	float3 initial_direction = float3{ 1.0f, 0.0f, 0.0f };
	unsigned int initial_face_vertex_index = SupportFacePoint(face_index, initial_direction);

	unsigned int edge_count = face.EdgeCount();
	ECS_STACK_CAPACITY_STREAM(unsigned short, remaining_points, 64);
	remaining_points.CopyOther(face.points);
	remaining_points.RemoveSwapBack(initial_face_vertex_index);
	face.points.size = 1;
	face.points[0] = face.points[initial_face_vertex_index];

	// To determine the next point, iterate over all points and compute 2 dots products
	// One is used to determine if the vertex is on the CCW or CW side, and the other
	// To determine the closest point
	unsigned int current_face_vertex_index = face.points[0];
	for (unsigned int index = 0; index < edge_count - 2; index++) {
		// Compute a "right direction", by using the cross product between the face normal
		// And the center to point direction
		float3 current_point = GetPoint(current_face_vertex_index);
		float3 current_center_point_direction = current_point - face_center;
		float3 right_direction = Cross(current_center_point_direction, face.plane.normal);
		float max_dot = -FLT_MAX;
		float max_distance = -FLT_MAX;
		unsigned int candidate_point_index = -1;

		for (unsigned int subindex = 0; subindex < remaining_points.size; subindex++) {
			float3 candidate_point = GetPoint(remaining_points[subindex]);
			float order_dot = Dot(right_direction, candidate_point - face_center);
			if (order_dot >= 0.0f) {
				// Compute the other dot product
				float3 candidate_direction = candidate_point - current_point;
				float candidate_point_length = Length(candidate_direction);
				float distance_dot = Dot(candidate_direction / candidate_point_length, current_center_point_direction);
				// If it is extremely close, consider them to be on the same line
				if (FloatCompare(distance_dot, max_dot)) {
					if (candidate_point_length < max_distance) {
						max_dot = max(max_dot, distance_dot);
						max_distance = candidate_point_length;
						candidate_point_index = subindex;
					}
				}
				else if (distance_dot > max_dot) {
					max_dot = distance_dot;
					max_distance = candidate_point_length;
					candidate_point_index = subindex;
				}
			}
		}

		ECS_ASSERT(candidate_point_index != -1, "ConvexHull could not reorder face using angles");
		current_face_vertex_index = remaining_points[candidate_point_index];
		face.points.Add(current_face_vertex_index);
		remaining_points.RemoveSwapBack(candidate_point_index);
	}
	// The last remaining point can be added directly
	ECS_ASSERT(remaining_points.size == 1, "ConvexHull could not reorder face using angles");
	face.points.Add(remaining_points[0]);
}

void ConvexHull::ReorderFacePoints() {
	// Create a temporary hash table with the vertex connections
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);
	Stream<CapacityStream<unsigned int>> vertex_to_edge_table = ComputeVertexToEdgeTable(&stack_allocator);

	for (unsigned int index = 0; index < faces.size; index++) {
		ReorderFacePointsByEdges(index, vertex_to_edge_table);
	}
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
		float3 cross_product = Cross(second_normal, first_normal);
		float3 edge_direction = GetPoint(edge.point_2) - GetPoint(edge.point_1);
		// Include the 0.0f case, that can happen when the cross poduct is close to 0.0f
		// (for almost parallel normals. This should be a rare occurence, since that would
		// Mean that we have nearly coplanar faces, and these should be merged)
		bool same_direction = Dot(edge_direction, cross_product) >= 0.0f;
		if (!same_direction) {
			std::swap(edge.point_1, edge.point_2);
		}
	}

	//for (unsigned int index = 0; index < edges.size; index++) {
	//	ConvexHullEdge& edge = edges[index];
	//	float3 first_normal = faces[edge.face_1_index].plane.normal;
	//	float3 second_normal = faces[edge.face_2_index].plane.normal;
	//	float3 cross_product = Cross(first_normal, second_normal);
	//	float3 edge_direction = GetPoint(edge.point_2) - GetPoint(edge.point_1);
	//	// Include the 0.0f case, that can happen when the cross poduct is close to 0.0f
	//	// (for almost parallel normals. This should be a rare occurence, since that would
	//	// Mean that we have nearly coplanar faces, and these should be merged)
	//	bool same_direction = Dot(edge_direction, cross_product) >= 0.0f;
	//	if (!same_direction) {
	//		//same_direction = !same_direction;
	//		ECS_ASSERT(false, "Redirecting edges failed!");
	//	}
	//}
}

void ConvexHull::RemoveDegenerateEdges(AllocatorPolymorphic allocator) {
	// Go through the vertices. If there is a single
	// Edge that connects to it, we can remove that edge and that point as well

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);

	// In order to perform this fast, create a parallel array to the vertices
	// Where we hold this extra information
	Stream<CapacityStream<unsigned int>> connections = ComputeVertexToEdgeTable(&stack_allocator);

	for (unsigned int index = 0; index < vertex_size; index++) {
		if (connections[index].size <= 1) {
			// Remove any edges that refer to this point
			// Do this before removing the point
			if (connections[index].size == 1) {
				unsigned int edge_to_be_removed = connections[index][0];
				edges.RemoveSwapBack(edge_to_be_removed);
				unsigned int edge_to_be_restored = edges.size;

				for (unsigned int subindex = 0; subindex < vertex_size; subindex++) {
					for (unsigned int connection_index = 0; connection_index < connections[subindex].size; connection_index++) {
						if (connections[subindex][connection_index] == edge_to_be_removed) {
							connections[subindex].RemoveSwapBack(connection_index);
						}
						else if (connections[subindex][connection_index] == edge_to_be_restored) {
							connections[subindex][connection_index] = edge_to_be_removed;
						}
					}
				}
			}

			// Remove this point
			SoARemoveSwapBack(vertex_size, index, vertices_x, vertices_y, vertices_z);

			// We need to restore the references for the edges that referenced the last point
			unsigned int invalid_referenced_index = vertex_size;
			for (unsigned int subindex = 0; subindex < connections[invalid_referenced_index].size; subindex++) {
				ConvexHullEdge& edge = edges[connections[invalid_referenced_index][subindex]];
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

void ConvexHull::RegenerateEdges(AllocatorPolymorphic allocator)
{
	edges.size = 0;
	// For each face, simply create/update the edge
	for (unsigned int index = 0; index < faces.size; index++) {
		const ConvexHullFace& face = faces[index];
		for (unsigned int face_edge_index = 0; face_edge_index < face.EdgeCount(); face_edge_index++) {
			uint2 point_indices = GetFaceEdgeIndices(index, face_edge_index);
			unsigned int edge_index = FindEdge(point_indices.x, point_indices.y);
			if (edge_index == -1) {
				if (allocator.allocator != nullptr) {
					ReserveEdges(allocator);
				}
				edge_index = AddEdge(point_indices.x, point_indices.y);
				ConvexHullEdge& edge = edges[edge_index];
				edge.face_1_index = index;
			}
			else {
				ConvexHullEdge& edge = edges[edge_index];
				// Assert that the second slot is empty
				//ECS_ASSERT(edge.face_2_index == USHORT_MAX, "ConvexHull regenerating edges impossible because of degenerate faces");
				if (edge.face_2_index == USHORT_MAX) {
					edge.face_2_index = index;
				}
			}
		}
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

bool ConvexHull::Validate() const {
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
	unsigned char* face_reference_count = (unsigned char*)stack_allocator.Allocate(sizeof(unsigned char) * faces.size);
	memset(face_reference_count, 0, sizeof(unsigned char) * faces.size);
	// Verify that each edge has 2 faces
	// Verify that each face is referenced as the number of edges it has
	for (unsigned int index = 0; index < edges.size; index++) {
		if (edges[index].face_1_index == USHORT_MAX || edges[index].face_2_index == USHORT_MAX) {
			return false;
		}
		face_reference_count[edges[index].face_1_index]++;
		face_reference_count[edges[index].face_2_index]++;
	}

	for (unsigned int index = 0; index < faces.size; index++) {
		if (face_reference_count[index] != faces[index].EdgeCount()) {
			uint2 edge_0 = GetFaceEdgeIndices(index, 0);
			uint2 edge_1 = GetFaceEdgeIndices(index, 1);
			uint2 edge_2 = GetFaceEdgeIndices(index, 2);
			const ConvexHullEdge& edge_0_ = edges[FindEdge(edge_0.x, edge_0.y)];
			const ConvexHullEdge& edge_1_ = edges[FindEdge(edge_1.x, edge_1.y)];
			const ConvexHullEdge& edge_2_ = edges[FindEdge(edge_2.x, edge_2.y)];
			return false;
		}
	}

	return true;
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
		else {
			face_2_index = face_2_index;
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
