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

			t_min = std::max(direction_min, t_min);
			t_max = std::min(direction_max, t_max);
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

}