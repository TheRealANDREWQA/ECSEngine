#include "pch.h"
#include "GiftWrapping.h"

#define MAX_VERTICES 4000
#define MAX_EDGES 25000
#define MAX_FACES 25000

struct ActiveEdge {
	// These are the indices in the points array
	unsigned int point_a_index;
	unsigned int point_b_index;
	unsigned int point_c_index;
	// These are the indices in the triangle array
	unsigned int point_a_mesh_index;
	unsigned int point_b_mesh_index;
};

struct EdgeTablePair {
	ECS_INLINE unsigned int Hash() const {
		// We need a hash that gives different values when a and b are commuted
		// Add some values to a and b such that for the 0 case we don't get many collisions
		return (a_index + 25) * (b_index + 57) * 0x1A3F7281;
	}

	ECS_INLINE bool operator == (EdgeTablePair other) const {
		// The order matters
		return a_index == other.a_index && b_index == other.b_index;
	}

	unsigned int a_index;
	unsigned int b_index;
};

typedef HashTableEmpty<EdgeTablePair, HashFunctionPowerOfTwo> ProcessedEdgeTable;

// Linked list of chunked connections such that we don't need to overallocate
struct VertexConnections {
	// This will add to this connection locally, it doesn't follow the chain
	void Add(unsigned int connecting_vertex, AllocatorPolymorphic allocator) {
		if (entry_count == std::size(entries)) {
			next_connections = (VertexConnections*)Allocate(allocator, sizeof(VertexConnections));
			next_connections->entry_count = 0;
			next_connections->next_connections = nullptr;
			next_connections->entries[next_connections->entry_count++] = connecting_vertex;
		}
		else {
			entries[entry_count++] = connecting_vertex;
		}
	}

	unsigned int entries[7];
	unsigned int entry_count;
	VertexConnections* next_connections;
};

typedef HashTable<VertexConnections, unsigned int, HashFunctionPowerOfTwo> VertexConnectionTable;

// Allocates an entry if there is no such entry, else it returns the existing one
static VertexConnections* GetVertexConnection(VertexConnectionTable* table, unsigned int vertex, AllocatorPolymorphic allocator) {
	VertexConnections* vertex_connection = table->TryGetValuePtr(vertex);
	if (vertex_connection == nullptr) {
		VertexConnections stack_connection;
		stack_connection.entry_count = 0;
		stack_connection.next_connections = nullptr;
		table->InsertDynamic(allocator, stack_connection, vertex);
		vertex_connection = table->GetValuePtr(vertex);
	}
	return vertex_connection;
}

// If the last connections is specified, then it will return the last connection in the chain
// Const variant
template<bool early_exit = false, typename Functor>
static bool ForEachVertexConnection(const VertexConnections* connections, Functor&& functor, const VertexConnections** last_connection = nullptr) {
	if (last_connection != nullptr) {
		*last_connection = connections;
	}
	while (connections != nullptr) {
		if constexpr (early_exit) {
			if (functor(connections)) {
				return true;
			}
		}
		else {
			functor(connections);
		}
		if (last_connection != nullptr) {
			*last_connection = connections;
		}
		connections = connections->next_connections;
	}
	return false;
}

// If the last connections is specified, then it will return the last connection in the chain
template<bool early_exit = false, typename Functor>
static bool ForEachVertexConnection(VertexConnections* connections, Functor&& functor, VertexConnections** last_connection = nullptr) {
	if (last_connection != nullptr) {
		*last_connection = connections;
	}
	while (connections != nullptr) {
		if constexpr (early_exit) {
			if (functor(connections)) {
				return true;
			}
		}
		else {
			functor(connections);
		}
		if (last_connection != nullptr) {
			*last_connection = connections;
		}
		connections = connections->next_connections;
	}
	return false;
}

// Fills in the points that form a triangle with this edge
static void GetTrianglesFromConnections(
	unsigned int first_index,
	unsigned int second_index,
	const VertexConnectionTable* table,
	CapacityStream<unsigned int>* triangle_points
) {
	const VertexConnections* first_connection = table->GetValuePtr(first_index);
	const VertexConnections* second_connection = table->GetValuePtr(second_index);
	ForEachVertexConnection(first_connection, [&](const VertexConnections* first_connection) {
		for (unsigned int index = 0; index < first_connection->entry_count; index++) {
			if (first_connection->entries[index] != second_index) {
				unsigned int current_first_index = first_connection->entries[index];
				ForEachVertexConnection(second_connection, [&](const VertexConnections* second_connection) {
					for (unsigned int subindex = 0; subindex < second_connection->entry_count; subindex++) {
						if (second_connection->entries[subindex] == current_first_index) {
							triangle_points->AddAssert(current_first_index);
						}
					}
				});
			}
		}
	});
}

static VertexConnections* GetLastConnection(VertexConnections* connection) {
	VertexConnections* last_connection = connection;
	while (connection != nullptr) {
		last_connection = connection;
		connection = connection->next_connections;
	}
	return last_connection;
}

static void AddVertexConnectionToLast(VertexConnectionTable* table, unsigned int vertex, unsigned int connection_vertex, AllocatorPolymorphic allocator) {
	VertexConnections* connection = GetVertexConnection(table, vertex, allocator);
	VertexConnections* last_connection = GetLastConnection(connection);
	last_connection->Add(connection_vertex, allocator);
}

// Adds the entry only if it doesn't exist already. The connection needs to be the start
// Of the chain
static bool AddVertexConnectionIfNotExists(VertexConnections* connection, unsigned int connection_vertex, AllocatorPolymorphic allocator) {
	VertexConnections* last_connection;
	bool exists = ForEachVertexConnection<true>(connection, [connection_vertex](VertexConnections* current_connection) {
		for (unsigned int index = 0; index < current_connection->entry_count; index++) {
			if (current_connection->entries[index] == connection_vertex) {
				return true;
			}
		}
		return false;
	}, &last_connection);
	if (!exists) {
		last_connection->Add(connection_vertex, allocator);
	}
	return exists;
}

// This function returns true if there is a point that has edges to the given 2
// Vertices, else returns false. The triangle_third_point is used to exclude that
// Point from consideration. It returns the index of the connection
static unsigned int FindPointThatEnclosesEdge(
	Stream<float3> points,
	VertexConnectionTable* table, 
	unsigned int edge_1, 
	unsigned int edge_2, 
	unsigned int pivot_edge_other_point, 
	AllocatorPolymorphic allocator,
	const ProcessedEdgeTable* processed_edges
) {
	VertexConnections* edge_1_connection = GetVertexConnection(table, edge_1, allocator);
	// Early exit if any of these do not have entries
	if (edge_1_connection->entry_count == 0) {
		return -1;
	}
	VertexConnections* edge_2_connection = GetVertexConnection(table, edge_2, allocator);
	if (edge_2_connection->entry_count == 0) {
		return -1;
	}

	unsigned int final_connection = -1;

	float3 edge_start = points[edge_1];
	float3 edge_end = points[edge_2];
	float3 edge_normalized_direction = Normalize(edge_end - edge_start);
	float3 reference_point = points[pivot_edge_other_point];

	// For each connection in edge 1, check to see if it appears in the edge_2
	bool exists = ForEachVertexConnection<true>(edge_1_connection, [&](VertexConnections* connection) {
		for (unsigned int index = 0; index < connection->entry_count; index++) {
			unsigned int current_connection = connection->entries[index];
			if (current_connection != pivot_edge_other_point) {
				// We need to test to see if this point is on the other side of the edge than
				// The 3rd point, because there can be this case here
				//      A
				//      | \
				//      C - D
				//        /
				//       /
				//      /
				//     /
				//    /
				//   /
				// B
				// We pivoted around AC from the ACD triangle and found B. We need to add ACD,
				// And we see that it belongs to a point that is on the same side of the line AB
				// As the C point. So reject it. We need to have points that are on different sides

				// There is another invalid case
				// Take this example
				//        C
				//         \
				// A        \
				// | \ \     \
				// |  \ \     B
				// |   \  D  /
				// |    \ | /
				// |     \|/
				// F ---- E
				//
				// We are pivoting around BC and we find A. AE already has 2 faces, ADE and AFE.
				// When enclosing the AB edge, we would find the point E as a valid candidate
				// For believing that we are enclosing another triangle, when in reality we are not
				// What we need to do, is to see if the edges AE and AB face only 1 edges associated with them
				// Since AE has 2 faces, it must appear 2 times in the edge table

				// Verify the count first
				unsigned int connection_edge_count_1 = 0;
				unsigned int connection_edge_count_2 = 0;
				
				EdgeTablePair connection_edge = { edge_1, current_connection };
				connection_edge_count_1 += processed_edges->Find(connection_edge) != -1;
				EdgeTablePair reversed_connection_edge = { current_connection, edge_1 };
				connection_edge_count_1 += processed_edges->Find(reversed_connection_edge) != -1;

				if (connection_edge_count_1 > 1) {
					continue;
				}

				EdgeTablePair second_connection_edge = { edge_2, current_connection };
				connection_edge_count_2 += processed_edges->Find(second_connection_edge) != -1;
				EdgeTablePair reversed_second_connection_edge = { current_connection, edge_2 };
				connection_edge_count_2 += processed_edges->Find(reversed_second_connection_edge) != -1;

				if (connection_edge_count_2 == 1) {
					bool is_same_side = PointSameLineHalfPlaneNormalized(edge_start, edge_normalized_direction, reference_point, points[current_connection]);
					float3 projected_point = ProjectPointOnLineDirectionNormalized(edge_start, edge_normalized_direction, points[current_connection]);
					float3 perpendicular_line = points[current_connection] - projected_point;
					perpendicular_line = Normalize(perpendicular_line);
					float3 diff = reference_point - edge_start;
					diff = Normalize(diff);
					float dot = Dot(perpendicular_line, diff);
					bool result = dot > 0.0f;
					
					
					if (!is_same_side) {
						// Check to see if the points are collinear. If they are, reject the point
						// Because of floating point errors, we can get the answer that
						// The points are on different sides of the line even tho they are collinear
						bool are_collinear = IsPointCollinearDirectionByAngle(edge_start, edge_normalized_direction, points[current_connection], 1.0f);
						are_collinear = false;
						if (!are_collinear) {
							bool has_connection = ForEachVertexConnection<true>(edge_2_connection, [&](VertexConnections* second_connection) {
								for (unsigned int subindex = 0; subindex < second_connection->entry_count; subindex++) {
									if (current_connection == second_connection->entries[subindex]) {
										return true;
									}
								}
								return true;
								});
							if (has_connection) {
								final_connection = current_connection;
								return true;
							}
						}
					}
				}
			}
		}
		return false;
	});
	return final_connection;
}

// Calls the functor for each triangle that the vertex is part of
// The functor should have as arguments (uint3 indices)
// The x index is always going to be the vertex
template<bool early_exit = false, typename Functor>
static bool ForEachTriangleForPoint(const VertexConnectionTable* table, unsigned int vertex, Functor&& functor) {
	const VertexConnections* connection = table->TryGetValuePtr(vertex);
	if (connection != nullptr) {
		if (connection->entry_count > 0) {
			// Flatten the connections in a single array
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 8, ECS_MB);
			unsigned int vertex_connection_count = 0;
			ForEachVertexConnection(connection, [&vertex_connection_count](const VertexConnections* connection) {
				vertex_connection_count += connection->entry_count;
				});
			unsigned int* vertex_connections = (unsigned int*)stack_allocator.Allocate(sizeof(unsigned int) * vertex_connection_count);
			unsigned int offset = 0;
			ForEachVertexConnection(connection, [&](const VertexConnections* connection) {
				for (unsigned int index = 0; index < connection->entry_count; index++) {
					vertex_connections[offset + index] = connection->entries[index];
				}
				offset += connection->entry_count;
				});

			// For each entry in the connections, verify if it has a paired connection
			for (unsigned int index = 0; index < vertex_connection_count; index++) {
				unsigned int neighbour = vertex_connections[index];
				const VertexConnections* current_neighbour = table->TryGetValuePtr(neighbour);
				if (current_neighbour != nullptr) {
					if (current_neighbour->entry_count > 0) {
						bool should_early_exit = ForEachVertexConnection<early_exit>(current_neighbour, [&](const VertexConnections* neighbour_connections) {
							for (unsigned int neighbour_index = 0; neighbour_index < neighbour_connections->entry_count; neighbour_index++) {
								// Check to see if it appears in the original vertex connection
								unsigned int neighbour_connection_value = neighbour_connections->entries[neighbour_index];
								for (unsigned int original_connection_index = 0; original_connection_index < vertex_connection_count; original_connection_index++) {
									if (original_connection_index != index && vertex_connections[original_connection_index] == neighbour_connection_value) {
										if constexpr (early_exit) {
											if (functor({ vertex, neighbour, neighbour_connection_value })) {
												return true;
											}
										}
										else {
											functor({ vertex, neighbour, neighbour_connection_value });
										}
									}
								}
							}
							return false;
						});
						if constexpr (early_exit) {
							if (should_early_exit) {
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

// Returns the index of the next edge
// The exclude index is used to exclude a vertex that we already have used to form a triangle for that edge
static size_t FindInitialNextPoint(float3 a, float3 b, size_t a_index, size_t b_index, Stream<float3> points) {
	// The idea is to find a third point, c, which is different from a and b.
	// After that, for all the other points, we calculate the signed tetrahedron volume
	// of the pyramid formed with a, b, c as base and the consideration point d. If the
	// volume is negative, then c is updated to be d. The final value is c

	float3 c;
	size_t c_index;
	if (a_index > 0 && b_index > 0) {
		c = points[0];
		c_index = 0;
	}
	else if (a_index == 0) {
		if (b_index == 1) {
			c = points[2];
			c_index = 2;
		}
		else {
			c = points[1];
			c_index = 1;
		}
	}
	else if (b_index == 0) {
		if (a_index == 1) {
			c = points[2];
			c_index = 2;
		}
		else {
			c = points[1];
			c_index = 1;
		}
	}

	for (size_t index = 0; index < points.size; index++) {
		if (index != a_index && index != b_index) {
			float3 d = points[index];
			float tetrahedron_volume = TetrahedronVolume(a, b, c, d);
			if (tetrahedron_volume < 0.0f) {
				c = d;
				c_index = index;
			}
		}
	}
	return c_index;
}

// We need the processed edges in order to not consider points
// Which already have edges with the current edge, in this case AB
template<typename Functor>
static size_t FindNextPointAroundTriangleEdge(
	unsigned int a_index, 
	unsigned int b_index, 
	unsigned int c_index, 
	Stream<float3> points,
	Stream<unsigned int> indices_to_iterate,
	const VertexConnectionTable* vertex_connection_table,
	Functor&& functor
) {
	// Here, we record the point that forms the largest convex angle
	// With the triangle edge

	// Precalculate the triangle normal, we will need it for all points
	float3 a = points[a_index];
	float3 b = points[b_index];
	float3 abc_normal = TriangleNormal(a, b, points[c_index]);
	// We need to normalize the normal
	abc_normal = Normalize(abc_normal);
	float3 ab = b - a;
	float3 normalized_ab = Normalize(ab);

	// TODO: Remove the candidate array? Or keep it for the future, in case
	// We need it again
	struct Candidate {
		ECS_INLINE bool operator > (Candidate other) const {
			return dot > other.dot || (dot == other.dot && squared_length < other.squared_length);
		}

		ECS_INLINE bool operator >= (Candidate other) const {
			return dot > other.dot || (dot == other.dot && squared_length <= other.squared_length);
		}

		float dot;
		float squared_length;
		unsigned int index;
	};
	ECS_STACK_CAPACITY_STREAM(Candidate, candidates, 64);

	float smallest_angle_dot = -FLT_MAX;

	auto insert_candidate = [&candidates, &smallest_angle_dot](float dot, unsigned int vertex_index, float squared_length) {
		size_t index = 0;
		for (; index < candidates.size; index++) {
			if (candidates[index].dot < dot) {
				break;
			}
		}
		if (index == candidates.size) {
			if (candidates.size < candidates.capacity) {
				candidates.Add({ dot, squared_length, vertex_index });
				smallest_angle_dot = dot;
			}
		}
		else {
			// remove the last entry (if at max capacity) and insert this new one
			if (candidates.size == candidates.capacity) {
				candidates.size--;
			}
			candidates.Insert(index, { dot, squared_length, vertex_index });
			smallest_angle_dot = candidates[candidates.size - 1].dot;
		}
	};

	float collinearity_cosine = cos(DegToRad(1.5f));
	for (size_t index = 0; index < indices_to_iterate.size; index++) {
		unsigned int current_index = indices_to_iterate[index];
		if (current_index != a_index && current_index != b_index && current_index != c_index) {
			// Calculate the normal of this newly formed triangle
			// For example
			//		F	 D
			//		\	/
			// C ---- AB - P
			//		  \
			//		   E 
			// The current edge is AB, which is perpendicular to the screen and not represented
			// The ABC triangle plane is horizontal
			// Consider that the ABC normal will point upwards. We want to detect an angle of
			// ~120 degrees for E, and ~150 for D and ~60 for F
			// We just have to calculate the normal for the respective triangle and do the dot
			// Product. If we have P, we would get an identical normal to that of ABC, and the dot
			// Would be maximal, and that is good, since the angle is 180 degrees. For E, we would
			// Get a positive reduced dot, which is correct, since the angle is still obtuse. For a right
			// Angle we would get 0, and for the case when the angle is sharp, we would get a negative dot
			// Which is decresing towards -1 when the angle would be 0 (i.e. the point is contained on the same
			// Plane as ABC and on the same side of the edge BC). This is the desired behaviour. It works for
			// Both points above and below the ABC plane.

			// PERFORMANCE TODO: SIMD-ize this, since it is quite easy
			
			float3 d = points[current_index];
			// Here, a - d was chosen instead of d - a since this version correctly detects
			// The cases where points lie on the same side of the edge with the other triangle
			// Point and assign negative values for their dot
			float3 abd_normal = Cross(ab, a - d);
			// Normalize this as well to avoid the distance from the edge to influence the decision making
			abd_normal = Normalize(abd_normal);

			// TODO: Impose condition such that when we pivot around an edge that was already added,
			// That the point is on the other side of the edge 
			float current_angle_dot = Dot(abc_normal, abd_normal);
			// We need to include a very small epsilon for points that have the same angle
			// With the edge, but due to small inaccuracies in the calculation they have a smaller
			// Value or equal
			if (current_angle_dot > smallest_angle_dot) {
				// We could perform this test later on, but this can
				// Introduce many invalid points as candidates and the
				// Test is quite cheap
				//bool is_collinear = IsPointCollinearDirectionByCosine(a, normalized_ab, d, collinearity_cosine);
				//if (!is_collinear) {
					insert_candidate(current_angle_dot, current_index, SquaredDistanceToLineNormalized(a, normalized_ab, d));
				//}
				//candidates[0].index = current_index;
				//candidates[0].dot = current_angle_dot;
				//smallest_angle_dot = current_angle_dot;
			}
		}
	}

	//size_t equal_count = 0;
	//for (; equal_count < candidates.size - 1; equal_count++) {
	//	//if (FloatCompare(candidates[equal_count].dot, candidates[equal_count + 1].dot)) {
	//		//break;
	//	//}
	//	if (candidates[equal_count].dot != candidates[equal_count + 1].dot) {
	//		break;
	//	}
	//}
	//InsertionSort<false>(candidates.buffer, equal_count);

	size_t index = 0;
	unsigned int triangle_count = functor.GetFaceCount();
	float3 mesh_center = functor.GetMeshCenter();
	unsigned int edge_count = functor.GetEdgeCount();
	float plane_coplanarity_cosine = cos(DegToRad(10.0f));
	for (; index < candidates.size; index++) {
		// Check for each point if it overlaps any triangles that the
		// Pivot points have. If it overlaps it, then it must be discarded
		float3 point = points[candidates[index].index];
		unsigned int point_functor_index = functor.FindVertex(point);
		PlaneScalar current_point_triangle = ComputePlaneAway(a, b, point, mesh_center);
		unsigned int triangle_index = 0;
		for (; triangle_index < triangle_count; triangle_index++) {
			uint3 triangle_points = functor.GetFacePoints(triangle_index);
			float3 triangle_A = functor.GetPoint(triangle_points.x);
			float3 triangle_B = functor.GetPoint(triangle_points.y);
			float3 triangle_C = functor.GetPoint(triangle_points.z);
			PlaneScalar triangle_plane = functor.GetFacePlane(triangle_index);

			float current_coplanarity_cosine = Dot(current_point_triangle.normal, triangle_plane.normal);

			// Determine if the planes are nearly coplanar, test the triangles
			// Don't use IsParallel or Compare Angle since those return true for
			// Triangles that are facing in the opposite direction as well
			if (current_coplanarity_cosine > plane_coplanarity_cosine) {
				// For convex shapes, we don't have to test the distance between the planes
				// Since nearly coplanar faces can have only very close planes
				// Project one of the triangles onto the other
				float3 a_projected = ProjectPointOnPlane(triangle_plane, a);
				float3 b_projected = ProjectPointOnPlane(triangle_plane, b);
				float3 point_projected = ProjectPointOnPlane(triangle_plane, point);
				if (AreCoplanarTrianglesIntersecting(a_projected, b_projected, point_projected, triangle_A, triangle_B, triangle_C, 0.99f)) {
					break;
					functor.AddOrFindVertex(point);
				}
			}
		}
		//triangle_index = triangle_count;
		if (triangle_index == triangle_count) {
			unsigned int edge_index = 0;
			if (functor.FindVertex(point) == -1) {
				// Test to see if the point lies on an edge
				for (; edge_index < edge_count; edge_index++) {
					uint2 edge_indices = functor.GetEdge(edge_index);
					float3 edge_a = functor.GetPoint(edge_indices.x);
					float3 edge_b = functor.GetPoint(edge_indices.y);
					if (IsPointCollinearByCosine(edge_a, edge_b, point, collinearity_cosine)) {
						if (Dot(edge_b - point, edge_a - point) < 0.0f) {
							break;
						}
					}
				}
			}
			else {
				edge_index = edge_count;
			}
			if (edge_index == edge_count) {
				break;
			}
		}
	}

	return index == candidates.size ? (size_t)-1 : (size_t)candidates[index].index;
	//return candidates[0].index;
}

// At first, I thought this algorithm is going to be easy. For the general case it works
// Reasonably, but for some input types it fails miserably. There are quite some tricky
// Degenerate cases that need to be handled

// The functor must have as functions
// unsigned int AddOrFindVertex(float3 value);
// void AddTriangle(unsigned int triangle_A, unsigned int triangle_B, unsigned int triangle_C);
// unsigned int GetVertexCount() const;
// void SetMeshCenter(float3 center); This function is called before adding any vertex or triangle
// bool ShouldStop() const; The functor can tell the algorithm that it should stop
// float3 GetPoint(unsigned int index) const;
// The vertex positions at the end will be welded together
template<typename Functor>
void GiftWrappingImpl(Stream<float3> vertex_positions, Functor&& functor) {
	ECS_ASSERT(vertex_positions.size < UINT_MAX, "Gift wrapping for meshes with more than 4GB vertices is not available");

	// TODO: Is averaging better than snapping?
	WeldVertices<ECS_WELD_VERTICES_AVERAGE>(vertex_positions, float3::Splat(0.05f), true);
	
	float3 mesh_center = CalculateFloat3Midpoint(vertex_positions);
	functor.SetMeshCenter(mesh_center);

	ResizableLinearAllocator function_allocator{ ECS_MB * 16, ECS_MB * 64, ECS_MALLOC_ALLOCATOR };
	ScopedFreeAllocator scoped_free{ { &function_allocator } };
	Stream<unsigned int> indices_to_iterate;
	indices_to_iterate.Initialize(&function_allocator, vertex_positions.size);
	MakeSequence(indices_to_iterate);

	// Here we are using a stack basically, but here a queue would have been the same
	ResizableStream<ActiveEdge> active_edges;
	// We most likely need a large number of entries
	active_edges.Initialize(&function_allocator, 1000);

	// Find the a point which is extreme in one direction
	// To determine the second point on the convex hull, use the FindInitialNextPoint
	// With a fictional point that is different from the initial extreme
	size_t y_min_index;
	GetFloat3MinY(vertex_positions, &y_min_index);
	float3 y_min_value = vertex_positions[y_min_index];
	float3 virtual_edge_point = y_min_value + float3(1.0f, 0.0f, 0.0f);

	float3 initial_edge_a = y_min_value;
	// For the virtual_point, we can use the same index as y_min, it won't have an impact
	size_t initial_edge_b_index = FindInitialNextPoint(initial_edge_a, virtual_edge_point, y_min_index, y_min_index, vertex_positions);

	// Generate a third point, such that we can start with a triangle and add it to the triangle mesh
	size_t initial_edge_c_index = FindInitialNextPoint(initial_edge_a, vertex_positions[initial_edge_b_index], y_min_index, initial_edge_b_index, vertex_positions);

	// Add the initial triangle
	unsigned int u_y_min_index = (unsigned int)y_min_index;
	unsigned int u_initial_edge_b_index = (unsigned int)initial_edge_b_index;
	unsigned int u_initial_edge_c_index = (unsigned int)initial_edge_c_index;

	unsigned int functor_y_index = functor.AddOrFindVertex(y_min_value);
	unsigned int functor_initial_b_index = functor.AddOrFindVertex(vertex_positions[initial_edge_b_index]);
	unsigned int functor_initial_c_index = functor.AddOrFindVertex(vertex_positions[initial_edge_c_index]);
	functor.AddTriangle(functor_y_index, functor_initial_b_index, functor_initial_c_index);

	active_edges.Add({ u_y_min_index, u_initial_edge_b_index, u_initial_edge_c_index, functor_y_index, functor_initial_b_index });
	active_edges.Add({ u_y_min_index, u_initial_edge_c_index, u_initial_edge_b_index, functor_y_index, functor_initial_c_index });
	active_edges.Add({ u_initial_edge_b_index, u_initial_edge_c_index, u_y_min_index, functor_initial_b_index, functor_initial_c_index });

	// PERFORMANCE TODO: 
	// Improve this guesstimate?
	// Guess that there will be 2 times more edges that MAX_VERTICES
	ProcessedEdgeTable processed_edge_table;
	processed_edge_table.Initialize(&function_allocator, PowerOfTwoGreater(MAX_VERTICES * 2));

	VertexConnectionTable vertex_connection_table;
	vertex_connection_table.Initialize(&function_allocator, PowerOfTwoGreater(MAX_VERTICES * 2));

	// This will only add the pair to the vertex connections
	auto add_edge_to_vertex_connections = [&](EdgeTablePair table_pair) {
		AddVertexConnectionToLast(&vertex_connection_table, table_pair.a_index, table_pair.b_index, &function_allocator);
		AddVertexConnectionToLast(&vertex_connection_table, table_pair.b_index, table_pair.a_index, &function_allocator);
	};

	// This function will add the edge to both the processed edge table and to the
	// Vertex connections table
	auto add_edge_to_both_tables = [&](EdgeTablePair table_pair) {
		add_edge_to_vertex_connections(table_pair);
		processed_edge_table.InsertDynamic(&function_allocator, {}, table_pair);
	};

	add_edge_to_both_tables({ u_y_min_index, u_initial_edge_b_index });
	add_edge_to_both_tables({ u_y_min_index, u_initial_edge_c_index });
	add_edge_to_both_tables({ u_initial_edge_b_index, u_initial_edge_c_index });

	// We need to add the initial_edge_b_index and initial_edge_c_index again to the processed edge table
	// Since this is the first edge that we are pivoting and the algorithm will not catch the fact
	// That it has 2 faces attached after the first iteration
	//processed_edge_table.InsertDynamic(&function_allocator, {}, { u_initial_edge_c_index, u_initial_edge_b_index });

	while (active_edges.size > 0 && functor.GetVertexCount() < MAX_VERTICES && !functor.ShouldStop()) {
		// We can remove this edge since it will be matched at the end
		active_edges.size--;
		ActiveEdge active_edge = active_edges[active_edges.size];
		
		bool remove_degenerates = true;
		unsigned int unew_point_index = -1;
		while (remove_degenerates) {
			remove_degenerates = false;
			// Find the next point for this edge
			size_t new_point_index = FindNextPointAroundTriangleEdge(
				active_edge.point_a_index,
				active_edge.point_b_index,
				active_edge.point_c_index,
				vertex_positions,
				indices_to_iterate,
				&vertex_connection_table,
				functor
			);
			if (new_point_index == -1) {
				return;
			}
			//ECS_ASSERT(new_point_index != -1, "Could not gift wrapp the given mesh");
			unew_point_index = new_point_index;

			// Determine all the collinear points for the newly to be added point
			// If this point is not extreme on this line, we need to disconsider it
			// For that point. We do this to avoid degenerate cases where we get overlapping
			// Triangles
			auto detect_line_degenerate_points = [&](unsigned int edge_A, unsigned int edge_B) {
				float collinearity_cosine = cos(DegToRad(0.5f));
				float3 A = vertex_positions[edge_A];
				float3 edge = vertex_positions[edge_B] - A;
				float edge_length = Length(edge);
				float3 normalized_edge = edge / edge_length;
				float min_T = 0.0f;
				float max_T = edge_length;
				unsigned int min_index = edge_A;
				unsigned int max_index = edge_B;

				ECS_STACK_CAPACITY_STREAM(unsigned int, line_points, ECS_KB * 4);

				for (size_t index = 0; index < indices_to_iterate.size; index++) {
					unsigned int current_index = indices_to_iterate[index];
					if (current_index != edge_A && current_index != edge_B) {
						float3 current_point = vertex_positions[current_index];
						float3 direction = current_point - A;
						float current_length = Length(direction);
						float3 current_normalized_direction = direction / current_length;
						if (IsParallelAngleCosineMask(normalized_edge, current_normalized_direction, collinearity_cosine)) {
							line_points.AddAssert(current_index);
							float t_factor = Dot(normalized_edge, current_normalized_direction) * current_length;

							auto is_support_point = [&]() {
								float3 center_direction = current_point;
								float dot = -FLT_MAX;
								unsigned int dot_index = -1;
								for (unsigned int vertex_index = 0; vertex_index < indices_to_iterate.size; vertex_index++) {
									float current_dot = Dot(center_direction, vertex_positions[indices_to_iterate[vertex_index]]);
									if (current_dot > dot) {
										dot = current_dot;
										dot_index = indices_to_iterate[vertex_index];
									}
								}
								return dot_index == current_index;
							};

							if (t_factor < min_T) {
								if (is_support_point()) {
									min_T = t_factor;
									min_index = current_index;
								}
							}
							if (t_factor > max_T) {
								if (is_support_point()) {
									max_T = t_factor;
									max_index = current_index;
								}
							}
						}
					}
				}

				// Also add edge_B since it can be eliminated as well
				bool was_edge_B_added = functor.FindVertex(vertex_positions[edge_B]) != -1;
				if (!was_edge_B_added) {
					line_points.AddAssert(edge_B);
				}
				for (unsigned int index = 0; index < line_points.size; index++) {
					if (line_points[index] != min_index && line_points[index] != max_index) {
						if (functor.FindVertex(vertex_positions[line_points[index]]) == -1) {
							indices_to_iterate.RemoveSwapBackByValue(line_points[index], "Giftwrapping critical error");
						}
					}
				}
				if (min_index != edge_A) {
					unsigned int functor_a_index = functor.FindVertex(A);
					functor.SetVertex(functor_a_index, vertex_positions[min_index]);
					unsigned int index_to_iterate_a_index = indices_to_iterate.Find(edge_A);
					if (index_to_iterate_a_index != -1) {
						indices_to_iterate.RemoveSwapBack(index_to_iterate_a_index);
					}
				}
				return was_edge_B_added ? edge_B : max_index;
			};

			//unew_point_index = detect_line_degenerate_points(active_edge.point_a_index, unew_point_index);
			//if (unew_point_index != new_point_index) {
			//	remove_degenerates = true;
			//}
			//else {
			//	unew_point_index = detect_line_degenerate_points(active_edge.point_b_index, unew_point_index);
			//	remove_degenerates = unew_point_index != new_point_index;
			//}
		}

		// Removes the active edge
		auto remove_active_edge = [&](unsigned int first_index, unsigned int second_index) {
			for (unsigned int index = 0; index < active_edges.size; index++) {
				const ActiveEdge& edge = active_edges[index];
				bool is_first_second = edge.point_a_index == first_index && edge.point_b_index == second_index;
				bool is_second_first = edge.point_a_index == second_index && edge.point_b_index == first_index;
				if (is_first_second || is_second_first) {
					// Remove it
					active_edges.RemoveSwapBack(index);
					break;
				}
			}
		};

		// Removes the AP and BP edges
		auto remove_active_edges = [&](unsigned int A_index, unsigned int B_index, unsigned int P_index) {
			unsigned int remove_count = 0;
			for (unsigned int index = 0; index < active_edges.size; index++) {
				const ActiveEdge& edge = active_edges[index];
				bool is_AP = edge.point_a_index == A_index && edge.point_b_index == P_index;
				bool is_PA = edge.point_a_index == P_index && edge.point_b_index == A_index;
				bool is_BP = edge.point_a_index == B_index && edge.point_b_index == P_index;
				bool is_PB = edge.point_a_index == P_index && edge.point_b_index == B_index;
				if (is_AP || is_PA || is_BP || is_PB) {
					active_edges.RemoveSwapBack(index);
					remove_count++;
					if (remove_count == 2) {
						// Both edges were removed, no need to continue searching
						break;
					}
					index--;
				}
			}
		};

		// For each edge, we must check both versions (a, b) and (b, a) since the order matters
		// For both cases
		auto add_edge = [&](ActiveEdge active_edge, EdgeTablePair table_edge) {
			// The original edge points are the active edge A and C
			EdgeTablePair edge_reversed = { table_edge.b_index, table_edge.a_index };
			if (processed_edge_table.Find(table_edge) == -1) {
				if (processed_edge_table.Find(edge_reversed) == -1) {
					// There is one more case to detect here. Here is an example
					// A -- B
					// | \     
					// |  \		E
					// C -- D /
					// We pivot around edge DE and find the point B
					// So, the state will become
					// A -- B
					// | \  | \ 
					// |  \	|	E
					// C -- D /
					// BD had no edge entry n the processed edge table, we will add one for the
					// Triangle BDE, but the edge is closed for the triangle ABD as well. So we
					// Don't need to add it to the active edges since it already has 2 faces for it
					// So what we need to do, is to look for a point on the other side of the BD edge
					// Such that it has edges with B and D. To do this, we need a hash table where we
					// Store the edges for each point such that we can query them fast
					unsigned int existing_connection = FindPointThatEnclosesEdge(
						vertex_positions,
						&vertex_connection_table,
						active_edge.point_a_index,
						active_edge.point_b_index,
						active_edge.point_c_index,
						&function_allocator,
						&processed_edge_table
					);
					// We need to add the edge to both tables
					add_edge_to_both_tables(table_edge);
					if (existing_connection == -1) {
						active_edges.Add(active_edge);
					}
					else {
						// We also need to add the reverse edge
						processed_edge_table.InsertDynamic(&function_allocator, {}, edge_reversed);

						// Because we completed a separate triangle, we must perform the
						// Active edge reduction here again
						remove_active_edges(active_edge.point_a_index, active_edge.point_b_index, existing_connection);

						// We also need to notify the functor
						// The AddOrFind should always find the vertex
						functor.AddTriangle(
							active_edge.point_a_mesh_index,
							active_edge.point_b_mesh_index,
							functor.AddOrFindVertex(vertex_positions[existing_connection])
						);
					}
				}
				else {
					// We don't need to add the edge to the vertex connections since the other entry
					// Already did it
					processed_edge_table.InsertDynamic(&function_allocator, {}, table_edge);
					// We need to remove the active edge, if it is still there
					remove_active_edge(table_edge.a_index, table_edge.b_index);
				}
			}
			else if (processed_edge_table.Find(edge_reversed) == -1) {
				// Here we don't need to add to the active edges since we know
				// That there is an entry for this. We don't need to add the edge
				// To the vertex connections since the other edge already did it
				processed_edge_table.InsertDynamic(&function_allocator, {}, edge_reversed);
				// We need to remove the active edge, if it still there
				remove_active_edge(edge_reversed.a_index, edge_reversed.b_index);
			}
		};

		// We need to add the pivot edge into the processed edge table
		EdgeTablePair pivot_edge = { active_edge.point_a_index, active_edge.point_b_index };
		if (processed_edge_table.Find(pivot_edge) != -1) {
			EdgeTablePair reversed_pivot_edge = { active_edge.point_b_index, active_edge.point_a_index };
			ECS_ASSERT(processed_edge_table.Find(reversed_pivot_edge) == -1);
			processed_edge_table.InsertDynamic(&function_allocator, {}, reversed_pivot_edge);
		}
		else {
			processed_edge_table.InsertDynamic(&function_allocator, {}, pivot_edge);
		}

		unsigned int triangle_mesh_index = functor.AddOrFindVertex(vertex_positions[unew_point_index]);
		ActiveEdge first_active_edge = { active_edge.point_a_index, unew_point_index, active_edge.point_b_index, active_edge.point_a_mesh_index, triangle_mesh_index };
		EdgeTablePair first_edge = { active_edge.point_a_index, unew_point_index };
		add_edge(first_active_edge, first_edge);

		ActiveEdge second_active_edge = { active_edge.point_b_index, unew_point_index, active_edge.point_a_index, active_edge.point_b_mesh_index, triangle_mesh_index };
		EdgeTablePair second_edge = { active_edge.point_b_index, unew_point_index };
		add_edge(second_active_edge, second_edge);

		// We can now add a new triangle entry
		functor.AddTriangle(active_edge.point_a_mesh_index, active_edge.point_b_mesh_index, triangle_mesh_index);
	}
}

TriangleMesh GiftWrappingTriangleMesh(Stream<float3> vertex_positions, AllocatorPolymorphic allocator) {
	TriangleMesh triangle_mesh;
	triangle_mesh.Initialize(allocator, MAX_VERTICES, MAX_VERTICES * 2);

	float3 mesh_center;
	struct ImplFunctor {
		unsigned int AddOrFindVertex(float3 position) {
			triangle_mesh->ReservePositions(allocator);
			return triangle_mesh->FindOrAddPoint(position);
		}

		unsigned int FindVertex(float3 position) const {
			return -1;
		}

		void AddTriangle(unsigned int A_index, unsigned int B_index, unsigned int C_index) {
			triangle_mesh->ReserveTriangles(allocator);
			triangle_mesh->AddTriangle({ A_index, B_index, C_index });
		}

		ECS_INLINE unsigned int GetVertexCount() const {
			return triangle_mesh->position_size;
		}

		ECS_INLINE unsigned int GetFaceCount() const {
			return triangle_mesh->triangles.size;
		}

		ECS_INLINE unsigned int GetEdgeCount() const {
			return 0;
		}
		
		ECS_INLINE float3 GetMeshCenter() const {
			return *mesh_center;
		}

		ECS_INLINE float3 GetPoint(unsigned int index) const {
			return triangle_mesh->GetPoint(index);
		}

		ECS_INLINE uint2 GetEdge(unsigned int index) const {
			return { 0, 0 };
		}

		ECS_INLINE uint3 GetFacePoints(unsigned int index) const {
			return triangle_mesh->triangles[index];
		}

		ECS_INLINE PlaneScalar GetFacePlane(unsigned int index) const {
			uint3 face_points = GetFacePoints(index);
			float3 a = GetPoint(face_points.x);
			float3 b = GetPoint(face_points.y);
			float3 c = GetPoint(face_points.z);
			return ComputePlaneAway(a, b, c, *mesh_center);
		}

		ECS_INLINE void SetMeshCenter(float3 center) {
			*mesh_center = center;
		}

		ECS_INLINE bool ShouldStop() const {
			return false;
		}

		ECS_INLINE void SetVertex(unsigned int vertex_index, float3 new_position) {
			triangle_mesh->SetPoint(vertex_index, new_position);
		}

		TriangleMesh* triangle_mesh;
		AllocatorPolymorphic allocator;
		float3* mesh_center;
	};

	GiftWrappingImpl(vertex_positions, ImplFunctor{ &triangle_mesh, allocator, &mesh_center });
	
	// Point all normals away from the center, by using the triangle winding order
	triangle_mesh.RetargetNormals(mesh_center);
	// Resize such that the buffers don't consume unnecessary memory
	triangle_mesh.Resize(allocator, triangle_mesh.position_size, triangle_mesh.triangles.size);

	return triangle_mesh;
}

ConvexHull GiftWrappingConvexHull(Stream<float3> vertex_positions, AllocatorPolymorphic allocator) {
	ConvexHull convex_hull;
	convex_hull.Initialize(allocator, MAX_VERTICES, MAX_VERTICES * 2, MAX_VERTICES);

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_temporary_allocator, ECS_KB * 64, ECS_MB * 16);

	struct ImplFunctor {
		unsigned int AddOrFindVertex(float3 position) {
			convex_hull->ReserveVertices(allocator);
			return convex_hull->AddOrFindVertex(position);
		}

		unsigned int FindVertex(float3 position) const {
			for (unsigned int index = 0; index < convex_hull->vertex_size; index++) {
				if (convex_hull->GetPoint(index) == position) {
					return index;
				}
			}
			return -1;
		}

		void AddTriangle(unsigned int A_index, unsigned int B_index, unsigned int C_index) {
			// Test to see if the points are collinear, using the cross product
			// If the cross product is zero, then we don't need to add this triangle
			float3 A = GetPoint(A_index);
			float3 cross = Cross(GetPoint(B_index) - A, GetPoint(C_index) - A);
			const float ZERO_THRESHOLD = 0.000001f;
			// Use directly the length instead of squared length since
			// We would get into the very small floating point teritory
			if (Length(cross) < ZERO_THRESHOLD) {
				return;
			}

			convex_hull->ReserveEdges(allocator, 2);
			convex_hull->ReserveFaces(allocator);
			unsigned int edge_index = convex_hull->FindEdge(A_index, B_index);
			if (edge_index != -1) {
				convex_hull->AddTriangleToEdge(edge_index, C_index, *mesh_center, temporary_allocator);
			}
			else {
				convex_hull->AddTriangle(A_index, B_index, C_index, *mesh_center, temporary_allocator);
			}
		}

		ECS_INLINE unsigned int GetVertexCount() const {
			return convex_hull->vertex_size;
		}

		ECS_INLINE unsigned int GetFaceCount() const {
			return convex_hull->faces.size;
		}

		ECS_INLINE unsigned int GetEdgeCount() const {
			return convex_hull->edges.size;
		}

		ECS_INLINE float3 GetMeshCenter() const {
			return *mesh_center;
		}

		ECS_INLINE float3 GetPoint(unsigned int index) const {
			return convex_hull->GetPoint(index);
		}

		ECS_INLINE uint3 GetFacePoints(unsigned int index) const {
			const ConvexHullFace& face = convex_hull->faces[index];
			return { face.points[0], face.points[1], face.points[2] };
		}

		ECS_INLINE uint2 GetEdge(unsigned int index) const {
			return { convex_hull->edges[index].point_1, convex_hull->edges[index].point_2 };
		}

		ECS_INLINE PlaneScalar GetFacePlane(unsigned int index) const {
			return convex_hull->faces[index].plane;
		}

		ECS_INLINE void SetMeshCenter(float3 center) {
			*mesh_center = center;
		}

		ECS_INLINE bool ShouldStop() const {
			return convex_hull->edges.size >= MAX_EDGES || convex_hull->faces.size >= MAX_FACES;
		}

		ECS_INLINE void SetVertex(unsigned int vertex_index, float3 new_position) {
			convex_hull->SetPoint(new_position, vertex_index);
		}

		ConvexHull* convex_hull;
		AllocatorPolymorphic allocator;
		AllocatorPolymorphic temporary_allocator;
		float3* mesh_center;
	};
	
	float3 mesh_center = float3::Splat(FLT_MAX);
	GiftWrappingImpl(vertex_positions, ImplFunctor{ &convex_hull, allocator, &stack_temporary_allocator, &mesh_center });

	//bool is_hull_valid = convex_hull.Validate();
	//bool is_hull_valid_merge = convex_hull.Validate();
	//convex_hull.RegenerateEdges(&stack_temporary_allocator);
	//convex_hull.MergeCoplanarFaces2(4.0f, &stack_temporary_allocator);
	//bool is_hull_valid_merge_regenerate = convex_hull.Validate();
	//convex_hull.SimplifyTrianglesAndQuads();
	convex_hull.MergeCoplanarTriangles(0.3f, &stack_temporary_allocator);
	bool is_hull_valid_merge = convex_hull.Validate();

	convex_hull.center = mesh_center;
	//convex_hull.SimplifyTriangles();
	// Resize such that the buffers don't consume unnecessary memory
	convex_hull.Resize(allocator, convex_hull.vertex_size, convex_hull.edges.size, convex_hull.faces.size);
	convex_hull.ReallocateFaces(allocator);
	convex_hull.CalculateAndAssignCenter();
	convex_hull.RedirectEdges();

	return convex_hull;
}