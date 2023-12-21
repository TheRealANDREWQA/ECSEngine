#include "ecspch.h"
#include "Vector.h"

namespace ECSEngine {

#define EXPORT_TEMPLATE_PRECISION_LANE_WIDTH(return_value, function_name, ...) \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_PRECISE, false>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_PRECISE, true>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_ACCURATE, false>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_ACCURATE, true>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_FAST, false>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_FAST, true>(__VA_ARGS__);

#define EXPORT_TEMPLATE_PRECISION(return_value, function_name, ...) \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_PRECISE>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_ACCURATE>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_FAST>(__VA_ARGS__);
	

	namespace VectorGlobals {
		Vector8 ONE = Vector8(1.0f);
		Vector8 INFINITY_MASK = Vector8(0x7F800000);
		Vector8 SIGN_MASK = Vector8(0x7FFFFFFF);
		Vector8 EPSILON = Vector8(ECS_SIMD_VECTOR_EPSILON_VALUE);
	}

	// --------------------------------------------------------------------------------------------------------------

	void Vector8::StoreFloat3(float3* low, float3* high) const
	{
		// Store into a temp stack buffer the entire value such that
		// We don't have to perform multiple slower moves and some shifts
		float4 values[2];
		Store(values);

		*low = values[0].xyz();
		*high = values[1].xyz();
	}

	float3 Vector8::AsFloat3Low() const {
		// Store into a float4 since it will allow to issue a store
		// intrinsic instead of having to do some shifting and 2 moves
		float4 result;
		StorePartialConstant<4, true>(&result);
		return result.xyz();
	}

	float3 Vector8::AsFloat3High() const {
		// Store into a float4 since it will allow to issue a store
		// intrinsic instead of having to do some shifting and 2 moves
		float4 result;
		StorePartialConstant<4, false>(&result);
		return result.xyz();
	}

	float4 Vector8::AsFloat4Low() const {
		float4 result;
		StorePartialConstant<4, true>(&result);
		return result;
	}

	float4 Vector8::AsFloat4High() const {
		float4 result;
		StorePartialConstant<4, false>(&result);
		return result;
	}

	float Vector8::Last() const {
		return _mm_cvtss_f32(_mm256_castps256_ps128(permute8<3, V_DC, V_DC, V_DC, 7, V_DC, V_DC, V_DC>(value)));
	}

	float Vector8::LastHigh() const {
		return _mm_cvtss_f32(permute4<3, V_DC, V_DC, V_DC>(value.get_high()));
	}

	float2 Vector8::GetLasts() const {
		Vec8f permuted = permute8<3, V_DC, V_DC, V_DC, 7, V_DC, V_DC, V_DC>(value);
		return { _mm_cvtss_f32(_mm256_castps256_ps128(permuted)), _mm_cvtss_f32(permuted.get_high()) };
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL DegToRad(Vector8 angles) {
		Vector8 factor(DEG_TO_RAD_FACTOR);
		return angles * factor;
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL RadToDeg(Vector8 angles) {
		Vector8 factor(RAD_TO_DEG_FACTOR);
		return angles * factor;
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL ZeroVector() {
		return Vec8f(_mm256_setzero_ps());
	}

	Vector8 ECS_VECTORCALL RightVector() {
		return PerLaneBlend<4, 1, 2, 3>(ZeroVector(), VectorGlobals::ONE);
	}

	Vector8 ECS_VECTORCALL UpVector() {
		return PerLaneBlend<0, 5, 2, 3>(ZeroVector(), VectorGlobals::ONE);
	}

	Vector8 ECS_VECTORCALL ForwardVector() {
		return PerLaneBlend<0, 1, 6, 3>(ZeroVector(), VectorGlobals::ONE);
	}

	Vector8 ECS_VECTORCALL LastElementOneVector() {
		return PerLaneBlend<0, 1, 2, 7>(ZeroVector(), VectorGlobals::ONE);
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL IsInfiniteMask(Vector8 vector) {
		vector.value &= VectorGlobals::SIGN_MASK;
		return vector.value == VectorGlobals::INFINITY_MASK.value;
	}

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL Dot(Vector8 first, Vector8 second) {
		// It seems that the dot implementation might be slower than the
		// actual shuffles and add/mul
		Vector8 multiplication = first * second;
		Vector8 x = PerLaneBroadcast<0>(multiplication);
		Vector8 y = PerLaneBroadcast<1>(multiplication);
		Vector8 z = PerLaneBroadcast<2>(multiplication);
		if constexpr (use_full_lane) {
			Vector8 w = PerLaneBroadcast<3>(multiplication);
			return x + y + z + w;
		}
		else {
			return x + y + z;
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, Dot, Vector8, Vector8);

	float Dot(float2 first, float2 second) {
		return first.x * second.x + first.y * second.y;
	}

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL CompareMask(Vector8 first, Vector8 second, Vector8 epsilon) {
		Vector8 zero_vector = ZeroVector();
		Vector8 absolute_difference = abs(first - second);
		auto comparison = absolute_difference < epsilon;
		return Vector8(comparison);
	}
	
	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, CompareMask, Vector8, Vector8, Vector8);

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL CompareAngleNormalizedRadMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians) {
		Vector8 angle_cosine = cos(radians);
		Vector8 direction_cosine = abs(Dot<use_full_lane>(first_normalized, second_normalized));
		return direction_cosine > angle_cosine;
	}
	
	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, CompareAngleNormalizedRadMask, Vector8, Vector8, Vector8);

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL CompareAngleRadMask(Vector8 first, Vector8 second, Vector8 radians) {
		return CompareAngleNormalizedRadMask<use_full_lane>(Normalize<use_full_lane>(first), Normalize<use_full_lane>(second), radians);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, CompareAngleRadMask, Vector8, Vector8, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL ProjectPointOnLineDirection(Vector8 line_point, Vector8 line_direction, Vector8 point) {
		// Formula A + dot(AP,line_direction) / dot(line_direction, line_direction) * line_direction
		// Where A is a point on the line and P is the point that we want to project
		return line_point + Dot<use_full_lane>(point - line_point, line_direction) / Dot<use_full_lane>(line_direction, line_direction) * line_direction;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, ProjectPointOnLineDirection, Vector8, Vector8, Vector8);

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL ProjectPointOnLineDirectionNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point) {
		// In this version we don't need to divide by the magnitude of the line direction
		// Formula A + dot(AP,line_direction) * line_direction
		// Where A is a point on the line and P is the point that we want to project
		return line_point + Dot<use_full_lane>(point - line_point, line_direction_normalized) * line_direction_normalized;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, ProjectPointOnLineDirectionNormalized, Vector8, Vector8, Vector8);

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL ProjectPointOnLine(Vector8 line_a, Vector8 line_b, Vector8 point) {
		Vector8 line_direction = line_b - line_a;
		return ProjectPointOnLineDirection<use_full_lane>(line_a, line_direction, point);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, ProjectPointOnLine, Vector8, Vector8, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	// Cross for 3 element wide register
	Vector8 ECS_VECTORCALL Cross(Vector8 a, Vector8 b) {
		// v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x
		Vector8 vector1_yzx = PerLanePermute<1, 2, 0, V_DC>(a);
		Vector8 vector1_zxy = PerLanePermute<2, 0, 1, V_DC>(a);
		Vector8 vector2_zxy = PerLanePermute<2, 0, 1, V_DC>(b);
		Vector8 vector2_yzx = PerLanePermute<1, 2, 0, V_DC>(b);

		Vector8 second_multiply = vector1_zxy * vector2_yzx;
		return Fmsub(vector1_yzx, vector2_zxy, second_multiply);
	}

	// --------------------------------------------------------------------------------------------------------------

	float SquareLength(float3 vector) {
		return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
	}

	float SquareLength(float2 vector) {
		return vector.x * vector.x + vector.y * vector.y;
	}

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL Length(Vector8 vector) {
		return sqrt(SquareLength<use_full_lane>(vector));
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, Length, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision, bool use_full_lane>
	Vector8 ECS_VECTORCALL ReciprocalLength(Vector8 vector) {
		Vector8 square_length = SquareLength<use_full_lane>(vector);
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			return VectorGlobals::ONE / Vector8(sqrt(square_length));
		}
		else if constexpr (precision == ECS_VECTOR_ACCURATE) {
			return approx_recipr(sqrt(square_length));
		}
		else if constexpr (precision == ECS_VECTOR_FAST) {
			return approx_rsqrt(square_length);
		}
		else {
			static_assert(false);
		}
	}

	EXPORT_TEMPLATE_PRECISION_LANE_WIDTH(Vector8, ReciprocalLength, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL SquaredDistanceToLine(Vector8 line_point, Vector8 line_direction, Vector8 point) {
		Vector8 projected_point = ProjectPointOnLineDirection<use_full_lane>(line_point, line_direction, point);
		return SquareLength<use_full_lane>(projected_point - point);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, SquaredDistanceToLine, Vector8, Vector8, Vector8);

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL SquaredDistanceToLineNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point) {
		Vector8 projected_point = ProjectPointOnLineDirectionNormalized<use_full_lane>(line_point, line_direction_normalized, point);
		return SquareLength<use_full_lane>(projected_point - point);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, SquaredDistanceToLineNormalized, Vector8, Vector8, Vector8);

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL DistanceToLine(Vector8 line_point, Vector8 line_direction, Vector8 point) {
		return sqrt(SquaredDistanceToLine<use_full_lane>(line_point, line_direction, point));
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, DistanceToLine, Vector8, Vector8, Vector8);

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL DistanceToLineNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point) {
		return sqrt(SquaredDistanceToLineNormalized<use_full_lane>(line_point, line_direction_normalized, point));
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, DistanceToLineNormalized, Vector8, Vector8, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	Vector8 ECS_VECTORCALL Normalize(Vector8 vector) {
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			// Don't use reciprocal length so as to not add an extra read to the
			// VectorGlobals::ONE and then a multiplication
			return vector / Length<use_full_lane>(vector);
		}
		else {
			return vector * ReciprocalLength<precision, use_full_lane>(vector);
		}
	}

	EXPORT_TEMPLATE_PRECISION_LANE_WIDTH(Vector8, Normalize, Vector8);

	float3 Normalize(float3 vector) {
		Vector8 simd_vector(vector);
		simd_vector = Normalize<ECS_VECTOR_PRECISE>(simd_vector);
		return simd_vector.AsFloat3Low();
	}

	// Add the float2 variant as well
	float2 Normalize(float2 vector) {
		return vector * float2::Splat(1.0f / Length(vector));
	}

	// The vector needs to have 2 packed float2
	// It will normalize both at the same time
	Vector8 Normalize2x2(Vector8 vector) {
		Vector8 multiplication = vector * vector;
		Vector8 shuffle = PerLanePermute<1, 0, 3, 2>(multiplication);

		Vector8 squared_length = shuffle + multiplication;
		Vector8 length = sqrt(squared_length);
		return vector / length;
	}

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL IsNormalizedSquareLengthMask(Vector8 squared_length) {
		Vector8 one = VectorGlobals::ONE;
		return CompareMask<use_full_lane>(squared_length, one);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, IsNormalizedSquareLengthMask, Vector8);

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL IsNormalizedMask(Vector8 vector) {
		Vector8 squared_length = SquareLength<use_full_lane>(vector);
		return IsNormalizedSquareLengthMask<use_full_lane>(squared_length);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, IsNormalizedMask, Vector8);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	Vector8 ECS_VECTORCALL NormalizeIfNot(Vector8 vector) {
		if constexpr (precision == ECS_VECTOR_PRECISE || precision == ECS_VECTOR_ACCURATE) {
			Vector8 square_length = SquareLength<use_full_lane>(vector);
			if (!IsNormalizedSquareLengthWhole(square_length)) {
				if constexpr (precision == ECS_VECTOR_PRECISE) {
					return vector / Vector8(sqrt(square_length));
				}
				else {
					return vector * Vector8(approx_recipr(square_length));
				}
			}
			else {
				return vector;
			}
		}
		else {
			return Normalize<ECS_VECTOR_FAST, use_full_lane>(vector);
		}
	}

	EXPORT_TEMPLATE_PRECISION_LANE_WIDTH(Vector8, NormalizeIfNot, Vector8);

	template<VectorOperationPrecision precision>
	Vector8 ECS_VECTORCALL OneDividedVector(Vector8 value) {
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			return VectorGlobals::ONE / value;
		}
		else {
			return approx_recipr(value);
		}
	}

	EXPORT_TEMPLATE_PRECISION(Vector8, OneDividedVector, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL IsParallelMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon) {
		return CompareMask(first_normalized, second_normalized, epsilon);
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL IsParallelAngleMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians) {
		return CompareAngleNormalizedRadMask(first_normalized, second_normalized, radians);
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL IsPerpendicularMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon) {
		return CompareMask(Dot(first_normalized, second_normalized), ZeroVector(), epsilon);
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL IsPerpendicularAngleMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians) {
		Vector8 half_pi = PI / 2;
		return Vector8(Vec8fb(Not(CompareAngleNormalizedRadMask(first_normalized, second_normalized, half_pi - radians))));
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL Clamp(Vector8 value, Vector8 min, Vector8 max) {
		auto is_smaller = value < min;
		auto is_greater = value > max;
		return select(is_greater, max, select(is_smaller, min, value));
	}

	template<bool equal_min = false, bool equal_max = false>
	Vector8 ECS_VECTORCALL IsInRangeMask(Vector8 value, Vector8 min, Vector8 max) {
		Vector8 is_lower, is_greater;

		if constexpr (equal_min) {
			is_lower = value >= min;
		}
		else {
			is_lower = value > min;
		}

		if constexpr (equal_max) {
			is_greater = value <= max;
		}
		else {
			is_greater = value < max;
		}

		return is_lower.AsMask() && is_greater.AsMask();
	}

	ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL(Vector8, IsInRangeMask, Vector8, Vector8, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL Reflect(Vector8 incident, Vector8 normal) {
		// result = incident - (2 * Dot(incident, normal)) * normal
		Vector8 dot = Dot<use_full_lane>(incident, normal);
		return incident - Vector8(((dot + dot) * normal));
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, Reflect, Vector8, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<bool is_low_only, bool use_full_lane = false>
		Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
			// result = refraction_index * incident - normal * (refraction_index * Dot(incident, normal) +
			// sqrt(1 - refraction_index * refraction_index * (1 - Dot(incident, normal) * Dot(incident, normal))))

			Vector8 dot = Dot<use_full_lane>(incident, normal);
			Vector8 one = VectorGlobals::ONE;
			Vector8 zero = ZeroVector();
			Vector8 sqrt_dot_factor = one - dot * dot;
			Vector8 refraction_index_squared = refraction_index * refraction_index;

			Vector8 sqrt_value = one - refraction_index_squared * sqrt_dot_factor;
			auto is_less_or_zero = sqrt_value <= zero;

			Vector8 result = sqrt_value;

			int mask = _mm256_movemask_ps(is_less_or_zero);
			if constexpr (is_low_only) {
				if (mask == 0x0F) {
					return zero;
				}
			}

			// total reflection
			if (mask == 0xFF) {
				return zero;
			}
			else {
				result = Fmsub(refraction_index, incident, normal * Vector8(Fmadd(refraction_index, dot, Vector8(sqrt(result)))));
				if (mask == 0xF0) {
					result = Vector8(zero.value.get_low(), result.value.get_high());
				}
				else if (mask == 0x0F) {
					result = Vector8(result.value.get_low(), zero.value.get_low());
				}
				return result;
			}
		}
	}

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
		return SIMDHelpers::Refract<false, use_full_lane>(incident, normal, refraction_index);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, Refract, Vector8, Vector8, Vector8);

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL RefractLow(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
		return SIMDHelpers::Refract<true, use_full_lane>(incident, normal, refraction_index);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, RefractLow, Vector8, Vector8, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL AngleBetweenVectorsNormalizedRad(Vector8 a_normalized, Vector8 b_normalized) {
		// Just take the cosine (dot) between them and return the acos result
		Vector8 dot = Dot<use_full_lane>(a_normalized, b_normalized);
		return acos(dot);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, AngleBetweenVectorsNormalizedRad, Vector8, Vector8);

	// Returns the value of the angle between the 2 vectors in radians
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	Vector8 ECS_VECTORCALL AngleBetweenVectorsRad(Vector8 a, Vector8 b) {
		return AngleBetweenVectorsNormalizedRad(Normalize<precision, use_full_lane>(a), Normalize<precision, use_full_lane>(b));
	}

	EXPORT_TEMPLATE_PRECISION_LANE_WIDTH(Vector8, AngleBetweenVectorsRad, Vector8, Vector8);

	// Return the value of the angle between the 2 vectors in degrees
	template<bool use_full_lane = false>
	Vector8 ECS_VECTORCALL AngleBetweenVectorsNormalized(Vector8 a_normalized, Vector8 b_normalized) {
		return RadToDeg(AngleBetweenVectorsNormalizedRad<use_full_lane>(a_normalized, b_normalized));
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Vector8, AngleBetweenVectorsNormalized, Vector8, Vector8);

	// Return the value of the angle between the 2 vectors in degrees
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	Vector8 ECS_VECTORCALL AngleBetweenVectors(Vector8 a, Vector8 b) {
		return RadToDeg(AngleBetweenVectorsRad<precision, use_full_lane>(a, b));
	}

	EXPORT_TEMPLATE_PRECISION_LANE_WIDTH(Vector8, AngleBetweenVectors, Vector8, Vector8);

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL VectorFmod(Vector8 x, Vector8 y) {
		// Formula x - truncate(x / y) * y;

		// Fmadd version might have better precision but slightly slower because of the _mm_set1_ps
		//return Fmadd(truncate(x / y), -y, x);

		// Cannot use approximate reciprocal of y and multiply instead of division
		// since the error is big enough that it will break certain use cases
		return x - Vector8(truncate(x / y) * y);
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL DirectionToRotationEulerRad(Vector8 direction) {
		/* Scalar algorithm
		float x_angle = fmod(atan2(direction.y, direction.z), PI);
		float y_angle = fmod(atan2(direction.z, direction.x), PI);
		float z_angle = fmod(atan2(direction.y, direction.x), PI);*/

		Vector8 tangent_numerator = PerLanePermute<1, 2, 1, V_DC>(direction);
		Vector8 tangent_denominator = PerLanePermute<2, 0, 0, V_DC>(direction);

		Vector8 radians = atan2(tangent_numerator, tangent_denominator);
		Vector8 pi = Vector8(PI);
		radians = VectorFmod(radians, pi);

		// It can happen that atan2 is unstable when values are really small and returns 
		// something close to PI when it should be 0
		// Handle this case - we need a slightly larger epsilon
		Vector8 close_to_pi_mask = CompareMask(radians, pi, Vector8(0.001f));
		Vector8 zero_vector = ZeroVector();
		return Select(close_to_pi_mask, zero_vector, radians);
	}

	// Returns the euler angles in radians that correspond to that direction
	float3 DirectionToRotationEulerRad(float3 direction) {
		Vector8 vector_dir(direction);
		vector_dir = DirectionToRotationEuler(vector_dir);
		return vector_dir.AsFloat3Low();
	}

	// --------------------------------------------------------------------------------------------------------------

	// Constructs an orthonormal basis out of a given direction
	void ECS_VECTORCALL OrthonormalBasis(Vector8 direction_normalized, Vector8* direction_first, Vector8* direction_second) {
		/*
		*   Scalar algorithm - Credit to Building an Orthonormal Basis, Revisited - 2017
			if(n.z<0.){
				const float a = 1.0f / (1.0f - n.z);
				const float b = n.x * n.y * a;
				b1 = Vec3f(1.0f - n.x * n.x * a, -b, n.x);
				b2 = Vec3f(b, n.y * n.y*a - 1.0f, -n.y);
			}
			else{
				const float a = 1.0f / (1.0f + n.z);
				const float b = -n.x * n.y * a;
				b1 = Vec3f(1.0f - n.x * n.x * a, b, -n.x);
				b2 = Vec3f(b, 1.0f - n.y * n.y * a, -n.y);
			}
		*/

		Vector8 negative_z = direction_normalized < ZeroVector();
		Vector8 splatted_x = PerLaneBroadcast<0>(direction_normalized);
		Vector8 splatted_y = PerLaneBroadcast<1>(direction_normalized);
		Vector8 splatted_z = PerLaneBroadcast<2>(direction_normalized);
		Vector8 xx = splatted_x * splatted_x;
		Vector8 yy = splatted_y * splatted_y;
		Vector8 xy = splatted_x * splatted_y;
		negative_z = PerLaneBroadcast<2>(negative_z);

		Vector8 one = VectorGlobals::ONE;
		Vector8 first_a = one / (one + splatted_z);
		Vector8 second_a = one / (one - splatted_z);

		Vector8 a = Select(negative_z, first_a, second_a);
		Vector8 b = xy * a;
		Vector8 x_factor = one - xx * a;
		Vector8 y_factor = one - yy * a;

		Vector8 a_result_xy = PerLaneBlend<0, 5, V_DC, V_DC>(x_factor, b);
		Vector8 a_result = PerLaneBlend<0, 1, 6, V_DC>(a_result_xy, splatted_x);
		Vector8 b_result_xy = PerLaneBlend<4, 1, V_DC, V_DC>(y_factor, b);
		Vector8 b_result = PerLaneBlend<0, 1, 6, V_DC>(b_result_xy, splatted_y);

		Vector8 negative_a_sign_mask = PerLaneChangeSignMask<0, 1, 0, 0, Vector8>();
		Vector8 positive_a_sign_mask = PerLaneChangeSignMask<0, 0, 1, 0, Vector8>();
		Vector8 negative_b_sign_mask = PerLaneChangeSignMask<0, 0, 1, 0, Vector8>();
		Vector8 positive_b_sign_mask = PerLaneChangeSignMask<0, 1, 1, 0, Vector8>();

		Vector8 a_mask = Select(negative_z, negative_a_sign_mask, positive_a_sign_mask);
		Vector8 b_mask = Select(negative_z, negative_b_sign_mask, positive_b_sign_mask);

		*direction_first = PerLaneChangeSign(a_result, a_mask);
		*direction_second = PerLaneChangeSign(b_result, b_mask);
	}

	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {

		Vector8 ECS_VECTORCALL CullClipSpaceXMask(Vector8 vector, Vector8 splatted_w, Vector8 negative_splatted_w) {
			Vector8 splatted_x = PerLaneBroadcast<0>(vector);
			Vector8 x_greater = negative_splatted_w <= splatted_x;
			Vector8 x_smaller = splatted_x <= splatted_w;
			return x_greater.value & x_smaller.value;
		}

		Vector8 ECS_VECTORCALL CullClipSpaceYMask(Vector8 vector, Vector8 splatted_w, Vector8 negative_splatted_w) {
			Vector8 splatted_y = PerLaneBroadcast<1>(vector);
			Vector8 y_greater = negative_splatted_w <= splatted_y;
			Vector8 y_smaller = splatted_y <= splatted_w;
			return y_greater.value & y_smaller.value;
		}

		Vector8 ECS_VECTORCALL CullClipSpaceZMask(Vector8 vector, Vector8 splatted_w) {
			Vector8 splatted_z = PerLaneBroadcast<2>(vector);
			Vector8 z_greater = ZeroVector() <= splatted_z;
			Vector8 z_smaller = splatted_z <= splatted_w;
			return z_greater.value & z_greater.value;
		}

	}

	Vector8 ECS_VECTORCALL CullClipSpaceXMask(Vector8 vector) {
		Vector8 splatted_w = PerLaneBroadcast<3>(vector);
		return SIMDHelpers::CullClipSpaceXMask(vector, splatted_w, -splatted_w);
	}

	Vector8 ECS_VECTORCALL CullClipSpaceYMask(Vector8 vector) {
		Vector8 splatted_w = PerLaneBroadcast<3>(vector);
		return SIMDHelpers::CullClipSpaceYMask(vector, splatted_w, -splatted_w);
	}

	Vector8 ECS_VECTORCALL CullClipSpaceZMask(Vector8 vector) {
		return SIMDHelpers::CullClipSpaceZMask(vector, PerLaneBroadcast<3>(vector));
	}

	Vector8 ECS_VECTORCALL CullClipSpaceWMask(Vector8 vector) {
		Vector8 w_mask = vector >= ZeroVector();
		Vector8 splatted_w_mask = PerLaneBroadcast<3>(w_mask);
		return splatted_w_mask;
	}

	// Returns a mask where vertices which should be kept have their lane set to true
	Vector8 ECS_VECTORCALL CullClipSpaceMask(Vector8 vector) {
		// The entire conditions are like this
		/*
			w > 0
			-w <= x <= w
			-w <= y <= w
			0 <= z <= w
		*/

		// Check the w first. If it is negative, than it's behind the camera
		Vector8 w_mask = CullClipSpaceWMask(vector);
		// Both w are ngative, can early exit
		if (!horizontal_or(Vec8fb(w_mask.value))) {
			return ZeroVector();
		}

		Vector8 current_mask = w_mask;

		// Continue the testing with the X condition
		Vector8 splatted_w = PerLaneBroadcast<3>(vector);
		Vector8 negative_splatted_w = -splatted_w;

		current_mask.value &= SIMDHelpers::CullClipSpaceXMask(vector, splatted_w, negative_splatted_w);
		// If both tests have failed up until now we can exit
		if (!horizontal_or(Vec8fb(current_mask.value))) {
			return ZeroVector();
		}

		current_mask.value &= SIMDHelpers::CullClipSpaceYMask(vector, splatted_w, negative_splatted_w);
		if (!horizontal_or(Vec8fb(current_mask.value))) {
			return ZeroVector();
		}

		current_mask.value &= SIMDHelpers::CullClipSpaceZMask(vector, splatted_w);
		return current_mask;
	}

	// Converts from clip space to NDC
	Vector8 ECS_VECTORCALL ClipSpaceToNDC(Vector8 vector) {
		// We just need to divide xyz / w, but here we also divide w by w
		// so as to not add another masking instruction
		return vector / PerLaneBroadcast<3>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL GetRightVectorForDirection(Vector8 direction_normalized) {
		// Compute the cross product between the world up and the given direction
		// If they are parallel, then the right vector is the global right vector if the direction
		// is positive, else the negative right vector if the direction is negative

		Vector8 world_up = UpVector();
		Vector8 is_parallel_to_up = IsParallelMask(direction_normalized, world_up);

		// This is the branch were we calculate the result for the case where
		// the direction is parallel to the world up
		Vector8 world_right = RightVector();
		// The direction is either (0.0f, 1.0f, 0.0f) or (0.0f, -1.0f, 0.0f)
		// We can permute the value and the multiply with right
		Vector8 sign = PerLanePermute<1, V_DC, V_DC, V_DC>(direction_normalized);
		Vector8 parallel_result = sign * world_right;

		if (is_parallel_to_up.MaskResultWhole<3>()) {
			return parallel_result;
		}
		else {
			// Compute the cross product
			Vector8 cross_product = Cross(direction_normalized, world_up);
			cross_product = Normalize(cross_product);

			return Select(is_parallel_to_up, parallel_result, cross_product);
		}
	}

	Vector8 ECS_VECTORCALL GetUpVectorForDirection(Vector8 direction_normalized) {
		// This is the same as the the Right case, we just need to replace the
		// vectors and the permutation for the sign

		Vector8 world_right = RightVector();
		Vector8 is_parallel_to_right = IsParallelMask(direction_normalized, world_right);

		// This is the branch were we calculate the result for the case where
		// the direction is parallel to the world up
		Vector8 world_up = UpVector();
		// The direction is either (0.0f, 1.0f, 0.0f) or (0.0f, -1.0f, 0.0f)
		// We can permute the value and the multiply with right
		Vector8 sign = PerLanePermute<1, V_DC, V_DC, V_DC>(direction_normalized);
		Vector8 parallel_result = sign * world_up;

		if (is_parallel_to_right.MaskResultWhole<3>()) {
			return parallel_result;
		}
		else {
			// Compute the cross product
			Vector8 cross_product = Cross(direction_normalized, world_right);
			cross_product = Normalize(cross_product);

			return Select(is_parallel_to_right, parallel_result, cross_product);
		}
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL ClosestPoint(Stream<Vector8> points, Vector8 origin) {
		Vector8 smallest_distance = FLT_MAX;
		Vector8 closest_point;
		for (size_t index = 0; index < points.size; index++) {
			Vector8 distance = SquareLength(points[index] - origin);
			Vector8 is_closer = smallest_distance > distance;
			smallest_distance = Select(is_closer, distance, smallest_distance);
			closest_point = Select(is_closer, points[index], closest_point);
		}

		Vector8 closest_permute = Permute2f128Helper<1, 0>(closest_point, closest_point);
		Vector8 smallest_distance_permute = Permute2f128Helper<1, 0>(smallest_distance, smallest_distance);
		Vector8 permute_mask = smallest_distance_permute < smallest_distance;
		Vector8 final_point = Select(permute_mask, closest_permute, closest_point);
		return SplatLowLane(final_point);
	}

	// --------------------------------------------------------------------------------------------------------------

	bool SameQuadrantRange360(float degrees_a, float degrees_b) {
		degrees_a = degrees_a < 0.0f ? 360.0f + degrees_a : degrees_a;
		degrees_b = degrees_b < 0.0f ? 360.0f + degrees_b : degrees_b;

		// Don't include "Function.h" just for this function

		auto is_in_range = [](float val, float min, float max) {
			return val >= min && val <= max;
		};

		if (is_in_range(degrees_a, 0.0f, 90.0f)) {
			return is_in_range(degrees_b, 0.0f, 90.0f);
		}
		else if (is_in_range(degrees_a, 90.0f, 180.0f)) {
			return is_in_range(degrees_b, 90.0f, 180.0f);
		}
		else if (is_in_range(degrees_a, 180.0f, 270.0f)) {
			return is_in_range(degrees_b, 180.0f, 270.0f);
		}
		else {
			return is_in_range(degrees_b, 270.0f, 360.0f);
		}
	}

	bool SameQuadrant(float degrees_a, float degrees_b) {
		auto is_in_range = [](float val, float min, float max) {
			return val >= min && val <= max;
		};

		if (is_in_range(degrees_a, -360.0f, 360.0f) && is_in_range(degrees_b, -360.0f, 360.0f)) {
			return SameQuadrantRange360(degrees_a, degrees_b);
		}
		return SameQuadrantRange360(fmodf(degrees_a, 360.0f), fmodf(degrees_b, 360.0f));
	}

	// --------------------------------------------------------------------------------------------------------------

}