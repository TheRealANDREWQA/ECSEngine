#include "ecspch.h"
#include "Intersection.h"

#define INTERSECT_INTERPOLATION_THRESHOLD 1000000.0f

namespace ECSEngine {

	namespace SIMDHelpers {

		// Can be used for line tests, ray tests (semi-lines) or segment tests
		// Returns a mask that can be used to perform other operations
		template<typename ResultFunctor>
		ECS_INLINE Vector8 ECS_VECTORCALL IntersectLinePlane(
			Vector8 line_origin,
			Vector8 line_direction,
			Plane plane,
			Vector8& output,
			ResultFunctor&& result_functor
		) {
			// t = (plane.dot - Dot3(plane.normal, segment_a)) / Dot3(plane.normal, segment_direction)
			// The t_factor is stored in the 4th component
			Vector8 t_simd = (plane.normal_dot - Dot(plane.normal_dot, line_origin)) / Dot(plane.normal_dot, line_direction);
			t_simd = PerLaneBroadcast<3>(t_simd);
			Vector8 mask = result_functor(t_simd);
			// Calculating the final result is not expensive - 5 cycles
			output = Fmadd(t_simd, line_direction, line_origin.value);
			output = PerLaneBlend<0, 1, 2, 7>(output, t_simd);
			return mask;
		}

		// This is more or less for references purposes, in order to construct the SIMD versions
		// Returns true when there is an intersection, else false. Don't access the values when
		// There is no intersection
		static bool IntersectLineAABBScalar(
			float3 line_origin, 
			float3 line_direction_normalized, 
			AABBStorage aabb, 
			float& interpolation_factor_t, 
			float3& intersection_point
		) {
			// The code is inspired from Christer's Ericson Realtime Collision Detection Book

			interpolation_factor_t = 0.0f;
			float t_max = FLT_MAX;
			// For all three slabs
			for (size_t index = 0; index < 3; index++) {
				float min_difference = aabb.min[index] - line_origin[index];
				float max_difference = aabb.max[index] - line_origin[index];

				if (Abs(line_direction_normalized[index]) < ECS_SIMD_VECTOR_EPSILON_VALUE) {
					// In this case, the ray is parallel to one of the planes. If the origin
					// Lies outside the AABB, then there is no intersection
					if (min_difference > 0.0f || max_difference < 0.0f) {
						return false;
					}
				}
				else {
					// Calculate intersection t value of ray with near and far plane
					float direction_inverse = 1.0f / line_direction_normalized[index];
					float current_t_min = min_difference * direction_inverse;
					float current_t_max = max_difference * direction_inverse;
					// Swap the values if they are in the incorrect order
					if (current_t_min > current_t_max) {
						std::swap(current_t_min, current_t_max);
					}

					if (current_t_min > interpolation_factor_t) {
						interpolation_factor_t = current_t_min;
					}
					if (current_t_max > t_max) {
						t_max = current_t_max;
					}

					// If the interpolation factor t is bigger than the "max" interpolation factor
					if (interpolation_factor_t > t_max) {
						return false;
					}
				}
			}
			// Ray intersects all 3 slabs. Return point (q) and intersection t value (tmin)
			intersection_point = line_origin + line_direction_normalized * interpolation_factor_t;
			return true;
		}

		// The line origin must be splatted across both lanes
		// The line direction must be splatted across both lanes
		// The output will be splatted in both lanes
		template<typename ResultFunctor>
		ECS_INLINE Vector8 ECS_VECTORCALL IntersectLineAABB(
			Vector8 line_origin,
			Vector8 line_direction_normalized,
			AABB aabb,
			Vector8& output,
			ResultFunctor&& result_functor
		) {
			// In the low lane there is AABB.min - origin
			// In the high lane there is AABB.max - origin
			Vector8 min_max_distance = aabb.value - line_origin;
			Vector8 parallel_mask = Abs(line_direction_normalized) < VectorGlobals::EPSILON;
			if (horizontal_or(parallel_mask.AsMask())) {
				// This is the case that the ray is parallel to one of the planes
				// There is no hit in case the origin lies outside the AABB bounds
				Vector8 smaller = min_max_distance > ZeroVector();
				Vector8 greater = min_max_distance < ZeroVector();
				// Blend these two vectors to use than a single or instruction
				Vector8 combined_mask = BlendLowAndHigh(smaller, greater);
				if (horizontal_or(combined_mask.AsMask())) {
					// We can return now - empty mask
					return ZeroVector();
				}
			}

			// The low part is the min, the high part is the max
			Vector8 direction_inverse = OneDividedVector(line_direction_normalized);
			Vector8 current_t_min_max = min_max_distance * direction_inverse;
			// TODO: At the moment, it seems quite hard to continue using SIMD
			// So revert to scalar code (see if there is a use case for this type of call) 
			alignas(sizeof(direction_inverse)) float4 direction_inverse_scalar[2];
			alignas(sizeof(current_t_min_max)) float4 current_t_min_max_scalar[2];
			direction_inverse.StoreAligned(direction_inverse_scalar);
			current_t_min_max.StoreAligned(current_t_min_max_scalar);
			float t_min = 0.0f;
			float t_max = FLT_MAX;
			for (size_t index = 0; index < 3; index++) {
				float current_min = current_t_min_max_scalar[0][index];
				float current_max = current_t_min_max_scalar[1][index];
				if (current_min > current_max) {
					std::swap(current_min, current_max);
				}
				if (current_min > t_min) {
					t_min = current_min;
				}
				if (current_max > t_max) {
					t_max = current_max;
				}
				if (t_min > t_max) {
					return ZeroVector();
				}
			}
			Vector8 factor_splatted(t_min);
			Vector8 mask = result_functor(factor_splatted);
			// Don't check here to see if the mask is accepted or not - the operations are cheap
			// And not worth branching
			output = Fmadd(line_origin, line_direction_normalized, factor_splatted);
			output = PerLaneBlend<0, 1, 2, 7>(output, factor_splatted);
			return mask;
		}

		//// Direction needs to be normalized beforehand. The intersection_point will contain in the first 3 components 
		//// the position of the point and in the 4th component the factor interpolation factor t that retrieves the point
		//// along the ray. Returns true if there is a valid intersection point, else false
		//template<typename ReturnType, typename VectorType, typename ResultFunctor>
		//ECS_INLINE ReturnType ECS_VECTORCALL IntersectRaySphere(
		//	VectorType point, 
		//	VectorType direction, 
		//	VectorType sphere_center, 
		//	float radius,
		//	VectorType intersection_point,
		//	ResultFunctor&& result_functor
		//) {
		//	// We need to solve the quadratic equation
		//	// t^2 + 2bt + c = 0
		//	// Where b = Dot3(displacement, direction)
		//	// and c = Dot3(displacement, displacement) - r^2
		//	// We also need to take into consideration some edge cases

		//	VectorType displacement = point - sphere_center;
		//	float b = Dot3(displacement, displacement).First();
		//}

	}

	Vector8 ECS_VECTORCALL IntersectSegmentPlaneMask(Vector8 segment_a, Vector8 segment_b, Plane plane, Vector8& output) {
		return SIMDHelpers::IntersectLinePlane(
			segment_a, segment_b - segment_a, plane, output, [](Vector8 t_simd) {
				return IsInRangeMask<true, true>(t_simd, ZeroVector(), VectorGlobals::ONE);
			}
		);
	}

	Vector8 ECS_VECTORCALL IntersectRayPlaneMask(Vector8 ray_origin, Vector8 ray_direction, Plane plane, Vector8& output) {
		return SIMDHelpers::IntersectLinePlane(
			ray_origin, ray_direction, plane, output, [](Vector8 t_simd) {
				return t_simd >= ZeroVector();
			}
		);
	}

	Vector8 ECS_VECTORCALL IntersectLinePlaneMask(Vector8 line_direction, Plane plane, Vector8& output) {
		// The origin doesn't matter here (mostly)
		return SIMDHelpers::IntersectLinePlane(ZeroVector(), line_direction, plane, output, [](Vector8 t_simd) {
			// When the line and the plane are parallel, the t value will be very large because of the division by 0
			return abs(t_simd) < Vector8(INTERSECT_INTERPOLATION_THRESHOLD);
		});
	}

	Vector8 ECS_VECTORCALL IntersectSegmentAABBMask(Vector8 segment_a, Vector8 segment_b, AABB aabb, Vector8& output)
	{
		return SIMDHelpers::IntersectLineAABB(
			segment_a, segment_b - segment_a, aabb, output, [](Vector8 t_simd) {
				return IsInRangeMask<true, true>(t_simd, ZeroVector(), VectorGlobals::ONE);
			}
		);
	}

	Vector8 ECS_VECTORCALL IntersectRayAABBMask(Vector8 ray_origin, Vector8 ray_direction, AABB aabb, Vector8& output)
	{
		return SIMDHelpers::IntersectLineAABB(
			ray_origin, ray_direction, aabb, output, [](Vector8 t_simd) {
				return t_simd >= ZeroVector();
			}
		);
	}

	Vector8 ECS_VECTORCALL IntersectLineAABBMask(Vector8 line_direction_normalized, AABB aabb, Vector8& output)
	{
		// The origin doesn't matter here (mostly)
		return SIMDHelpers::IntersectLineAABB(ZeroVector(), line_direction_normalized, aabb, output, [](Vector8 t_simd) {
			// When the line and the plane are parallel, the t value will be very large because of the division by 0
			return abs(t_simd) < Vector8(INTERSECT_INTERPOLATION_THRESHOLD);
		});
	}

}