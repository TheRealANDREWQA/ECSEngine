//#pragma once
//#include "../Core.h"
//#include "../Utilities/BasicTypes.h"
//#include "../Containers/Stream.h"
//#include "Conversion.h"
//
//namespace ECSEngine {
//
//	struct AABBStorage {
//		float3 min;
//		float3 max;
//	};
//
//	struct AABB {
//		ECS_INLINE AABB() {}
//		ECS_INLINE AABB(float3 min, float3 max) : value(min, max) {}
//		ECS_INLINE AABB(AABBStorage storage) : value((float3*)&storage) {}
//		ECS_INLINE AABB(Vector8 _value) : value(_value) {}
//		
//		ECS_INLINE operator Vector8() const {
//			return value;
//		}
//
//		ECS_INLINE AABBStorage ToStorage() const {
//			AABBStorage aabb;
//			value.StoreFloat3((float3*)&aabb);
//			return aabb;
//		}
//
//		Vector8 value;
//	};
//
//	// This is the same as an OOBB but we haven't decided upon the OOBB
//	// Representation yet. Use this as a placeholder
//	struct RotatedAABB {
//		// Low lane center, high lane extents
//		Vector8 center_extents;
//		Vector8 local_axis_xy;
//		// The axis is splatted in both lanes
//		Vector8 local_axis_z;
//	};
//
//	// Returns the AABBStorage that encompases all of them
//	ECSENGINE_API AABBStorage GetCoalescedAABB(Stream<AABBStorage> aabbs);
//
//	ECSENGINE_API AABBStorage GetAABBFromPoints(Stream<float3> points);
//
//	ECSENGINE_API AABBStorage GetCombinedAABBStorage(AABBStorage first, AABBStorage second);
//
//	ECS_INLINE AABBStorage InfiniteAABBStorage() {
//		return { float3::Splat(-FLT_MAX), float3::Splat(FLT_MAX) };
//	}
//
//	// Can use this to initialized the bounding box when calculating it from points
//	ECS_INLINE AABBStorage ReverseInfiniteAABBStorage() {
//		return { float3::Splat(FLT_MAX), float3::Splat(-FLT_MAX) };
//	}
//
//	ECS_INLINE bool CompareAABBStorage(AABBStorage first, AABBStorage second, float3 epsilon = float3::Splat(ECS_SIMD_VECTOR_EPSILON_VALUE)) {
//		return BasicTypeFloatCompareBoolean(first.min, first.min, epsilon) && BasicTypeFloatCompareBoolean(first.max, second.max, epsilon);
//	}
//
//	ECS_INLINE float3 AABBCenterStorage(AABBStorage aabb) {
//		return aabb.min + (aabb.max - aabb.min) * float3::Splat(0.5f);
//	}
//
//	ECS_INLINE Vector8 ECS_VECTORCALL AABBCenter(AABB aabb) {
//		Vector8 shuffle_min = SplatLowLane(aabb.value);
//		Vector8 shuffle_max = SplatHighLane(aabb.value);
//		Vector8 half = 0.5f;
//		return Fmadd(shuffle_max - shuffle_min, half, shuffle_min);
//	}
//
//	ECS_INLINE Vector8 ECS_VECTORCALL AABBExtents(AABB aabb) {
//		Vector8 shuffle = Permute2f128Helper<1, 0>(aabb.value, aabb.value);
//		// Broadcast the low lane to the upper lane
//		Vector8 extents = shuffle - aabb.value;
//		return SplatLowLane(extents);
//	}
//
//	ECS_INLINE Vector8 ECS_VECTORCALL AABBHalfExtents(AABB aabb) {
//		return AABBExtents(aabb) * Vector8(0.5f);
//	}
//
//	// The translation value needs to be same in the low and high lanes
//	ECS_INLINE AABB ECS_VECTORCALL TranslateAABB(AABB aabb, Vector8 translation) {
//		return aabb.value + translation;
//	}
//
//	namespace SIMDHelpers {
//
//		template<typename RotationStructure>
//		ECS_INLINE RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, RotationStructure rotation_structure) {
//			Vector8 x_axis = RightVector();
//			Vector8 y_axis = UpVector();
//			Vector8 z_axis = ForwardVector();
//
//			Vector8 xy_axis = BlendLowAndHigh(x_axis, y_axis);
//			Vector8 rotated_xy_axis = RotateVector(xy_axis, rotation_structure);
//			Vector8 rotated_z_axis = RotateVector(z_axis, rotation_structure);
//
//			Vector8 aabb_center = AABBCenter(aabb);
//			Vector8 half_extents = AABBHalfExtents(aabb);
//
//			return { BlendLowAndHigh(aabb_center, half_extents), rotated_xy_axis, rotated_z_axis };
//		}
//
//	}
//
//	// It does not rotate the center of the AABB, it only rotates the corners
//	// With respect to the center of the AABB
//	ECS_INLINE RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Matrix rotation_matrix) {
//		return SIMDHelpers::RotateAABB(aabb, rotation_matrix);
//	}
//
//	// It does not rotate the center of the AABB, it only rotates the corners
//	// With respect to the center of the AABB
//	ECS_INLINE RotatedAABB ECS_VECTORCALL RotateAABB(AABB aabb, Quaternion rotation) {
//		return SIMDHelpers::RotateAABB(aabb, rotation);
//	}
//
//	// Computes the corresponding AABB from the given rotation matrix
//	// If you want to use a quaternion, you must convert it to a rotation matrix first
//	// And then call this function
//	ECS_INLINE AABB ECS_VECTORCALL AABBFromRotation(AABB aabb, Matrix rotation_matrix) {
//		/*
//			Let A be a min-max AAABB and M is a row major matrix. The axis aligned
//			bounding box B that bounds A is specified by the extent intervals formed by
//			the projection of the eight rotated vertices of A onto the world-coordinate axes. For,
//			say, the x extents of B, only the x components of the row vectors of M contribute.
//			Therefore, finding the extents corresponds to finding the vertices that produce the
//			minimal and maximal products with the rows of M. Each vertex of B is a combination
//			of three transformed min or max values from A. The minimum extent value is the
//			sum of the smaller terms, and the maximum extent is the sum of the larger terms.
//			Translation does not affect the size calculation of the new bounding box and can just
//			be added in. For instance, the maximum extent along the x axis can be computed as:
//
//			B.max[0] = max(m[0][0] * A.min[0], m[0][0] * A.max[0])
//					 + max(m[1][0] * A.min[1], m[1][0] * A.max[1])
//					 + max(m[2][0] * A.min[2], m[2][0] * A.max[2]);
//
//			Credit to Christer Ericson's book Realtime collision detection
//
//			Scalar algorithm
//			for (int i = 0; i < 3; i++) {
//				b.min[i] = b.max[i] = 0;
//				// Form extent by summing smaller and larger terms respectively
//				for (int j = 0; j < 3; j++) {
//					float e = m[j][i] * a.min[j];
//					float f = m[j][i] * a.max[j];
//					if (e < f) {
//						b.min[i] += e;
//						b.max[i] += f;
//					}
//					else {
//						b.min[i] += f;
//						b.max[i] += e;
//					}
//				}
//			}
//		*/
//
//		// An attempt at making this vectorized failed since there are so many shuffles and
//		// Permutes that need to be made such that the best way is to go with the scalar algorithm
//		// Maybe investigate later on a better vectorization method
//
//		float4 scalar_matrix[4];
//		AABBStorage aabb_storage = aabb.ToStorage();
//		rotation_matrix.Store(scalar_matrix);
//
//		// Use 2 float4's for the final aabb such that we can do a load without any permute
//		float4 resulting_aabb[2];
//
//		for (size_t column = 0; column < 3; column++) {
//			resulting_aabb[0][column] = 0.0f;
//			resulting_aabb[1][column] = 0.0f;
//			// Form extent by summing smaller and larger terms respectively
//			for (size_t row = 0; row < 3; row++) {
//				float min_val = scalar_matrix[row][column] * aabb_storage.min[row];
//				float max_val = scalar_matrix[row][column] * aabb_storage.max[row];
//				if (min_val < max_val) {
//					resulting_aabb[0][column] += min_val;
//					resulting_aabb[1][column] += max_val;
//				}
//				else {
//					resulting_aabb[0][column] += max_val;
//					resulting_aabb[1][column] += min_val;
//				}
//			}
//		}
//
//		return Vector8(resulting_aabb);
//	}
//
//	// The low and high lanes of the scale need to be the same
//	// This one scales the dimensions of the aabb (the center stays the same)
//	ECS_INLINE AABB ECS_VECTORCALL ScaleAABB(AABB aabb, Vector8 scale) {
//		Vector8 center = AABBCenter(aabb);
//		Vector8 half_extents = AABBHalfExtents(aabb);
//		Vector8 new_extents = half_extents * scale;
//		Vector8 min_corner = center - new_extents;
//		Vector8 max_corner = center + new_extents;
//		return BlendLowAndHigh(min_corner, max_corner);
//	}
//
//	// The low and high lanes of the scale need to be the same
//	// This version scales the values from the origin (it is as if the
//	// min and max are multiplied with the factor scale)
//	ECS_INLINE AABB ECS_VECTORCALL ScaleAABBFromOrigin(AABB aabb, Vector8 scale) {
//		return aabb.value * scale;
//	}
//
//	// Returns an encompasing AABB from the given transformation
//	// Only the low for translation and scale are considered
//	ECS_INLINE AABB ECS_VECTORCALL TransformAABB(AABB aabb, Vector8 translation, Matrix rotation_matrix, Vector8 scale) {
//		// Firstly scale the AABB, then rotate and then translate, like a normal transformation
//		AABB scaled_aabb = ScaleAABB(aabb, scale.SplatLow());
//		AABB rotated_aabb = AABBFromRotation(scaled_aabb, rotation_matrix);
//		return TranslateAABB(rotated_aabb, translation.SplatLow());
//	}
//
//	// Applies the matrix to both the min and max
//	ECS_INLINE Vector8 ECS_VECTORCALL ApplyMatrixOnAABB(AABB aabb, Matrix matrix) {
//		return TransformPoint(aabb.value, matrix);
//	}
//
//	ECS_INLINE Vector8 ECS_VECTORCALL CompareAABBMask(AABB first, AABB second, Vector8 epsilon = VectorGlobals::EPSILON) {
//		return CompareMask(first.value, second.value, epsilon);
//	}
//
//	ECS_INLINE bool ECS_VECTORCALL CompareAABB(AABB first, AABB second, Vector8 epsilon = VectorGlobals::EPSILON) {
//		return CompareWhole(first.value, second.value, epsilon);
//	}
//
//	ECS_INLINE AABB ECS_VECTORCALL GetCombinedAABB(AABB first, AABB second) {
//		return Vector8(BlendLowAndHigh(min(first.value, second.value), max(first.value, second.value)));
//	}
//
//	ECS_INLINE AABB ECS_VECTORCALL InfiniteAABB() {
//		return InfiniteAABBStorage();
//	}
//
//	ECS_INLINE AABB ECS_VECTORCALL ReverseInfiniteAABB() {
//		return ReverseInfiniteAABBStorage();
//	}
//
//	ECS_INLINE bool ECS_VECTORCALL AABBOverlap(AABB first, AABB second) {
//		// Conditions: first.max < second.min || first.min > second.max -> false
//		//				If passes for all dimensions, return false
//
//		AABB second_max_first_min_shuffle = Permute2f128Helper<3, 0>(first.value, second.value);
//		auto first_condition = first.value < second_max_first_min_shuffle.value;
//		auto second_condition = first.value > second_max_first_min_shuffle.value;
//		auto is_outside = BlendLowAndHigh(Vector8(second_condition), Vector8(first_condition));
//		// We need to zero out the 4th component such that it doesn't give a false
//		// Response that they overlap when there is garbage in that value
//		auto zero_vector = ZeroVector();
//		is_outside = PerLaneBlend<0, 1, 2, 7>(Vector8(is_outside), zero_vector);
//		// If any of the bits is set, then we have one of the
//		return horizontal_or(is_outside.AsMask());
//	}
//
//	ECSENGINE_API bool AABBOverlapStorage(AABBStorage first, AABBStorage second);
//
//	// There need to be 4 entries for the corners pointer
//	// The first 2 entries are the "left" face and the other
//	// 2 are for the "right" face
//	ECSENGINE_API void GetAABBCorners(AABB aabb, Vector8* corners);
//
//}