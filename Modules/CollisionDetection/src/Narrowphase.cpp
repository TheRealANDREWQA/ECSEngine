#include "pch.h"
#include "Narrowphase.h"

float3 ConvexHull::FurthestFrom(float3 direction) const
{
	/*Vector max_distance(-FLT_MAX);
	Vector max_points;
	Vector vector_direction(direction, direction);

	size_t simd_count = GetSimdCount(vertices.size, Vector::Lanes());
	size_t index = 0;
	for (; index < simd_count; index += Vector::Lanes()) {
		Vector current_vertices = Vector(vertices.buffer + index);
		Vector distance = Dot(current_vertices, vector_direction);
		Vector is_greater = distance > max_distance;
		max_distance = Select(is_greater, distance, max_distance);
		max_points = Select(is_greater, current_vertices, max_points);
	}

	float4 scalar_max_values[2];
	max_points.Store(scalar_max_values);
	float3 max = BasicType scalar_max_values->xyz()
	if (index < vertices.size) {

	}*/

	float3 max_point;
	float max_distance = -FLT_MAX;
	for (size_t index = 0; index < vertices.size; index++) {

	}
	
	return max_point;
}

float GJK(ConvexHull collider_a, ConvexHull collider_b) {
	return 0.0f;
}
