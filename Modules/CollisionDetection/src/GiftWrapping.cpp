#include "pch.h"
#include "GiftWrapping.h"

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
		return (a_index + 25) * b_index;
	}

	ECS_INLINE bool operator == (EdgeTablePair other) const {
		// The order matters
		return a_index == other.a_index && b_index == other.b_index;
	}

	unsigned int a_index;
	unsigned int b_index;
};

// For each edge, we store the triangle indices that make up this face
// We need this for a rejection test
struct EdgeInformation {
	uint3 triangle;
};

typedef HashTable<EdgeInformation, EdgeTablePair, HashFunctionPowerOfTwo> ProcessedEdgeTable;

// TODO: What axis to use here? There is not a definite answer for the time being,
// So pick any of them
typedef SphericalPartitioning<ECS_AXIS_Y, uint3> TrianglePartitioning;

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
	VertexConnections* vertex_connection;
	if (!table->TryGetValuePtr(vertex, vertex_connection)) {
		VertexConnections stack_connection;
		stack_connection.entry_count = 0;
		stack_connection.next_connections = nullptr;
		table->InsertDynamic(allocator, stack_connection, vertex);
		vertex_connection = table->GetValuePtr(vertex);
	}
	return vertex_connection;
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
	AllocatorPolymorphic allocator
) {
	VertexConnections* edge_1_connection = GetVertexConnection(table, edge_1, allocator);
	VertexConnections* edge_2_connection = GetVertexConnection(table, edge_2, allocator);
	unsigned int final_connection = -1;

	float3 edge_normalized_direction = Normalize(points[edge_2] - points[edge_1]);
	float3 edge_start = points[edge_1];
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
				bool is_same_side = PointSameLineHalfPlaneNormalized(edge_start, edge_normalized_direction, reference_point, points[current_connection]);
				if (!is_same_side) {
					bool exists_edge_2 = ForEachVertexConnection<true>(edge_2_connection, [&](VertexConnections* second_connection) {
						for (unsigned int subindex = 0; subindex < second_connection->entry_count; subindex++) {
							if (current_connection == second_connection->entries[subindex]) {
								return true;
							}
						}
						return false;
						});
					if (exists_edge_2) {
						final_connection = current_connection;
						return true;
					}
				}
			}
		}
		return false;
	});
	return final_connection;
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
static size_t FindNextPointAroundTriangleEdge(
	size_t a_index, 
	size_t b_index, 
	size_t c_index, 
	Stream<float3> points
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

	float angle_dot = -FLT_MAX;
	size_t point_index = -1;
	for (size_t index = 0; index < points.size; index++) {
		if (index != a_index && index != b_index && index != c_index) {
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

			
			float3 d = points[index];
			// Here, a - d was chosen instead of d - a since this version correctly detects
			// The cases where points lie on the same side of the edge with the other triangle
			// Point and assign negative values for their dot
			float3 abd_normal = Cross(ab, a - d);
			// Normalize this as well to avoid the distance from the edge to influence the decision making
			abd_normal = Normalize(abd_normal);

			float current_angle_dot = Dot(abc_normal, abd_normal);
			// We need to include a very small epsilon for points that have the same angle
			// With the edge, but due to small inaccuracies in the calculation they have a smaller
			// Value or equal
			if (current_angle_dot > angle_dot || (angle_dot - current_angle_dot) < 0.0000001f) {
				// Test for some degenerate cases where collinear points are chosen
				// We need quite a significant epsilon value
				bool is_collinear = IsPointCollinear(a, b, d, 0.0015f);
				if (!is_collinear) {
					// We need to use max in case this value is slighly less. We want to keep
					// The greater value as representative for the angle
					angle_dot = std::max(angle_dot, current_angle_dot);
					point_index = index;
				}
			}
		}
	}

	return point_index;
}

TriangleMesh GiftWrapping(Stream<float3> vertex_positions, AllocatorPolymorphic allocator) {
	// At first, I thought this algorithm is going to be easy. For the general case it works
	// Reasonably, but for some input types it fails miserably. There are quite some tricky
	// Degenerate cases that need to be handled

	ECS_ASSERT(vertex_positions.size < UINT_MAX, "Gift wrapping for meshes with more than 4GB vertices is not available");

	const size_t MAX_VERTICES = 5000;

	WeldVertices(vertex_positions, float3::Splat(0.15f), true);

	TriangleMesh triangle_mesh;
	triangle_mesh.Initialize(allocator, MAX_VERTICES, MAX_VERTICES * 2);

	ResizableLinearAllocator function_allocator{ ECS_MB * 16, ECS_MB * 64, {nullptr} };
	ScopedFreeAllocator scoped_free{ { &function_allocator } };

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

	unsigned int u_y_min_index = (unsigned int)y_min_index;
	unsigned int u_initial_edge_b_index = (unsigned int)initial_edge_b_index;
	unsigned int u_initial_edge_c_index = (unsigned int)initial_edge_c_index;
	active_edges.Add({ u_y_min_index, u_initial_edge_b_index, u_initial_edge_c_index, 0, 1 });
	active_edges.Add({ u_y_min_index, u_initial_edge_c_index, u_initial_edge_b_index, 0, 2 });
	active_edges.Add({ u_initial_edge_b_index, u_initial_edge_c_index, u_y_min_index, 1, 2 });

	// Add the triangle to the mesh
	triangle_mesh.AddTriangleWithPoints(initial_edge_a, vertex_positions[initial_edge_b_index], vertex_positions[initial_edge_c_index]);

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
	auto add_edge_to_both_tables = [&](EdgeInformation edge_information, EdgeTablePair table_pair) {
		add_edge_to_vertex_connections(table_pair);
		processed_edge_table.InsertDynamic(&function_allocator, edge_information, table_pair);
	};

	EdgeInformation initial_edge_information = { {u_y_min_index, u_initial_edge_b_index, u_initial_edge_c_index} };
	add_edge_to_both_tables(initial_edge_information, { u_y_min_index, u_initial_edge_b_index });
	add_edge_to_both_tables(initial_edge_information, { u_y_min_index, u_initial_edge_c_index });
	add_edge_to_both_tables(initial_edge_information, { u_initial_edge_b_index, u_initial_edge_c_index });

	// We need to keep track of some other information, and that is the normal for each triangle
	// With this normal, we can detect the cases when a triangle would overlap another existing one
	// We can accelerate this type of query by checking the neighbourhood of the normal. Since for
	// Convex polyhedra we cannot have 2 identical normals. This will also help with collapsing
	// Triangles into faces later on
	// We can use Spherical/Direction Partitioning from the engine side to accelerate this
	
	TrianglePartitioning triangle_partitioning;
	// Use a large partition size, such that we prune as much as possible
	triangle_partitioning.Initialize(&function_allocator, ECS_KB * 32, 8, 1);

	// Calculate the mesh center, such that we can use it to retarget normals away
	// From the center
	float3 mesh_center = CalculateFloat3Midpoint(vertex_positions);

	// The indices must be those from the vertex positions
	auto add_triangle_normal = [&](unsigned int triangle_a, unsigned int triangle_b, unsigned int triangle_c) {
		float3 normal = TriangleNormalNormalized(vertex_positions[triangle_a], vertex_positions[triangle_b], vertex_positions[triangle_c]);
		// Point the normal away from the center
		if (IsTriangleFacing(normal, vertex_positions[triangle_a], mesh_center)) {
			normal = -normal;
			// Invert the point order as well to reflect this orientation
			std::swap(triangle_b, triangle_c);
		}
		triangle_partitioning.Add(normal, { triangle_a, triangle_b, triangle_c });
	};
	add_triangle_normal(u_y_min_index, u_initial_edge_b_index, u_initial_edge_c_index);

	while (active_edges.size > 0 && triangle_mesh.position_size < MAX_VERTICES) {
		// We can remove this edge since it will be matched at the end
		active_edges.size--;
		ActiveEdge active_edge = active_edges[active_edges.size];

		// Find the next point for this edge
		size_t new_point_index = FindNextPointAroundTriangleEdge(
			active_edge.point_a_index,
			active_edge.point_b_index,
			active_edge.point_c_index,
			vertex_positions
		);
		if (new_point_index == -1) {
			break;
		}
		ECS_ASSERT(new_point_index != -1, "Could not gift wrapp the given mesh");
		unsigned int unew_point_index = new_point_index;

		// For each edge, we must check both versions (a, b) and (b, a) since the order matters
		// For both cases
		auto add_edge = [&](ActiveEdge active_edge, EdgeTablePair table_edge) {
			// The original edge points are the active edge A and C
			EdgeTablePair edge_reversed = { table_edge.b_index, table_edge.a_index };
			if (processed_edge_table.Find(table_edge) == -1) {
				EdgeInformation edge_information;
				edge_information.triangle = { active_edge.point_a_index, active_edge.point_c_index, unew_point_index };
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
						&function_allocator
					);
					if (existing_connection == -1) {
						active_edges.Add(active_edge);
						// We need to add the edge to both tables
						add_edge_to_both_tables(edge_information, table_edge);
					}
					else {
						// We need to add the edge to both tables
						add_edge_to_both_tables(edge_information, table_edge);
						// We also need to add the reverse edge
						edge_information.triangle.x = active_edge.point_a_index;
						edge_information.triangle.y = active_edge.point_b_index;
						edge_information.triangle.z = existing_connection;
						processed_edge_table.InsertDynamic(&function_allocator, edge_information, edge_reversed);
						
						// And there is one more thing to do
						// In case the AB or AD edge is in the active edges, it needs to be removed
						// Since we found a match for it
						for (unsigned int subindex = 0; subindex < active_edges.size; subindex++) {
							bool is_AB = active_edges[subindex].point_a_index == active_edge.point_a_index &&
								active_edges[subindex].point_b_index == existing_connection;
							bool is_BA = active_edges[subindex].point_a_index == existing_connection &&
								active_edges[subindex].point_b_index == active_edge.point_a_index;
							bool is_AD = active_edges[subindex].point_a_index == active_edge.point_b_index &&
								active_edges[subindex].point_b_index == existing_connection;
							bool is_DA = active_edges[subindex].point_a_index == existing_connection &&
								active_edges[subindex].point_b_index == active_edge.point_b_index;
							if (is_AB || is_BA || is_AD || is_DA) {
								// Remove it
								active_edges.RemoveSwapBack(subindex);
								subindex--;
							}
						}
					}
				}
				else {
					// We don't need to add the edge to the vertex connections since the other entry
					// Already did it
					processed_edge_table.InsertDynamic(&function_allocator, edge_information, table_edge);
				}
			}
			else if (processed_edge_table.Find(edge_reversed) == -1) {
				// Here we don't need to add to the active edges since we know
				// That there is an entry for this. We don't need to add the edge
				// To the vertex connections since the other edge already did it
				EdgeInformation edge_information;
				edge_information.triangle = { active_edge.point_a_index, active_edge.point_c_index, unew_point_index };
				processed_edge_table.InsertDynamic(&function_allocator, edge_information, edge_reversed);
			}
		};

		triangle_mesh.ReservePositions(allocator);
		unsigned int triangle_mesh_index = triangle_mesh.FindOrAddPoint(vertex_positions[new_point_index]);
		ActiveEdge first_active_edge = { active_edge.point_a_index, unew_point_index, active_edge.point_b_index, active_edge.point_a_mesh_index, triangle_mesh_index };
		EdgeTablePair first_edge = { active_edge.point_a_index, unew_point_index };
		add_edge(first_active_edge, first_edge);
 
		ActiveEdge second_active_edge = { active_edge.point_b_index, unew_point_index, active_edge.point_a_index, active_edge.point_b_mesh_index, triangle_mesh_index };
		EdgeTablePair second_edge = { active_edge.point_b_index, unew_point_index };
		add_edge(second_active_edge, second_edge);

		// We can now add a new triangle entry to the triangle mesh
		triangle_mesh.ReserveTriangles(allocator);
		triangle_mesh.AddTriangle({ active_edge.point_a_mesh_index, active_edge.point_b_mesh_index, triangle_mesh_index });
		// We need to add the triangle normal for all iterations
		add_triangle_normal(active_edge.point_a_index, active_edge.point_b_index, unew_point_index);
	}
	// Point all normals away from the center, by using the triangle winding order
	triangle_mesh.RetargetNormals(mesh_center);
	// Resize such that the buffers don't consume unnecessary memory
	triangle_mesh.Resize(allocator, triangle_mesh.position_size, triangle_mesh.triangles.size);

	return triangle_mesh;
}