#include "ecspch.h"
#include "Intersection.h"

#define INTERSECT_INTERPOLATION_THRESHOLD 1000000.0f

namespace ECSEngine {

	// Can be used for line tests, ray tests (semi-lines) or segment tests
	// Returns a mask that can be used to perform other operations
	template<typename MaskType, typename Vector, typename Plane, typename TType, typename ResultFunctor>
	ECS_INLINE MaskType ECS_VECTORCALL IntersectLinePlaneImpl(
		Vector line_origin,
		Vector line_direction,
		Plane plane,
		Vector& output_point,
		TType& t_point,
		ResultFunctor&& result_functor
	) {
		// t = (plane.dot - Dot(plane.normal, segment_a)) / Dot(plane.normal, segment_direction)
		TType t = (plane.dot - Dot(plane.normal, line_origin)) / Dot(plane.normal, line_direction);
		auto mask = result_functor(t);
		output_point = Fmadd(Vector::Splat(t), line_direction, line_origin);
		t_point = t;
		return mask;
	}

	bool IntersectSegmentPlane(float3 segment_a, float3 segment_b, PlaneScalar plane, float3& point_output, float& t_output) {
		return IntersectLinePlaneImpl<bool>(
			segment_a, segment_b - segment_a, plane, point_output, t_output, [](float t_simd) {
				return IsInRangeMaskSingle<true, true>(t_simd, 0.0f, 1.0f);
			}
		);
	}

	SIMDVectorMask ECS_VECTORCALL IntersectSegmentPlane(Vector3 segment_a, Vector3 segment_b, Plane plane, Vector3& point_output, Vec8f& t_output) {
		return IntersectLinePlaneImpl<SIMDVectorMask>(
			segment_a, segment_b - segment_a, plane, point_output, t_output, [](Vec8f t_simd) {
				return IsInRangeMaskSingle<true, true>(t_simd, Vec8f(ZeroVectorFloat()), VectorGlobals::ONE);
			}
		);
	}

	bool IntersectRayPlane(float3 ray_origin, float3 ray_direction, PlaneScalar plane, float3& point_output, float& t_output) {
		return IntersectLinePlaneImpl<bool>(
			ray_origin, ray_direction, plane, point_output, t_output, [](float t_simd) {
				return t_simd >= 0.0f;
			}
		);
	}

	SIMDVectorMask ECS_VECTORCALL IntersectRayPlane(Vector3 ray_origin, Vector3 ray_direction, Plane plane, Vector3& point_output, Vec8f& t_output) {
		return IntersectLinePlaneImpl<SIMDVectorMask>(
			ray_origin, ray_direction, plane, point_output, t_output, [](Vec8f t_simd) {
				return t_simd >= ZeroVectorFloat();
			}
		);
	}

	// The implementation is made like this to look alike the SIMD one
	bool IntersectRayAABB(
		float3 ray_origin,
		float3 ray_direction,
		AABBScalar aabb,
		float3& intersection_point,
		float& interpolation_factor_t
	) {
		float t_min = 0.0f;
		float t_max = FLT_MAX;

		float3 direction_inverse = float3::Splat(1.0f) / ray_direction;
		bool3 signs;
		signs.x = signbit(direction_inverse.x);
		signs.y = signbit(direction_inverse.y);
		signs.z = signbit(direction_inverse.z);

		for (size_t index = 0; index < 3; index++) {
			float current_min = aabb[signs[index]][index];
			float current_max = aabb[!signs[index]][index];

			float direction_min = (current_min - ray_origin[index]) * direction_inverse[index];
			float direction_max = (current_min - ray_origin[index]) * direction_inverse[index];

			t_min = max(direction_min, t_min);
			t_max = min(direction_max, t_max);
		}

		if (t_min <= t_max) {
			interpolation_factor_t = t_min;
			intersection_point = Fmadd(float3::Splat(t_min), ray_direction, ray_origin);
			return true;
		}
		return false;
	}

	IntersectAABBPrepareData ECS_VECTORCALL IntersectAABBPrepareIterate(float3 origin, float3 direction) {
		IntersectAABBPrepareData result;

		result.origin = Vector3().Splat(origin);
		result.direction = Vector3().Splat(direction);
		result.inverse_direction = Vector3::Splat(VectorGlobals::ONE) / result.direction;
		result.signs = { signbit(direction.x), signbit(direction.y), signbit(direction.z) };

		return result;
	}

	SIMDVectorMask ECS_VECTORCALL IntersectRayAABB(
		float3 ray_origin,
		float3 ray_direction,
		AABB aabb,
		Vector3& point_output,
		Vec8f& t_output
	) {
		IntersectAABBPrepareData prepare_data = IntersectAABBPrepareIterate(ray_origin, ray_direction);
		return IntersectRayAABBIterate(&prepare_data, aabb, point_output, t_output);
	}

	SIMDVectorMask ECS_VECTORCALL IntersectRayAABBIterate(
		const IntersectAABBPrepareData* prepare_data,
		AABB aabb,
		Vector3& point_output,
		Vec8f& t_output
	) {
		Vec8f t_min = ZeroVectorFloat();
		Vec8f t_max = FLT_MAX;

		for (size_t index = 0; index < 3; index++) {
			Vec8f current_min = aabb[prepare_data->signs[index]][index];
			Vec8f current_max = aabb[!prepare_data->signs[index]][index];

			Vec8f direction_min = (current_min - prepare_data->origin[index]) * prepare_data->inverse_direction[index];
			Vec8f direction_max = (current_min - prepare_data->origin[index]) * prepare_data->inverse_direction[index];

			t_min = max(direction_min, t_min);
			t_max = min(direction_max, t_max);
		}

		SIMDVectorMask mask = t_min <= t_max;
		t_output = t_min;
		point_output = Fmadd(Vector3::Splat(t_min), prepare_data->direction, prepare_data->origin);
		return mask;
	}

	void ClipSegmentAgainstPlane(
		const Plane& plane,
		const Vector3& segment_a,
		const Vector3& normalized_direction,
		const Vec8f& segment_t_factor,
		Vec8f& segment_t_min,
		Vec8f& segment_t_max,
		float parallel_epsilon
	) {
		Vec8f epsilon = parallel_epsilon;
		
		// Instead of using the IntersectSegmentPlane function, we can write
		// The same function here but more efficiently since some parameters
		// Can be shared or some calculations omitted

		// t = (plane.dot - Dot(plane.normal, segment_a)) / Dot(plane.normal, segment_direction)
		// The plane and the segment are parallel if the denominator is close to 0.0f
		// We need to normalize the segment_direction tho for consistent checks
		Vec8f denominator = Dot(plane.normal, normalized_direction);
		Vec8f zero_vector = SingleZeroVector<Vec8f>();
		// We will use this mask later on to mask the results
		SIMDVectorMask is_parallel_mask = CompareMaskSingle<Vec8f>(denominator, zero_vector, epsilon);

		Vec8f distance_to_plane = plane.dot - Dot(plane.normal, segment_a);
		Vec8f t = distance_to_plane / denominator;
		// Multiply by the factor in order to have the accurate value, otherwise
		// We will have the t value according to the normalized magnitude
		t *= segment_t_factor;
		// If the denominator is less than 0.0f, the plane faces the segment and we must
		// Update the t_min. Else we must update t_max

		SIMDVectorMask is_negative = denominator < zero_vector;
		segment_t_min = SelectSingle(is_negative, Max(segment_t_min, t), segment_t_min);
		SIMDVectorMask is_positive = denominator > zero_vector;
		segment_t_max = SelectSingle(is_positive, Min(segment_t_max, t), segment_t_max);

		// There is one more thing to do. If the line is parallel, the segment
		// Should be clipped, basically 0.0f. We can do this by setting segment_t_min
		// To FLT_MAX or segment_t_max to -FLT_MAX
		Vec8f flt_max = FLT_MAX;
		segment_t_min = SelectSingle(is_parallel_mask, flt_max, segment_t_min);
	}

	bool ClipSegmentAgainstPlane(
		const PlaneScalar& plane,
		const float3& segment_a,
		const float3& normalized_direction,
		float segment_t_factor,
		float& segment_t_min,
		float& segment_t_max,
		float parallel_epsilon
	) {
		// We have a different version from the SIMD one since we can ifs in this case
		// For how this works, look at the SIMD version
		float denominator = Dot(plane.normal, normalized_direction);
		float distance = plane.dot - Dot(plane.normal, segment_a);

		if (CompareMaskSingle(denominator, 0.0f, parallel_epsilon)) {
			// If the line is not contained by the plane and on the
			// other side of the plane, we can exit
			if (distance < -0.00000001f) {
				segment_t_min = FLT_MAX;
				return false;
			}
		}
		else {
			float t = distance / denominator;
			t *= segment_t_factor;
			if (denominator < 0.0f) {
				segment_t_min = max(segment_t_min, t);
			}
			else {
				segment_t_max = min(segment_t_max, t);
			}
		}

		return segment_t_min < segment_t_max;
	}

}