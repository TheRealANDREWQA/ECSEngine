#pragma once
#include "ConvexHull.h"

// A negative distance means penetration, a positive value means separation
struct SATEdgeQuery {
	float distance;
	unsigned int edge_1_index;
	unsigned int edge_2_index;
	// This axis points away from the first hull
	// If the first hull is moved with a translation equal to that of
	// Distance along this axis, it will result in non-penetration
	float3 separation_axis;
};

// A negative distance means penetration, a positive value means separation
struct SATFaceQuery {
	float distance;
	unsigned int face_index;
	// This is true if the face belongs to the first collider
	bool first_collider;
};

enum SAT_QUERY_TYPE : unsigned char {
	SAT_QUERY_NONE,
	SAT_QUERY_EDGE,
	SAT_QUERY_FACE
};

struct SATQuery {
	ECS_INLINE SATQuery() : type(SAT_QUERY_NONE) {}

	SAT_QUERY_TYPE type;
	union {
		SATEdgeQuery edge;
		SATFaceQuery face;
	};
};

COLLISIONDETECTION_API SATQuery SAT(const ConvexHull* first, const ConvexHull* second);