#pragma once
#include "Export.h"
#include "ECSEngineContainers.h"

struct ConvexHull {
	float3 FurthestFrom(float3 direction) const;

	Stream<float3> vertices;
};

float GJK(ConvexHull collider_a, ConvexHull collider_b);