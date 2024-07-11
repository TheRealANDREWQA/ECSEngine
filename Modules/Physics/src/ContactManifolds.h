#pragma once
#include "ECSEngineMath.h"
#include "Export.h"
#include "CollisionDetection/src/SAT.h"

namespace ECSEngine {
	struct World;
}

struct ConvexHull;

struct ContactManifold {
	// This axis needs to be normalized
	float3 separation_axis;
	float separation_distance;
	unsigned int point_count = 0;
	float3 points[4];

	// These describe the features that produced this contact manifold
	unsigned int feature_index_A;
	unsigned int feature_index_B;
	bool is_face_contact;
};

// This is a contact manifold that also has the features 
// that generated each point
struct PHYSICS_API ContactManifoldFeatures : ContactManifold {
	void RemoveSwapBack(unsigned int index);

	unsigned int point_indices[ECS_COUNTOF(ContactManifold::points)];
	uint2 point_edge_indices[ECS_COUNTOF(ContactManifold::points)];
};


// Returns the index of the added point
ECS_INLINE unsigned int ContactManifoldAddPoint(ContactManifold& manifold, float3 point) {
	ECS_ASSERT(manifold.point_count < ECS_COUNTOF(ContactManifold::points), "ContactManifold too many contact points!");
	manifold.points[manifold.point_count++] = point;
	return manifold.point_count - 1;
}

ECS_INLINE unsigned int ContactManifoldFeaturesAddPoint(ContactManifoldFeatures& manifold, float3 point, unsigned int point_index, uint2 edge_indices) {
	unsigned int index = ContactManifoldAddPoint(manifold, point);
	manifold.point_indices[index] = point_index;
	manifold.point_edge_indices[index] = edge_indices;
	return index;
}

// Returns the index of the first added point
ECS_INLINE unsigned int ContactManifoldWritePoints(ContactManifold& manifold, Stream<float3> write_points) {
	ECS_ASSERT(manifold.point_count + write_points.size <= ECS_COUNTOF(ContactManifold::points), "ContactManifold too many contact points!");
	write_points.CopyTo(manifold.points + manifold.point_count);
	manifold.point_count += write_points.size;
	return manifold.point_count - write_points.size;
}

// Returns the index of the first added point
ECS_INLINE unsigned int ContactManifoldWritePoints(ContactManifold& manifold, Stream<ConvexHullClippedPoint> write_points) {
	ECS_ASSERT(manifold.point_count + write_points.size <= ECS_COUNTOF(ContactManifold::points), "ContactManifold too many contact points!");
	for (size_t index = 0; index < write_points.size; index++) {
		manifold.points[manifold.point_count + index] = write_points[index].position;
	}
	manifold.point_count += write_points.size;
	return manifold.point_count - write_points.size;
}

ECS_INLINE PlaneScalar ContactManifoldGetPlane(const ContactManifold& manifold) {
	// The manifold must have at least one point
	return PlaneScalar::FromNormalized(manifold.separation_axis, manifold.points[0]);
}

// Searches a point by its index. Returns -1 if it doesn't find it
ECS_INLINE unsigned int ContactManifoldFeaturesFind(const ContactManifoldFeatures& manifold, unsigned int point_index) {
	for (unsigned int index = 0; index < manifold.point_count; index++) {
		if (manifold.point_indices[index] == point_index) {
			return index;
		}
	}
	return -1;
}

// Searches a point by its index and ensures that the edge indices are respected. Returns -1 if it doesn't find it
ECS_INLINE unsigned int ContactManifoldFeaturesFind(const ContactManifoldFeatures& manifold, unsigned int point_index, uint2 edge_indices) {
	for (unsigned int index = 0; index < manifold.point_count; index++) {
		if (manifold.point_indices[index] == point_index) {
			if (manifold.point_edge_indices[index] == edge_indices) {
				return index;
			}
		}
	}
	return -1;
}

// The indices mask is used to search only those points
// Searches a point by its index and ensures that the edge indices are respected. Returns -1 if it doesn't find it
ECS_INLINE unsigned int ContactManifoldFeaturesFind(
	const ContactManifoldFeatures& manifold, 
	unsigned int point_index, 
	uint2 edge_indices, 
	Stream<unsigned int> indices_mask
) {
	for (size_t index = 0; index < indices_mask.size; index++) {
		if (manifold.point_indices[indices_mask[index]] == point_index) {
			if (manifold.point_edge_indices[indices_mask[index]] == edge_indices) {
				return indices_mask[index];
			}
		}
	}
	return -1;
}

// Searches a point given a position, using a provided epsilon. Returns -1 if it doesn't find it
ECS_INLINE unsigned int ContactManifoldFindByPosition(const ContactManifold& manifold, float3 position, float3 epsilon) {
	for (unsigned int index = 0; index < manifold.point_count; index++) {
		if (CompareMask(manifold.points[index], position, epsilon)) {
			return index;
		}
	}
	return -1;
}

// The indices mask is used to search only those points
// Searches a point given a position, using a provided epsilon. Returns -1 if it doesn't find it
ECS_INLINE unsigned int ContactManifoldFindByPosition(const ContactManifold& manifold, float3 position, float3 epsilon, Stream<unsigned int> indices_mask) {
	for (size_t index = 0; index < indices_mask.size; index++) {
		if (CompareMask(manifold.points[indices_mask[index]], position, epsilon)) {
			return indices_mask[index];
		}
	}
	return -1;
}

// It takes the query as a reference because it will reverse the order of the
// Query in case the manifold cannot be computed from the initial order for the face case
PHYSICS_API ContactManifoldFeatures ComputeContactManifold(
	World* world,
	const ConvexHull* first_hull, 
	const ConvexHull* second_hull, 
	SATQuery& query
);

// The points must be coplanar. Returns the count of valid entries
PHYSICS_API size_t SimplifyContactManifoldPoints(World* world, Stream<float3> points, float3 plane_normal);

// The points must be coplanar. Returns the count of valid entries
PHYSICS_API size_t SimplifyContactManifoldPoints(World* world, Stream<ConvexHullClippedPoint> points, float3 plane_normal);