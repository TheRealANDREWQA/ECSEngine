#include "ecspch.h"
#include "AABB.h"
#include "../Utilities/Utilities.h"

namespace ECSEngine {

	AABBScalar GetCoalescedAABB(Stream<AABBScalar> aabbs)
	{
		float3 min = float3::Splat(FLT_MAX);
		float3 max = float3::Splat(-FLT_MAX);
		for (size_t index = 0; index < aabbs.size; index++) {
			min = BasicTypeMin(min, aabbs[index].min);
			max = BasicTypeMax(max, aabbs[index].max);
		}

		return { min, max };
	}

	AABBScalar GetAABBFromPoints(Stream<float3> points)
	{
		Vec8f vector_min = FLT_MAX;
		Vec8f vector_max = -FLT_MAX;
		// Here we are using just 2 float3's at a time
		const size_t simd_width = Vec8f::size() / float3::Count();
		ECS_ASSERT(IsPowerOfTwo(simd_width));
		size_t simd_count = GetSimdCount(points.size, simd_width);
		for (size_t index = 0; index < simd_count; index += simd_width) {
			Vec8f current_positions = Vec8f().load((const float*)(points.buffer + index));
			vector_min = min(vector_min, current_positions);
			vector_max = max(vector_max, current_positions);
		}

		float3 mins[simd_width + 1];
		float3 maxs[simd_width + 1];

		vector_min.store((float*)mins);
		vector_max.store((float*)maxs);

		float3 current_min = BasicTypeMin(mins[0], mins[1]);
		float3 current_max = BasicTypeMax(maxs[0], maxs[1]);

		for (size_t index = simd_count; index < points.size; index++) {
			current_min = BasicTypeMin(current_min, points[index]);
			current_max = BasicTypeMax(current_max, points[index]);
		}

		return { current_min, current_max };
	}

	template<typename RotationStructure>
	ECS_INLINE RotatedAABB ECS_VECTORCALL RotateAABBImpl(AABB aabb, RotationStructure rotation_structure) {
		Vector3 x_axis = RightVector();
		Vector3 y_axis = UpVector();

		Vector3 rotated_x_axis = RotateVector(x_axis, rotation_structure);
		Vector3 rotated_y_axis = RotateVector(y_axis, rotation_structure);

		Vector3 aabb_center = AABBCenter(aabb);
		Vector3 half_extents = AABBHalfExtents(aabb);

		return { aabb_center, half_extents, rotated_x_axis, rotated_y_axis };
	}

	RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Matrix rotation_matrix) {
		return RotateAABBImpl(aabb, rotation_matrix);
	}

	RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Quaternion rotation) {
		return RotateAABBImpl(aabb, rotation);
	}

	AABBScalar AABBFromRotation(const AABBScalar& aabb, const Matrix& rotation_matrix)
	{
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

		AABBScalar result;
		alignas(ECS_SIMD_BYTE_SIZE) float matrix_values[4][4];
		rotation_matrix.StoreAligned(matrix_values);
		for (size_t index = 0; index < 3; index++) {
			result.min[index] = 0.0f;
			result.max[index] = 0.0f;
			for (size_t subindex = 0; subindex < 3; subindex++) {
				float min_factor = matrix_values[subindex][index] * aabb.min[subindex];
				float max_factor = matrix_values[subindex][index] * aabb.max[subindex];
				if (min_factor > max_factor) {
					std::swap(min_factor, max_factor);
				}
				result.min[index] += min_factor;
				result.max[index] += max_factor;
			}
		}

		return result;
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

		AABB result;
		for (size_t index = 0; index < 3; index++) {
			result.min[index] = ZeroVectorFloat();
			result.max[index] = ZeroVectorFloat();
			// Broadcast the matrix element such that we perform the multiplication
			// With the entire min/max vectors
			// Instead of using a loop, flatten it out such that we don't branch on the broadcasted element
			// Multiple times
			Vec8f matrix_element;
			auto perform_step = [&](size_t j) {
				Vec8f min_factor = matrix_element * aabb.min[j];
				Vec8f max_factor = matrix_element * aabb.max[j];
				SIMDVectorMask min_mask = min_factor < max_factor;
				min_factor = select(min_mask, min_factor, max_factor);
				max_factor = select(min_mask, max_factor, min_factor);
				result.min[index] += min_factor;
				result.max[index] += max_factor;
			};

			if (index == 0) {
				matrix_element = Broadcast8<0>(rotation_matrix.v[0]);
			}
			else if (index == 1) {
				matrix_element = Broadcast8<1>(rotation_matrix.v[0]);
			}
			else {
				matrix_element = Broadcast8<2>(rotation_matrix.v[0]);
			}
			perform_step(0);

			if (index == 0) {
				matrix_element = Broadcast8<4>(rotation_matrix.v[0]);
			}
			else if (index == 1) {
				matrix_element = Broadcast8<5>(rotation_matrix.v[0]);
			}
			else {
				matrix_element = Broadcast8<6>(rotation_matrix.v[0]);
			}
			perform_step(1);

			if (index == 0) {
				matrix_element = Broadcast8<0>(rotation_matrix.v[1]);
			}
			else if (index == 1) {
				matrix_element = Broadcast8<1>(rotation_matrix.v[1]);
			}
			else {
				matrix_element = Broadcast8<2>(rotation_matrix.v[1]);
			}
			perform_step(2);
		}
		return result;
	}

	template<typename AABB, typename Vector>
	static ECS_INLINE AABB ECS_VECTORCALL ScaleAABBImpl(AABB aabb, Vector scale) {
		Vector center = AABBCenter(aabb);
		Vector half_extents = AABBHalfExtents(aabb);
		Vector new_extents = half_extents * scale;
		Vector min_corner = center - new_extents;
		Vector max_corner = center + new_extents;
		return { min_corner, max_corner };
	}

	AABBScalar ScaleAABB(const AABBScalar& aabb, float3 scale) {
		return ScaleAABBImpl(aabb, scale);
	}

	AABB ECS_VECTORCALL ScaleAABB(AABB aabb, Vector3 scale) {
		return ScaleAABBImpl(aabb, scale);
	}

	template<typename AABBType, typename Vector>
	static ECS_INLINE AABBType ECS_VECTORCALL TransformAABBImpl(AABBType aabb, Vector translation, Matrix rotation_matrix, Vector scale) {
		// Firstly scale the AABB, then rotate and then translate, like a normal transformation
		auto scaled_aabb = ScaleAABB(aabb, scale);
		auto rotated_aabb = AABBFromRotation(scaled_aabb, rotation_matrix);
		return TranslateAABB(rotated_aabb, translation);
	}

	AABBScalar TransformAABB(const AABBScalar& aabb, float3 translation, const Matrix& rotation_matrix, float3 scale) {
		return TransformAABBImpl(aabb, translation, rotation_matrix, scale);
	}

	AABB ECS_VECTORCALL TransformAABB(AABB aabb, Vector3 translation, Matrix rotation_matrix, Vector3 scale) {
		return TransformAABBImpl(aabb, translation, rotation_matrix, scale);
	}

	AABBScalar GetCombinedAABB(const AABBScalar& first, const AABBScalar& second) {
		return { BasicTypeMin(first.min, second.min), BasicTypeMax(first.max, second.max) };
	}

	AABB ECS_VECTORCALL GetCombinedAABB(AABB first, AABB second) {
		return { Min(first.min, second.min), Max(first.max, second.max) };
	}

	bool AABBOverlap(const AABBScalar& first, const AABBScalar& second) {
		// Conditions: first.max < second.min || first.min > second.max -> false
		//				If passes for all dimensions, return true

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

	SIMDVectorMask ECS_VECTORCALL AABBOverlap(AABB first, AABB second, size_t first_index) {
		// Conditions: first.max < second.min || first.min > second.max -> false
		//				If passes for all dimensions, return true
		Vector3 first_min = { Broadcast8(first.min.x, first_index), Broadcast8(first.min.y, first_index), Broadcast8(first.min.z, first_index) };
		Vector3 first_max = { Broadcast8(first.max.x, first_index), Broadcast8(first.max.y, first_index), Broadcast8(first.max.z, first_index) };
		
		SIMDVectorMask x_passed = first_max.x >= second.min.x && first_min.x <= second.max.x;
		SIMDVectorMask y_passed = first_max.y >= second.min.y && first_min.y <= second.max.y;
		SIMDVectorMask z_passed = first_max.z >= second.min.z && first_min.z <= second.max.z;
		return x_passed && y_passed && z_passed;
	}

	SIMDVectorMask ECS_VECTORCALL AABBOverlap(AABB aabbs, size_t index) {
		return AABBOverlap(aabbs, aabbs, index);
	}

	template<typename AABB, typename Vector>
	static ECS_INLINE void GetAABBCornersImpl(AABB aabb, Vector* corners) {
		// The left face have the x min
		corners[0] = aabb.min;
		corners[1] = { aabb.min.x, aabb.max.y, aabb.max.z };
		corners[2] = { aabb.min.x, aabb.min.y, aabb.max.z };
		corners[3] = { aabb.min.x, aabb.max.y, aabb.min.z };

		// The right face have the x max
		corners[4] = aabb.max;
		corners[5] = { aabb.max.x, aabb.min.y, aabb.min.z };
		corners[6] = { aabb.max.x, aabb.min.y, aabb.max.z };
		corners[7] = { aabb.max.x, aabb.max.y, aabb.min.z };
	}

	void GetAABBCorners(const AABBScalar& aabb, float3* corners) {
		GetAABBCornersImpl(aabb, corners);
	}

	void GetAABBCorners(AABB aabb, Vector3* corners)
	{
		GetAABBCornersImpl(aabb, corners);
	}

	template<typename ReturnType, typename AABB, typename Vector>
	static ECS_INLINE ReturnType ECS_VECTORCALL AABBToPointSquaredDistanceImpl(AABB aabb, Vector point) {
		// The squared distance is the excess positive distance outside
		// The AABB extents
		ReturnType zero = SingleZeroVector<ReturnType>();
		ReturnType squared_distance = zero;
		for (size_t index = 0; index < Vector::Count(); index++) {
			ReturnType min_difference = aabb.min[index] - point[index];
			min_difference = Max<ReturnType>(min_difference, zero);
			squared_distance += min_difference * min_difference;

			ReturnType max_difference = point[index] - aabb.max[index];
			max_difference = Max<ReturnType>(max_difference, zero);
			squared_distance += max_difference * max_difference;
		}
		return squared_distance;
	}

	float AABBToPointSquaredDistance(AABBScalar aabb, float3 point) {
		return AABBToPointSquaredDistanceImpl<float>(aabb, point);
	}

	Vec8f ECS_VECTORCALL AABBToPointSquaredDistance(AABB aabb, Vector3 point) {
		return AABBToPointSquaredDistanceImpl<Vec8f>(aabb, point);
	}

	template<typename SingleValue, typename AABB, typename Vector>
	static ECS_INLINE Vector ECS_VECTORCALL AABBClosestPointImpl(AABB aabb, Vector point) {
		// The closest point is found by clamping the point to the min and max of the AABB
		Vector closest_point;
		for (size_t index = 0; index < Vector::Count(); index++) {
			SingleValue current_axis = point[index];
			current_axis = Max<SingleValue>(current_axis, aabb.min[index]);
			current_axis = Min<SingleValue>(current_axis, aabb.max[index]);
			closest_point[index] = current_axis;
		}
		return closest_point;
	}

	float3 AABBClosestPoint(AABBScalar aabb, float3 point) {
		return AABBClosestPointImpl<float>(aabb, point);
	}

	Vector3 ECS_VECTORCALL AABBClosestPoint(AABB aabb, Vector3 point) {
		return AABBClosestPointImpl<Vec8f>(aabb, point);
	}

}
