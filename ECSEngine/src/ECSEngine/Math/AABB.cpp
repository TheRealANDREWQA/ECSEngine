#include "ecspch.h"
#include "AABB.h"
#include "Vector.h"
#include "../Utilities/Utilities.h"

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

	AABBStorage GetAABBFromPoints(Stream<float3> points)
	{
		Vector8 vector_min(FLT_MAX), vector_max(-FLT_MAX);
		size_t simd_count = GetSimdCount(points.size, vector_min.Lanes());
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

	AABBStorage GetCombinedAABBStorage(AABBStorage first, AABBStorage second)
	{
		return {
			BasicTypeMin(first.min, second.min),
			BasicTypeMax(first.max, second.max)
		};
	}

	bool CompareAABBStorage(AABBStorage first, AABBStorage second, float3 epsilon) {
		return BasicTypeFloatCompareBoolean(first.min, first.min, epsilon) && BasicTypeFloatCompareBoolean(first.max, second.max, epsilon);
	}

	Vector8 ECS_VECTORCALL AABBCenter(AABB aabb) {
		Vector8 shuffle_min = SplatLowLane(aabb.value);
		Vector8 shuffle_max = SplatHighLane(aabb.value);
		Vector8 half = 0.5f;
		return Fmadd(shuffle_max - shuffle_min, half, shuffle_min);
	}

	Vector8 ECS_VECTORCALL AABBExtents(AABB aabb) {
		Vector8 shuffle = Permute2f128Helper<1, 0>(aabb.value, aabb.value);
		// Broadcast the low lane to the upper lane
		Vector8 extents = shuffle - aabb.value;
		return SplatLowLane(extents);
	}

	Vector8 ECS_VECTORCALL AABBHalfExtents(AABB aabb) {
		return AABBExtents(aabb) * Vector8(0.5f);
	}

	namespace SIMDHelpers {

		template<typename RotationStructure>
		ECS_INLINE RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, RotationStructure rotation_structure) {
			Vector8 x_axis = RightVector();
			Vector8 y_axis = UpVector();
			Vector8 z_axis = ForwardVector();

			Vector8 xy_axis = BlendLowAndHigh(x_axis, y_axis);
			Vector8 rotated_xy_axis = RotateVector(xy_axis, rotation_structure);
			Vector8 rotated_z_axis = RotateVector(z_axis, rotation_structure);

			Vector8 aabb_center = AABBCenter(aabb);
			Vector8 half_extents = AABBHalfExtents(aabb);

			return { BlendLowAndHigh(aabb_center, half_extents), rotated_xy_axis, rotated_z_axis };
		}

	}

	RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Matrix rotation_matrix) {
		return SIMDHelpers::RotateAABB(aabb, rotation_matrix);
	}

	RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Quaternion rotation) {
		return SIMDHelpers::RotateAABB(aabb, rotation);
	}

	AABB ECS_VECTORCALL AABBFromRotation(AABB aabb, Matrix rotation_matrix) {
		/*
			Let A be a min-max AAABB and M is a row major matrix. The axis aligned
			bounding box B that bounds A is specified by the extent intervals formed by
			the projection of the eight rotated vertices of A onto the world-coordinate axes. For,
			say, the x extents of B, only the x components of the row vectors of M contribute.
			Therefore, finding the extents corresponds to finding the vertices that produce the
			minimal and maximal products with the rows of M. Each vertex of B is a combination
			of three transformed min or max values from A. The minimum extent value is the
			sum of the smaller terms, and the maximum extent is the sum of the larger terms.
			Translation does not affect the size calculation of the new bounding box and can just
			be added in. For instance, the maximum extent along the x axis can be computed as:

			B.max[0] = max(m[0][0] * A.min[0], m[0][0] * A.max[0])
					 + max(m[1][0] * A.min[1], m[1][0] * A.max[1])
					 + max(m[2][0] * A.min[2], m[2][0] * A.max[2]);

			Credit to Christer Ericson's book Realtime collision detection

			Scalar algorithm
			for (int i = 0; i < 3; i++) {
				b.min[i] = b.max[i] = 0;
				// Form extent by summing smaller and larger terms respectively
				for (int j = 0; j < 3; j++) {
					float e = m[j][i] * a.min[j];
					float f = m[j][i] * a.max[j];
					if (e < f) {
						b.min[i] += e;
						b.max[i] += f;
					}
					else {
						b.min[i] += f;
						b.max[i] += e;
					}
				}
			}
		*/

		// An attempt at making this vectorized failed since there are so many shuffles and
		// Permutes that need to be made such that the best way is to go with the scalar algorithm
		// Maybe investigate later on a better vectorization method

		float4 scalar_matrix[4];
		AABBStorage aabb_storage = aabb.ToStorage();
		rotation_matrix.Store(scalar_matrix);

		// Use 2 float4's for the final aabb such that we can do a load without any permute
		float4 resulting_aabb[2];

		for (size_t column = 0; column < 3; column++) {
			resulting_aabb[0][column] = 0.0f;
			resulting_aabb[1][column] = 0.0f;
			// Form extent by summing smaller and larger terms respectively
			for (size_t row = 0; row < 3; row++) {
				float min_val = scalar_matrix[row][column] * aabb_storage.min[row];
				float max_val = scalar_matrix[row][column] * aabb_storage.max[row];
				if (min_val < max_val) {
					resulting_aabb[0][column] += min_val;
					resulting_aabb[1][column] += max_val;
				}
				else {
					resulting_aabb[0][column] += max_val;
					resulting_aabb[1][column] += min_val;
				}
			}
		}

		return Vector8(resulting_aabb);
	}

	AABB ECS_VECTORCALL ScaleAABB(AABB aabb, Vector8 scale) {
		Vector8 center = AABBCenter(aabb);
		Vector8 half_extents = AABBHalfExtents(aabb);
		Vector8 new_extents = half_extents * scale;
		Vector8 min_corner = center - new_extents;
		Vector8 max_corner = center + new_extents;
		return BlendLowAndHigh(min_corner, max_corner);
	}

	AABB ECS_VECTORCALL TransformAABB(AABB aabb, Vector8 translation, Matrix rotation_matrix, Vector8 scale) {
		// Firstly scale the AABB, then rotate and then translate, like a normal transformation
		AABB scaled_aabb = ScaleAABB(aabb, scale.SplatLow());
		AABB rotated_aabb = AABBFromRotation(scaled_aabb, rotation_matrix);
		return TranslateAABB(rotated_aabb, translation.SplatLow());
	}

	AABB ECS_VECTORCALL GetCombinedAABB(AABB first, AABB second) {
		return Vector8(BlendLowAndHigh(min(first.value, second.value), max(first.value, second.value)));
	}

	bool ECS_VECTORCALL AABBOverlap(AABB first, AABB second) {
		// Conditions: first.max < second.min || first.min > second.max -> false
		//				If passes for all dimensions, return true

		AABB second_max_first_min_shuffle = Permute2f128Helper<3, 0>(first.value, second.value);
		auto first_condition = first.value < second_max_first_min_shuffle.value;
		auto second_condition = first.value > second_max_first_min_shuffle.value;
		auto is_outside = BlendLowAndHigh(Vector8(second_condition), Vector8(first_condition));
		// We need to zero out the 4th component such that it doesn't give a false
		// Response that they overlap when there is garbage in that value
		auto zero_vector = ZeroVector();
		is_outside = PerLaneBlend<0, 1, 2, 7>(Vector8(is_outside), zero_vector);
		// If any of the bits is set, then we have one of the
		return horizontal_or(is_outside.AsMask());
	}

	bool AABBOverlapStorage(AABBStorage first, AABBStorage second)
	{
		// Conditions: first.max < second.min || first.min > second.max -> false
		// If passes for all dimensions, return true

		if (first.max.x < second.min.x || first.min.x > second.max.x) {
			return false;
		}
		if (first.max.y < second.min.y || first.min.y > second.max.y) {
			return false;
		}
		if (first.max.z < second.min.z || first.min.z > second.max.z) {
			return false;
		}
		return true;
	}

	void GetAABBCorners(AABB aabb, Vector8* corners)
	{
		Vector8 max_min = Permute2f128Helper<1, 0>(aabb.value, aabb.value);
		Vector8 xy_min_z_max = PerLaneBlend<0, 1, 6, V_DC>(aabb.value, max_min);
		Vector8 x_min_y_max_z_min = PerLaneBlend<0, 5, 2, V_DC>(aabb.value, max_min);
		Vector8 x_max_yz_min = PerLaneBlend<4, 1, 2, V_DC>(aabb.value, max_min);
		Vector8 xy_max_z_min = PerLaneBlend<4, 5, 2, V_DC>(aabb.value, max_min);
		Vector8 x_max_y_min_z_max = PerLaneBlend<4, 1, 6, V_DC>(aabb.value, max_min);
		Vector8 x_min_yz_max = PerLaneBlend<0, 5, 6, V_DC>(aabb.value, max_min);

		// The left face have the x min
		// The right face have the x max
		corners[0] = BlendLowAndHigh(aabb.value, xy_min_z_max);
		corners[1] = BlendLowAndHigh(x_min_y_max_z_min, x_min_yz_max);

		corners[2] = BlendLowAndHigh(x_max_yz_min, x_max_y_min_z_max);
		corners[3] = BlendLowAndHigh(xy_max_z_min, aabb.value);
	}

}
