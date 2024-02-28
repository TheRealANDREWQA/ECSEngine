#pragma once
#include "ConvexHull.h"

// A negative distance means penetration, a positive value means separation
struct SATEdgeQuery {
	float distance;
	unsigned int edge_1_index;
	unsigned int edge_2_index;
};

// A negative distance means penetration, a positive value means separation
struct SATFaceQuery {
	float distance;
	unsigned int face_index;
};

enum SAT_QUERY_TYPE : unsigned char {
	SAT_QUERY_NONE,
	SAT_QUERY_EDGE,
	SAT_QUERY_FACE
};

struct SATQuery {
	SAT_QUERY_TYPE type;
	union {
		SATEdgeQuery edge;
		SATFaceQuery face;
	};
};

COLLISIONDETECTION_API SATQuery SAT(const ConvexHull* first, const ConvexHull* second);