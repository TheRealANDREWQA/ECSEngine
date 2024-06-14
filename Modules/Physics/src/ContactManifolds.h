#pragma once
#include "ECSEngineMath.h"
#include "Export.h"
#include "CollisionDetection/src/SAT.h"

struct ConvexHull;

struct ContactManifold {
	ECS_INLINE void AddContactPoint(float3 point) {
		ECS_ASSERT(point_count < ECS_COUNTOF(points), "ContactManifold too many contact points!");
		points[point_count++] = point;
	}

	ECS_INLINE void WriteContactPoints(Stream<float3> write_points) {
		ECS_ASSERT(point_count + write_points.size <= ECS_COUNTOF(points), "ContactManifold too many contact points!");
		write_points.CopyTo(points + point_count);
		point_count += write_points.size;
	}

	ECS_INLINE PlaneScalar GetPlane() const {
		// The manifold must have at least one point
		return PlaneScalar::FromNormalized(separation_axis, points[0]);
	}

	// This axis needs to be normalized
	float3 separation_axis;
	float separation_distance;
	unsigned int point_count = 0;
	float3 points[4];
};

struct ContactManifoldFeatures {

	
};

PHYSICS_API ContactManifold ComputeContactManifold(const ConvexHull* first_hull, const ConvexHull* second_hull, SATQuery query);

// The points must be coplanar. Returns the count of valid entries
PHYSICS_API size_t SimplifyContactManifoldPoints(Stream<float3> points, float3 plane_normal);