#include "pch.h"
#include "ContactManifolds.h"

ContactManifold ComputeContactManifold(const ConvexHull* first_hull, const ConvexHull* second_hull, SATQuery query) {
	ContactManifold contact_manifold;
	
	// Start by branching on the edge vs face case
	if (query.type == SAT_QUERY_FACE) {
		// Determine the incident face for this reference face
		const ConvexHull* reference_hull = query.face.first_collider ? first_hull : second_hull;
		const ConvexHull* incident_hull = query.face.first_collider ? second_hull : first_hull;
		
		// The incident face is simply the support face along the negated reference face normal
		PlaneScalar reference_plane = reference_hull->faces[query.face.face_index].plane;
		float3 reference_face_normal = reference_plane.normal;
		unsigned int incident_face_index = incident_hull->SupportFace(-reference_face_normal);

		// Clip the incident face against the reference face
		ECS_STACK_CAPACITY_STREAM(float3, clipped_points, 64);
		reference_hull->ClipFace(query.face.face_index, incident_hull, incident_face_index, &clipped_points);

		// Discard point above the reference face
		for (unsigned int index = 0; index < clipped_points.size; index++) {
			if (IsAbovePlaneMask(reference_plane, clipped_points[index])) {
				clipped_points.RemoveSwapBack(index);
				index--;
			}
		}

		// Project points below the plane on the reference plane
		for (unsigned int index = 0; index < clipped_points.size; index++) {
			clipped_points[index] = ProjectPointOnPlane(reference_plane, clipped_points[index]);
		}

		// If we have more than 4 points, we need to simply the manifold
		if (clipped_points.size > 4) {
			clipped_points.size = SimplifyContactManifoldPoints(clipped_points);
		}

		// We can write the manifold now
		contact_manifold.WriteContactPoints(clipped_points);
		// The separation axis is the reference face axis
		contact_manifold.separation_axis = reference_face_normal;
		contact_manifold.separation_distance = query.face.distance;
	}
	else if (query.type == SAT_QUERY_EDGE) {
		// For this case, compute the closest points between the 2 lines,
		// And the manifold will contain a single point at the midpoint
		// Of these 2 points. The separating axis is the cross product
		// Between the 2 lines, with the caveat that it is pointing
		// Away from the first hull
	}
	else {
		// Don't do anyhting here
	}
	
	return contact_manifold;
}

size_t SimplifyContactManifoldPoints(Stream<float3> points) {

}