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

	bool operator == (EdgeTablePair other) const {
		// The order matters
		return a_index == other.a_index && b_index == other.b_index;
	}

	unsigned int a_index;
	unsigned int b_index;
};

struct EmptyStruct {};

typedef HashTable<EmptyStruct, EdgeTablePair, HashFunctionPowerOfTwo> ProcessedEdgeTable;

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
	float3 abc_normal = TriangleNormal(a, points[b_index], points[c_index]);
	// We need to normalize the normal
	abc_normal = Normalize(abc_normal);
	float3 ab = points[b_index] - a;

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
			if (current_angle_dot > angle_dot) {
				angle_dot = current_angle_dot;
				point_index = index;
			}
		}
	}

	return point_index;
}

TriangleMesh GiftWrapping(Stream<float3> vertex_positions, AllocatorPolymorphic allocator) {
	ECS_ASSERT(vertex_positions.size < UINT_MAX, "Gift wrapping for meshes with more than 4GB vertices is not available");

	Timer timer;
	WeldVertices(vertex_positions, float3::Splat(0.125f));
	float float_duration = timer.GetDuration(ECS_TIMER_DURATION_MS);
	ECS_FORMAT_TEMP_STRING(poggers, "Duration: {#}", float_duration);
	OutputDebugStringA(poggers.buffer);

	//auto compare_mask = [](float3 a, float3 b) {
	//	float3 absolute_difference = BasicTypeAbsoluteDifference(a, b);
	//	return BasicTypeLessEqual(absolute_difference, float3::Splat(0.055f));
	//};

	//for (size_t index = 0; index < vertex_positions.size; index++) {
	//	for (size_t subindex = index + 1; subindex < vertex_positions.size; subindex++) {
	//		if (compare_mask(vertex_positions[index], vertex_positions[subindex])) {
	//			// We can remove the subindex point
	//			vertex_positions.RemoveSwapBack(subindex);
	//			subindex--;
	//		}
	//	}
	//}

	// TODO: Decide a better way of initializing the space for this triangle mesh
	// Guesstimate
	TriangleMesh triangle_mesh;
	triangle_mesh.Initialize(allocator, vertex_positions.size, vertex_positions.size);

	// Here we are using a stack basically, but here a queue would have been the same
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);
	ResizableStream<ActiveEdge> active_edges;
	active_edges.Initialize(&stack_allocator, 32);
	
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

	active_edges.Add({ (unsigned int)y_min_index, (unsigned int)initial_edge_b_index, (unsigned int)initial_edge_c_index, 0, 1 });
	active_edges.Add({ (unsigned int)y_min_index, (unsigned int)initial_edge_c_index, (unsigned int)initial_edge_b_index, 0, 2 });
	active_edges.Add({ (unsigned int)initial_edge_b_index, (unsigned int)initial_edge_c_index, (unsigned int)y_min_index, 1, 2 });

	// Add the triangle to the mesh
	triangle_mesh.AddTriangleWithPoints(initial_edge_a, vertex_positions[initial_edge_b_index], vertex_positions[initial_edge_c_index]);

	// TODO: 
	// Improve this guesstimate?
	// Guess that there will be half of the vertex positions as edges
	ProcessedEdgeTable processed_edge_table;
	processed_edge_table.Initialize(&stack_allocator, PowerOfTwoGreater(vertex_positions.size / 2));
	processed_edge_table.Insert({}, { (unsigned int)y_min_index, (unsigned int)initial_edge_b_index });
	processed_edge_table.Insert({}, { (unsigned int)y_min_index, (unsigned int)initial_edge_c_index });
	processed_edge_table.Insert({}, { (unsigned int)initial_edge_b_index, (unsigned int)initial_edge_c_index });

	while (active_edges.size > 0) {
		if (triangle_mesh.triangles.size > 50000) {
			break;
		}

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
			EdgeTablePair edge_reversed = { table_edge.b_index, table_edge.a_index };
			if (processed_edge_table.Find(table_edge) == -1) {
				if (processed_edge_table.Find(edge_reversed) == -1) {
					active_edges.Add(active_edge);
				}
				processed_edge_table.InsertDynamic(&stack_allocator, {}, table_edge);
			}
			else if (processed_edge_table.Find(edge_reversed) == -1) {
				// Here we don't need to add to the active edges since we know
				// That exists an entry for this
				processed_edge_table.InsertDynamic(&stack_allocator, {}, edge_reversed);
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
	}

	// Calculate the triangle mesh center
	float3 mesh_center = CalculateFloat3Midpoint(triangle_mesh.position_x, triangle_mesh.position_y, triangle_mesh.position_z, triangle_mesh.position_size);
	// Point all normals away from the center, by using the triangle winding order
	triangle_mesh.RetargetNormals(mesh_center);

	return triangle_mesh;
}