#include "ecspch.h"
#include "AABB.h"
#include "Vector.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	AABBStorage GetCoalescedAABB(Stream<AABBStorage> aabbs)
	{
		float3 min = float3::Splat(FLT_MAX);
		float3 max = float3::Splat(-FLT_MAX);
		for (size_t index = 0; index < aabbs.size; index++) {
			min = BasicTypeMin(min, aabbs[index].min);
			max = BasicTypeMax(max, aabbs[index].max);
		}

		return { min, max };
	}

	AABBStorage GetPointsBoundingBox(Stream<float3> points)
	{
		Vector8 vector_min(FLT_MAX), vector_max(-FLT_MAX);
		size_t simd_count = function::GetSimdCount(points.size, vector_min.Lanes());
		for (size_t index = 0; index < simd_count; index += vector_min.Lanes()) {
			Vector8 current_positions(points.buffer + index);
			vector_min = min(vector_min, current_positions);
			vector_max = max(vector_max, current_positions);
		}

		float4 mins[2];
		float4 maxs[2];

		vector_min.Store(mins);
		vector_max.Store(maxs);

		float3 current_min = BasicTypeMin(mins[0].xyz(), mins[1].xyz());
		float3 current_max = BasicTypeMax(maxs[0].xyz(), maxs[1].xyz());

		if (simd_count < points.size) {
			current_min = BasicTypeMin(current_min, points[simd_count]);
			current_max = BasicTypeMax(current_max, points[simd_count]);
		}

		return { current_min, current_max };
	}

	AABBStorage GetCombinedBoundingBox(AABBStorage first, AABBStorage second)
	{
		return {
			BasicTypeMin(first.min, second.min),
			BasicTypeMax(first.max, second.max)
		};
	}

}
