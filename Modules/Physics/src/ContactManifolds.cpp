#include "pch.h"
#include "ContactManifolds.h"
#include "ECSEngineWorld.h"

ContactManifoldFeatures ComputeContactManifold(
	World* world,
	const ConvexHull* first_hull, 
	const ConvexHull* second_hull, 
	SATQuery& query
) {
	ContactManifoldFeatures contact_manifold;

	// Start by branching on the edge vs face case
	if (query.type == SAT_QUERY_FACE) {
		// Determine the incident face for this reference face
		const ConvexHull* reference_hull = first_hull;
		const ConvexHull* incident_hull = second_hull;
		if (!query.face.first_collider) {
			swap(reference_hull, incident_hull);
		}

		unsigned int reference_face_index = query.face.face_index;

		// The incident face is simply the support face along the negated reference face normal
		PlaneScalar reference_plane = reference_hull->faces[reference_face_index].plane;
		float3 reference_face_normal = reference_plane.normal;
		unsigned int incident_face_index = incident_hull->SupportFace(-reference_face_normal);

		// Choose a higher epsilon value, since points that are too close don't help with the manifold stability
		// And confuse the simplifier
		ConvexHullClipFaceOptions clip_options = { false, float3::Splat(0.005f) };

		// Clip the incident face against the reference face
		ECS_STACK_CAPACITY_STREAM(ConvexHullClippedPoint, clipped_points, 64);
		reference_hull->ClipFace(reference_face_index, incident_hull, incident_face_index, &clipped_points, &clip_options);

		// If there are no contact points are found, invert the faces
		// And retry the process
		if (clipped_points.size == 0) {
			// Flip the query order
			query.face.first_collider = !query.face.first_collider;
			// Swap all the necessary local variables
			swap(incident_hull, reference_hull);
			swap(incident_face_index, reference_face_index);
			reference_plane = reference_hull->faces[reference_face_index].plane;
			reference_face_normal = reference_plane.normal;

			// Retry again with the swapped variables
			reference_hull->ClipFace(reference_face_index, incident_hull, incident_face_index, &clipped_points, &clip_options);
			ECS_CRASH_CONDITION(clipped_points.size > 0, "Creating contact manifold from faces failed! No points could be obtained!");
		}

		// Discard point above the reference face
		for (unsigned int index = 0; index < clipped_points.size; index++) {
			if (IsAbovePlaneMask(reference_plane, clipped_points[index].position)) {
				clipped_points.RemoveSwapBack(index);
				index--;
			}
		}

		// Project points below the plane on the reference plane
		for (unsigned int index = 0; index < clipped_points.size; index++) {
			clipped_points[index].position = ProjectPointOnPlane(reference_plane, clipped_points[index].position);
		}

		// This function will check internally that there are at least 5 points
		// To proceed with the simplification
		clipped_points.size = SimplifyContactManifoldPoints(world, clipped_points, reference_face_normal);

		// We can write the manifold now
		unsigned int write_index = ContactManifoldWritePoints(contact_manifold, clipped_points);
		// Complete the manifold information
		for (unsigned int index = 0; index < clipped_points.size; index++) {
			contact_manifold.point_indices[write_index + index] = clipped_points[index].point_index;
			contact_manifold.point_edge_indices[write_index + index] = clipped_points[index].incident_edge_index;
		}

		// The separation axis is the reference face axis
		contact_manifold.separation_axis = reference_face_normal;
		contact_manifold.separation_distance = query.face.distance;
		contact_manifold.feature_index_A = reference_face_index;
		contact_manifold.feature_index_B = incident_face_index;
		contact_manifold.is_face_contact = true;
	}
	else if (query.type == SAT_QUERY_EDGE) {
		// For this case, compute the closest points between the 2 lines,
		// And the manifold will contain a single point at the midpoint
		// Of these 2 points. The separating axis is the cross product
		// Between the 2 lines, with the caveat that it is pointing
		// Away from the first hull

		Line3D first_edge = first_hull->GetEdgePoints(query.edge.edge_1_index);
		Line3D second_edge = second_hull->GetEdgePoints(query.edge.edge_2_index);
		float3 first_closest_point;
		float3 second_closest_point;
		ClosestSegmentPoints(first_edge.A, first_edge.B, second_edge.A, second_edge.B, &first_closest_point, &second_closest_point);
		float3 midpoint = (first_closest_point + second_closest_point) * 0.5f;

		// Set the point index and edge indices as -1, 
		// since there is no "physical" point for the edge case
		ContactManifoldFeaturesAddPoint(contact_manifold, midpoint, -1, { (unsigned int)-1, (unsigned int)-1 });
		contact_manifold.separation_axis = Normalize(query.edge.separation_axis);
		contact_manifold.separation_distance = query.edge.distance;
		contact_manifold.feature_index_A = query.edge.edge_1_index;
		contact_manifold.feature_index_B = query.edge.edge_2_index;
		contact_manifold.is_face_contact = false;
	}
	else {
		// Don't do anything here
	}

	return contact_manifold;
}

size_t SimplifyContactManifoldPoints(World* world, Stream<float3> points, float3 plane_normal) {
	// For less than 5 points, don't do anything
	if (points.size < 5) {
		return points.size;
	}

	// Simplify
	// Choose a vector and compute the support point alongside that direction
	float3 initial_direction = float3{ 1.0f, 0.0f, 0.0f };
	size_t initial_point_index = ComputeSupportPointScalar(points, initial_direction);

	// For the 2nd point we have 2 options: compute the support point
	// In the opposite direction, or compute the point with the largest
	// Distance to the initial one. The second option is choosen since it
	// Is a little bit more accurate
	size_t second_point_index = ComputeFarthestPointFromScalar(points, points[initial_point_index]);

	// PERFORMANCE TODO: Copy the points to a temporary stack buffer
	// Such that we don't need to check with an if that it is different
	// From the first points?

	// For the third point, we need to maximize the area of the triangle formed
	// With the 2 known points
	float3 first_point = points[initial_point_index];
	float3 second_point = points[second_point_index];

	auto compute_area = [plane_normal](float3 first_point, float3 second_point, float3 current_point) {
		return Dot(plane_normal, Cross(first_point - current_point, second_point - current_point));
	};

	float triangle_area = 0.0f;
	size_t third_point_index = -1;
	for (size_t index = 0; index < points.size; index++) {
		if (index != initial_point_index && index != second_point_index) {
			// We can approximate the area with the cross product of the edges
			// Starting from the third point with the dot product with the face normal
			// This gives us a signed area that we can use to distinguish between CCW
			// and CW triangles, and we need one CCW and one CW triangle
			float3 current_point = points[index];
			float current_triangle_area = compute_area(first_point, second_point, current_point);
			if (current_triangle_area > triangle_area) {
				triangle_area = current_triangle_area;
				third_point_index = index;
			}
		}
	}
	if (third_point_index == -1) {
		// There was no triangle in that order
		// There must be in the order
		for (size_t index = 0; index < points.size; index++) {
			if (index != initial_point_index && index != second_point_index) {
				float3 current_point = points[index];
				float current_triangle_area = compute_area(first_point, second_point, current_point);
				if (current_triangle_area < triangle_area) {
					triangle_area = current_triangle_area;
					third_point_index = index;
				}
			}
		}
	}

	// For the 4th point, compute the point that adds the most negative area to the existing
	// Triangle. To do this, compute the area for each edge in turn. Do not take into account
	// Positive areas, only negative ones
	float fourth_point_negative_area = 0.0f;
	size_t fourth_point_index = -1;
	float3 third_point = points[third_point_index];
	for (size_t index = 0; index < points.size; index++) {
		if (index != initial_point_index && index != second_point_index && index != third_point_index) {
			float3 current_point = points[index];
			float current_negative_area = 0.0f;
			
			float first_edge_area = compute_area(first_point, second_point, current_point);
			if (first_edge_area < 0.0f) {
				current_negative_area += first_edge_area;
			}

			float second_edge_area = compute_area(first_point, third_point, current_point);
			if (second_edge_area < 0.0f) {
				current_negative_area += second_edge_area;
			}

			float third_edge_area = compute_area(second_point, third_point, current_point);
			if (third_edge_area < 0.0f) {
				current_negative_area += third_edge_area;
			}

			if (current_negative_area < fourth_point_negative_area) {
				fourth_point_negative_area = current_negative_area;
				fourth_point_index = index;
			}
		}
	}

	// It can happen that the fourth point is not found because there are manifold points
	// That are close together and they fail to produce a fourth point
	if (fourth_point_index == -1) {
		points[0] = first_point;
		points[1] = second_point;
		points[2] = third_point;
		return 3;
	}

	float3 fourth_point = points[fourth_point_index];
	points[0] = first_point;
	points[1] = second_point;
	points[2] = third_point;
	points[3] = fourth_point;
	return 4;
}

size_t SimplifyContactManifoldPoints(World* world, Stream<ConvexHullClippedPoint> points, float3 plane_normal) {
	// Use the normal float3 version and match the results at the end
	if (points.size < 4) {
		return points.size;
	}

	ECS_STACK_CAPACITY_STREAM(float3, position_points, 32);
	ECS_ASSERT(position_points.capacity >= points.size, "Too many points to simplify for contact manifold!");
	for (size_t index = 0; index < points.size; index++) {
		position_points.Add(points[index].position);
	}

	size_t count = SimplifyContactManifoldPoints(world, position_points, plane_normal);
	for (size_t index = 0; index < count; index++) {
		size_t subindex = index;
		for (; subindex < points.size; subindex++) {
			if (position_points[index] == points[subindex].position) {
				break;
			}
		}

		points.Swap(index, subindex);
	}
	return count;
}

void ContactManifoldFeatures::RemoveSwapBack(unsigned int index)
{
	point_count--;
	if (index != point_count) {
		points[index] = points[point_count];
		point_indices[index] = point_indices[point_count];
		point_edge_indices[index] = point_edge_indices[point_count];
	}
}
