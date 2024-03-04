#pragma once
#include "ECSEngineMath.h"
#include "Export.h"
#include "CollisionDetection/src/SAT.h"

struct ConvexHull;

struct ContactManifold {
	ECS_INLINE void AddContactPoint(float3 point) {
		ECS_ASSERT(contact_point_count < ECS_COUNTOF(contact_points), "ContactManifold too many contact points!");
		contact_points[contact_point_count++] = point;
	}

	ECS_INLINE void WriteContactPoints(Stream<float3> points) {
		ECS_ASSERT(contact_point_count + points.size <= ECS_COUNTOF(contact_points), "ContactManifold too many contact points!");
		points.CopyTo(contact_points + contact_point_count);
		contact_point_count += points.size;
	}

	float3 separation_axis;
	float separation_distance;
	unsigned int contact_point_count = 0;
	float3 contact_points[4];
};

PHYSICS_API ContactManifold ComputeContactManifold(const ConvexHull* first_hull, const ConvexHull* second_hull, SATQuery query);

// The points must be coplanar. Returns the count of valid entries
PHYSICS_API size_t SimplifyContactManifoldPoints(Stream<float3> points, float3 plane_normal);