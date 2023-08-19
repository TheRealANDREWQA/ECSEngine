#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "ecspch.h"
#include "VCLExtensions.h"

namespace ECSEngine {

	// Have the functions that return a vector mask that can be used then to generate all the permutations
	// of the boolean functions that correspond to that function

#define ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(function_name, default_lanes, signature, arguments) \
	template<int lanes = default_lanes> \
	ECS_INLINE bool ECS_VECTORCALL function_name##Low(signature) { \
		return PerLaneHorizontalAnd<lanes>(function_name##Mask(arguments).AsMaskLow()); \
	} \
	\
	template<int lanes = default_lanes> \
	ECS_INLINE bool2 ECS_VECTORCALL function_name(signature) { \
		return PerLaneHorizontalAnd<lanes>(function_name##Mask(arguments).AsMask()); \
	} \
	\
	template<int lanes = default_lanes> \
	ECS_INLINE bool ECS_VECTORCALL function_name##Whole(signature) { \
		return BasicTypeLogicAndBoolean(function_name(arguments)); \
	}

#define ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(function_name, lanes, signature, arguments) \
	ECS_INLINE bool ECS_VECTORCALL function_name##Low(signature) { \
		return PerLaneHorizontalAnd<lanes>(function_name##Mask(arguments).AsMaskLow()); \
	} \
	\
	ECS_INLINE bool2 ECS_VECTORCALL function_name(signature) { \
		return PerLaneHorizontalAnd<lanes>(function_name##Mask(arguments).AsMask()); \
	} \
	\
	ECS_INLINE bool ECS_VECTORCALL function_name##Whole(signature) { \
		return BasicTypeLogicAndBoolean(function_name(arguments)); \
	}

#define ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(function_name, signature, arguments) \
	template<bool use_full_lane = false> \
	ECS_INLINE bool ECS_VECTORCALL function_name##Low(signature) { \
		return PerLaneHorizontalAnd<use_full_lane ? 4 : 3>(function_name##Mask<use_full_lane>(arguments).AsMaskLow()); \
	} \
	\
	template<bool use_full_lane = false> \
	ECS_INLINE bool2 ECS_VECTORCALL function_name(signature) { \
		return PerLaneHorizontalAnd<use_full_lane ? 4 : 3>(function_name##Mask<use_full_lane>(arguments).AsMask()); \
	} \
	\
	template<bool use_full_lane = false> \
	ECS_INLINE bool ECS_VECTORCALL function_name##Whole(signature) { \
		return horizontal_and(function_name##Mask<use_full_lane>(arguments).AsMask()); \
	}

	enum VectorOperationPrecision {
		ECS_VECTOR_PRECISE,
		ECS_VECTOR_ACCURATE,
		ECS_VECTOR_FAST
	};

	struct Vector8 {
		ECS_INLINE Vector8() {}
		ECS_INLINE Vector8(const void* values) { value.load((float*)values); }
		ECS_INLINE Vector8(float _value) {
			value = Vec8f(_value);
		}
		// It will splat the first into the first 4 values and second in the last 4 values
		ECS_INLINE Vector8(float low, float high) {
			value = Vec8f(low, low, low, low, high, high, high, high);
		}
		// Useful for masks
		explicit ECS_INLINE Vector8(int _value) {
			value = _mm256_castsi256_ps(_mm256_set1_epi32(_value));
		}
		ECS_INLINE Vector8(float x0, float y0, float z0, float w0, float x1, float y1, float z1, float w1) {
			value = Vec8f(x0, y0, z0, w0, x1, y1, z1, w1);
		}
		// Useful for masks
		ECS_INLINE Vector8(int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1) {
			value = _mm256_castsi256_ps(_mm256_setr_epi32(x0, y0, z0, w0, x1, y1, z1, w1));
		}
		ECS_INLINE Vector8(Vec8fb _value) : value(_value) {}
		ECS_INLINE Vector8(Vec8f _value) : value(_value) {}
		ECS_INLINE Vector8(const float* values) {
			value.load(values);
		}

		ECS_INLINE Vector8(float3 low) {
			// Try to see if loading directly leads to crashes
			value.load((const float*)&low);
		}

		ECS_INLINE Vector8(float4 low) {
			// Try to see if loading directly leads to crashes
			value.load((const float*)&low);
		}

		ECS_INLINE Vector8(float4 values0, float4 values1) {
			value = Vec8f(values0.x, values0.y, values0.z, values0.w, values1.x, values1.y, values1.z, values1.w);
		}
		ECS_INLINE Vector8(float3 values0, float3 values1) {
			value = Vec8f(values0.x, values0.y, values0.z, 0.0f, values1.x, values1.y, values1.z, 0.0f);
		}

		ECS_INLINE Vector8(const Vector8& other) = default;
		ECS_INLINE Vector8& ECS_VECTORCALL operator = (const Vector8& other) = default;

		ECS_INLINE bool ECS_VECTORCALL operator == (Vector8 other) const {
			return horizontal_and(value == other.value);
		}

		ECS_INLINE bool ECS_VECTORCALL operator != (Vector8 other) const {
			return !horizontal_and(value == other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator + (const Vector8 other) const {
			return Vector8(value + other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator - (const Vector8 other) const {
			return Vector8(value - other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator -() const {
			return Vector8(-value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator * (const Vector8 other) const {
			return Vector8(value * other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator / (const Vector8 other) const {
			return Vector8(value / other.value);
		}

		ECS_INLINE Vector8& ECS_VECTORCALL operator += (const Vector8 other) {
			value += other.value;
			return *this;
		}

		ECS_INLINE Vector8& ECS_VECTORCALL operator -= (const Vector8 other) {
			value -= other.value;
			return *this;
		}

		ECS_INLINE Vector8& ECS_VECTORCALL operator *= (const Vector8 other) {
			value *= other.value;
			return *this;
		}

		ECS_INLINE Vector8& ECS_VECTORCALL operator /= (const Vector8 other) {
			value /= other.value;
			return *this;
		}

		ECS_INLINE Vector8& ECS_VECTORCALL operator *= (float _value) {
			value *= Vec8f(_value);
			return *this;
		}

		ECS_INLINE Vector8& ECS_VECTORCALL operator /= (float _value) {
			value /= Vec8f(_value);
			return *this;
		}

		ECS_INLINE ECS_VECTORCALL operator Vec8f() const {
			return value;
		}

		ECS_INLINE ECS_VECTORCALL operator __m256() const {
			return value.operator __m256();
		}

		ECS_INLINE Vec4f Low() const {
			return value.get_low();
		}

		ECS_INLINE Vec4f High() const {
			return value.get_high();
		}

		ECS_INLINE Vector8& Load(const void* data) {
			value.load((float*)data);
			return *this;
		}

		ECS_INLINE Vector8& LoadAligned(const void* data) {
			value.load_a((float*)data);
			return *this;
		}

		ECS_INLINE Vector8& LoadStreamed(const void* data) {
			value = _mm256_castsi256_ps(_mm256_stream_load_si256((const __m256i*)data));
			return *this;
		}

		ECS_INLINE void Store(void* destination) const {
			value.store((float*)destination);
		}

		ECS_INLINE void StoreAligned(void* destination) const {
			value.store_a((float*)destination);
		}

		ECS_INLINE void StoreStreamed(void* destination) const {
			value.store_nt((float*)destination);
		}

		ECS_INLINE void StorePartial(void* destination, int count) const {
			value.store_partial(count, (float*)destination);
		}

		// If the count is <= 4, the low parameter tells which part to write
		// For count >= 5, low is irrelevant
		template<int count, bool low = true>
		ECS_INLINE void StorePartialConstant(void* destination) const {
			float* float_destination = (float*)destination;

			auto store = [&](Vec4f value) {
				float* float_destination = (float*)destination;
				if constexpr (count == 1) {
					_mm_store_ss(float_destination, value);
				}
				else if constexpr (count == 2) {
					_mm_store_sd((double*)float_destination, _mm_castps_pd(value));
				}
				else if constexpr (count == 3) {
					_mm_store_sd((double*)float_destination, _mm_castps_pd(value));
					__m128 temp = _mm_movehl_ps(value, value);
					_mm_store_ss(float_destination + 2, temp);
				}
				else if constexpr (count == 4) {
					value.store(float_destination);
				}
			};

			if constexpr (count <= 4) {
				if constexpr (low) {
					store(value.get_low());
				}
				else {
					store(value.get_high());
				}
			}
			else if constexpr (count <= 8) {
				StorePartialConstant<4, true>(float_destination);
				StorePartialConstant<count - 4, false>(float_destination + 4);
			}
		}

		ECS_INLINE void StoreFloat3(float3* low, float3* high) const {
			// Store into a temp stack buffer the entire value such that
			// We don't have to perform multiple slower moves and some shifts
			float4 values[2];
			Store(values);

			*low = values[0].xyz();
			*high = values[1].xyz();
		}

		// Stores both the low and the high part
		ECS_INLINE void StoreFloat3(float3* destinations) const {
			StoreFloat3(destinations, destinations + 1);
		}

		// Returns only the float3 from the low
		ECS_INLINE float3 AsFloat3Low() const {
			// Store into a float4 since it will allow to issue a store
			// intrinsic instead of having to do some shifting and 2 moves
			float4 result;
			StorePartialConstant<4, true>(&result);
			return result.xyz();
		}
		
		ECS_INLINE float3 AsFloat3High() const {
			// Store into a float4 since it will allow to issue a store
			// intrinsic instead of having to do some shifting and 2 moves
			float4 result;
			StorePartialConstant<4, false>(&result);
			return result.xyz();
		}

		ECS_INLINE float4 AsFloat4Low() const {
			float4 result;
			StorePartialConstant<4, true>(&result);
			return result;
		}

		ECS_INLINE float4 AsFloat4High() const {
			float4 result;
			StorePartialConstant<4, false>(&result);
			return result;
		}

		ECS_INLINE float First() const {
			return _mm256_cvtss_f32(value);
		}

		ECS_INLINE float FirstHigh() const {
			return _mm_cvtss_f32(value.get_high());
		}

		ECS_INLINE float2 GetFirsts() const {
			return { First(), FirstHigh() };
		}

		// Returns the 4th float (the last float in the low 4 components)
		ECS_INLINE float Last() const {
			return _mm_cvtss_f32(_mm256_castps256_ps128(permute8<3, V_DC, V_DC, V_DC, 7, V_DC, V_DC, V_DC>(value)));
		}

		// Returns the 8th float (the last float in the high 4 components)
		ECS_INLINE float LastHigh() const {
			return _mm_cvtss_f32(permute4<3, V_DC, V_DC, V_DC>(value.get_high()));
		}

		ECS_INLINE float2 GetLasts() const {
			Vec8f permuted = permute8<3, V_DC, V_DC, V_DC, 7, V_DC, V_DC, V_DC>(value);
			return { _mm_cvtss_f32(_mm256_castps256_ps128(permuted)), _mm_cvtss_f32(permuted.get_high()) };
		}

		ECS_INLINE Vec4fb AsMaskLow() const {
			return value.get_low();
		}

		ECS_INLINE Vec4fb AsMaskHigh() const {
			return value.get_high();
		}

		ECS_INLINE Vec8fb AsMask() const {
			return value;
		}

		template<int element_count>
		ECS_INLINE bool MaskResultLow() const {
			return PerLaneHorizontalAnd<element_count>(AsMaskLow());
		}

		template<int element_count>
		ECS_INLINE bool MaskResultHigh() const {
			return PerLaneHorizontalAnd<element_count>(AsMaskHigh());
		}

		template<int element_count>
		ECS_INLINE bool2 MaskResult() const {
			return PerLaneHorizontalAnd<element_count>(AsMask());
		}

		template<int element_count>
		ECS_INLINE bool MaskResultWhole() const {
			return BasicTypeLogicAndBoolean(MaskResult<element_count>());
		}

		template<int element_count>
		ECS_INLINE bool MaskResultNone() const {
			bool2 result = MaskResult<element_count>();
			return result.x == false && result.y == false;
		}

		// Make this call compliant with the underlying call
		ECS_INLINE constexpr static int size() {
			return Vec8f::size();
		}

		ECS_INLINE constexpr static int Lanes() {
			return 2;
		}

		ECS_INLINE Vector8 ECS_VECTORCALL SplatLow() const {
			return Permute2f128Helper<0, 0>(value, value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL SplatHigh() const {
			return Permute2f128Helper<1, 1>(value, value);
		}

		Vec8f value;
	};

	namespace VectorGlobals {
		inline Vector8 ONE = Vector8(1.0f);
		inline Vector8 INFINITY_MASK = Vector8(0x7F800000);
		inline Vector8 SIGN_MASK = Vector8(0x7FFFFFFF);
		inline Vector8 EPSILON = Vector8(0.0001f);

		// Investigate if having a global stored value
		// is faster than doing a blend. If the one vector
		// is used somewhere else we can have that value already in a register
		// instead of having to do a cold read and a blend is basically 1 cycle
		// This is the case for RIGHT, UP, FORWARD, QuaternionIdentity
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL DegToRad(Vector8 angles) {
		Vector8 factor(DEG_TO_RAD_FACTOR);
		return angles * factor;
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL RadToDeg(Vector8 angles) {
		Vector8 factor(RAD_TO_DEG_FACTOR);
		return angles * factor;
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL ZeroVector() {
		return Vec8f(_mm256_setzero_ps());
	}

	ECS_INLINE Vector8 ECS_VECTORCALL RightVector() {
		return PerLaneBlend<4, 1, 2, 3>(ZeroVector(), VectorGlobals::ONE);
	}

	ECS_INLINE float3 GetRightVector() {
		return float3(1.0f, 0.0f, 0.0f);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL UpVector() {
		return PerLaneBlend<0, 5, 2, 3>(ZeroVector(), VectorGlobals::ONE);
	}

	ECS_INLINE float3 GetUpVector() {
		return float3(0.0f, 1.0f, 0.0f);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL ForwardVector() {
		return PerLaneBlend<0, 1, 6, 3>(ZeroVector(), VectorGlobals::ONE);
	}

	ECS_INLINE float3 GetForwardVector() {
		return float3(0.0f, 0.0f, 1.0f);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL LastElementOneVector() {
		return PerLaneBlend<0, 1, 2, 7>(ZeroVector(), VectorGlobals::ONE);
	}

	ECS_INLINE Vector8 MinusOneVector() {
		return ZeroVector() - VectorGlobals::ONE;
	}

	ECS_INLINE Vector8 TrueMaskVector() {
		return Vector8(INT_MAX);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL IsInfiniteMask(Vector8 vector) {
		vector.value &= VectorGlobals::SIGN_MASK;
		return vector.value == VectorGlobals::INFINITY_MASK.value;
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(IsInfinite, 4, Vector8 vector, vector);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL Select(Vector8 mask, Vector8 a, Vector8 b) {
		return select(Vec8fb(mask.value), a.value, b.value);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL IsNanMask(Vector8 vector) {
		return vector.value != vector.value;
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(IsNan, 4, Vector8 vector, vector);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL Negate(Vector8 vector) {
		return ZeroVector() - vector;
	}

	// --------------------------------------------------------------------------------------------------------------

	// By default it will only operate on the first 3 elements (as it is the
	// case for 3D operations) but can force it to use all 4 channels
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL Dot(Vector8 first, Vector8 second) {
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

	// Add the float2 variant as well
	ECS_INLINE float Dot(float2 first, float2 second) {
		return first.x * second.x + first.y * second.y;
	}

	// --------------------------------------------------------------------------------------------------------------

	// Returns a mask that can be used to select some values. The difference is compared to an epsilon
	// The template here is needed to comply with the rule of thumb for use full lane
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL CompareMask(Vector8 first, Vector8 second, Vector8 epsilon = VectorGlobals::EPSILON) {
		Vector8 zero_vector = ZeroVector();
		Vector8 absolute_difference = abs(first - second);
		auto comparison = absolute_difference < epsilon;
		return Vector8(comparison);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		Compare, 
		FORWARD(Vector8 first, Vector8 second, Vector8 epsilon = VectorGlobals::EPSILON), 
		FORWARD(first, second, epsilon)
	);

	// Both directions need to be normalized beforehand
	// The angle must be given in radians
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL CompareAngleNormalizedRadMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians) {
		Vector8 angle_cosine = cos(radians);
		Vector8 direction_cosine = abs(Dot<use_full_lane>(first_normalized, second_normalized));
		return direction_cosine > angle_cosine;
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		CompareAngleNormalizedRad,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians),
		FORWARD(first_normalized, second_normalized, radians)
	);

	// The angle must be given in radians
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL CompareAngleRadMask(Vector8 first, Vector8 second, Vector8 radians) {
		return CompareAngleNormalizedMask<use_full_lane>(Normalize<use_full_lane>(first), Normalize<use_full_lane>(second), radians);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		CompareAngleRad,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians),
		FORWARD(first_normalized, second_normalized, radians)
	);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL Lerp(Vector8 a, Vector8 b, Vector8 percentages) {
		return Fmadd(b - a, percentages, a);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL ProjectPointOnLineDirection(Vector8 line_point, Vector8 line_direction, Vector8 point) {
		// Formula A + dot(AP,line_direction) / dot(line_direction, line_direction) * line_direction
		// Where A is a point on the line and P is the point that we want to project
		return line_point + Dot<use_full_lane>(point - line_point, line_direction) / Dot<use_full_lane>(line_direction, line_direction) * line_direction;
	}

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL ProjectPointOnLineDirectionNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point) {
		// In this version we don't need to divide by the magnitude of the line direction
		// Formula A + dot(AP,line_direction) * line_direction
		// Where A is a point on the line and P is the point that we want to project
		return line_point + Dot<use_full_lane>(point - line_point, line_direction_normalized) * line_direction_normalized;
	}

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL ProjectPointOnLine(Vector8 line_a, Vector8 line_b, Vector8 point) {		
		Vector8 line_direction = line_b - line_a;
		return ProjectPointOnLineDirection<use_full_lane>(line_a, line_direction, point);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Cross for 3 element wide register
	ECS_INLINE Vector8 ECS_VECTORCALL Cross(Vector8 a, Vector8 b) {
		// v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x
		Vector8 vector1_yzx = PerLanePermute<1, 2, 0, V_DC>(a);
		Vector8 vector1_zxy = PerLanePermute<2, 0, 1, V_DC>(a);
		Vector8 vector2_zxy = PerLanePermute<2, 0, 1, V_DC>(b);
		Vector8 vector2_yzx = PerLanePermute<1, 2, 0, V_DC>(b);

		Vector8 second_multiply = vector1_zxy * vector2_yzx;
		return Fmsub(vector1_yzx, vector2_zxy, second_multiply);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL SquareLength(Vector8 vector) {
		return Dot<use_full_lane>(vector, vector);
	}

	// Add the float2 variant as well
	ECS_INLINE float SquareLength(float3 vector) {
		return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
	}

	// Add the float2 variant as well
	ECS_INLINE float SquareLength(float2 vector) {
		return vector.x * vector.x + vector.y * vector.y;
	}

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL Length(Vector8 vector) {
		return sqrt(SquareLength<use_full_lane>(vector));
	}

	ECS_INLINE float Length(float3 vector) {
		return sqrt(SquareLength(vector));
	}

	ECS_INLINE float Length(float2 vector) {
		return sqrt(SquareLength(vector));
	}

	// --------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLength(Vector8 vector) {
		Vector8 square_length = SquareLength<use_full_lane>(vector);
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			return VectorGlobals::ONE / sqrt(square_length);
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

	// --------------------------------------------------------------------------------------------------------------

	// Project the point on the line and then get the distance between these 2 points

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL SquaredDistanceToLine(Vector8 line_point, Vector8 line_direction, Vector8 point) {
		Vector8 projected_point = ProjectPointOnLineDirection<use_full_lane>(line_point, line_direction, point);
		return SquareLength<use_full_lane>(projected_point - point);
	}

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL SquaredDistanceToLineNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point) {
		Vector8 projected_point = ProjectPointOnLineDirectionNormalized<use_full_lane>(line_point, line_direction_normalized, point);
		return SquareLength<use_full_lane>(projected_point - point);
	}

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL DistanceToLine(Vector8 line_point, Vector8 line_direction, Vector8 point) {
		return sqrt(SquaredDistanceToLine<use_full_lane>(line_point, line_direction, point));
	}

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL DistanceToLineNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point) {
		return sqrt(SquaredDistanceToLineNormalized<use_full_lane>(line_point, line_direction_normalized, point));
	}

	// --------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL Normalize(Vector8 vector) {
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			// Don't use reciprocal length so as to not add an extra read to the
			// VectorGlobals::ONE and then a multiplication
			return vector / Length<use_full_lane>(vector);
		}
		else {
			return vector * ReciprocalLength<precision, use_full_lane>(vector);
		}
	}

	ECS_INLINE float3 Normalize(float3 vector) {
		Vector8 simd_vector(vector);
		simd_vector = Normalize<ECS_VECTOR_PRECISE>(simd_vector);
		return simd_vector.AsFloat3Low();
	}

	// Add the float2 variant as well
	ECS_INLINE float2 Normalize(float2 vector) {
		return vector * float2::Splat(1.0f / Length(vector));
	}

	// The vector needs to have 2 packed float2
	// It will normalize both at the same time
	ECS_INLINE Vector8 Normalize2x2(Vector8 vector) {
		Vector8 multiplication = vector * vector;
		Vector8 shuffle = PerLanePermute<1, 0, 3, 2>(multiplication);
		
		Vector8 squared_length = shuffle + multiplication;
		Vector8 length = sqrt(squared_length);
		return vector / length;
	}

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL IsNormalizedSquareLengthMask(Vector8 squared_length) {
		Vector8 one = VectorGlobals::ONE;
		return CompareMask<use_full_lane>(squared_length, one);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		IsNormalizedSquareLength,
		Vector8 squared_length,
		squared_length
	);

	// Returns true if the vector is already normalized
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL IsNormalizedMask(Vector8 vector) {
		Vector8 squared_length = SquareLength<use_full_lane>(vector);
		return IsNormalizedSquareLengthMask<use_full_lane>(squared_length);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		IsNormalized,
		Vector8 vector,
		vector
	);

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly)
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL NormalizeIfNot(Vector8 vector) {
		if constexpr (precision == ECS_VECTOR_PRECISE || precision == ECS_VECTOR_ACCURATE) {
			Vector8 square_length = SquareLength<use_full_lane>(vector);
			if (!IsNormalizedSquareLengthWhole(square_length)) {
				if constexpr (precision == ECS_VECTOR_PRECISE) {
					return vector / Vector8(sqrt(square_length));
				}
				else {
					return vector * approx_recipr(square_length);
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

	// --------------------------------------------------------------------------------------------------------------

	// For both Is Parallel and Is Perpendicular we need to normalize both directions
	// Since we need to compare against a fixed epsilon and different magnitudes could
	// produce different results
	
	// Both directions need to be normalized beforehand - if you want consistent behaviour
	// Returns a SIMD mask that can be used to perform a selection
	ECS_INLINE Vector8 ECS_VECTORCALL IsParallelMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon = VectorGlobals::EPSILON) {
		return CompareMask(first_normalized, second_normalized, epsilon);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsParallel,
		3,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon = VectorGlobals::EPSILON),
		FORWARD(first_normalized, second_normalized, epsilon)
	);

	// --------------------------------------------------------------------------------------------------------------

	// This only compares the first 3 elements
	// Both directions need to be normalized beforehand
	// Returns a SIMD mask that can be used to perform a selection
	ECS_INLINE Vector8 ECS_VECTORCALL IsParallelAngleMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians) {
		return CompareAngleNormalizedRadMask(first_normalized, second_normalized, radians);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsParallelAngle,
		3,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians),
		FORWARD(first_normalized, second_normalized, radians)
	);

	// --------------------------------------------------------------------------------------------------------------

	// The cosine (dot) between them needs to be 0

	// This only compares the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL IsPerpendicularMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon = VectorGlobals::EPSILON) {
		return CompareMask(Dot(first_normalized, second_normalized), ZeroVector(), epsilon);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsPerpendicular,
		3,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon = VectorGlobals::EPSILON),
		FORWARD(first_normalized, second_normalized, epsilon)
	);

	// --------------------------------------------------------------------------------------------------------------

	// This only compares the first 3 elements
	// The radians angle needs to be smaller than half pi
	ECS_INLINE Vector8 ECS_VECTORCALL IsPerpendicularAngleMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians) {
		Vector8 half_pi = PI / 2;
		return Vector8(Vec8fb(Not(CompareAngleNormalizedRadMask(first_normalized, second_normalized, half_pi - radians))));
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsPerpendicularAngle,
		3,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians),
		FORWARD(first_normalized, second_normalized, radians)
	);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL Clamp(Vector8 value, Vector8 min, Vector8 max) {
		auto is_smaller = value < min;
		auto is_greater = value > max;
		return select(is_greater, max, select(is_smaller, min, value));
	}

	template<bool equal_min = false, bool equal_max = false>
	ECS_INLINE Vector8 ECS_VECTORCALL IsInRangeMask(Vector8 value, Vector8 min, Vector8 max) {
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
			is_greatr = value < max;
		}

		return is_lower.AsMask() && is_greater.AsMask();
	}

	// --------------------------------------------------------------------------------------------------------------

	// Operates on the full vector
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL Reflect(Vector8 incident, Vector8 normal) {
		// result = incident - (2 * Dot(incident, normal)) * normal
		Vector8 dot = Dot<use_full_lane>(incident, normal);
		return incident - Vector8(((dot + dot) * normal));
	}

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
					result = Vector8(Vector4(zero.value.get_low()), Vector4(result.value.get_high()));
				}
				else if (mask == 0x0F) {
					result = Vector8(Vector4(result.value.get_low()), Vector4(zero.value.get_low()));
				}
				return result;
			}
		}
	}

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
		return SIMDHelpers::Refract<false, use_full_lane>(incident, normal, refraction_index);
	}

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL RefractLow(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
		return SIMDHelpers::Refract<true, use_full_lane>(incident, normal, refraction_index);
	}

	// --------------------------------------------------------------------------------------------------------------

	// The vectors need to be normalized before
	// Returns the value of the angle between the 2 vectors in radians
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL AngleBetweenVectorsNormalizedRad(Vector8 a_normalized, Vector8 b_normalized) {
		// Just take the cosine (dot) between them and return the acos result
		Vector8 dot = Dot<use_full_lane>(a_normalized, b_normalized);
		return acos(dot);
	}

	// Returns the value of the angle between the 2 vectors in radians
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL AngleBetweenVectorsRad(Vector8 a, Vector8 b) {
		return AngleBetweenVectorsNormalizedRad(Normalize<precision, use_full_lane>(a), Normalize<precision, use_full_lane>(b));
	}

	// Return the value of the angle between the 2 vectors in degrees
	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL AngleBetweenVectorsNormalized(Vector8 a_normalized, Vector8 b_normalized) {
		return RadToDeg(AngleBetweenVectorsNormalizedRad<use_full_lane>(a_normalized, b_normalized));
	}

	// Return the value of the angle between the 2 vectors in degrees
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL AngleBetweenVectors(Vector8 a, Vector8 b) {
		return RadToDeg(AngleBetweenVectorsRad<precision, use_full_lane>(a, b));
	}

	// --------------------------------------------------------------------------------------------------------------

	// x % y
	template<typename Vector8>
	ECS_INLINE Vector8 ECS_VECTORCALL VectorFmod(Vector8 x, Vector8 y) {
		// Formula x - truncate(x / y) * y;

		// Fmadd version might have better precision but slightly slower because of the _mm_set1_ps
		//return Fmadd(truncate(x / y), -y, x);

		// Cannot use approximate reciprocal of y and multiply instead of division
		// since the error is big enough that it will break certain use cases
		return x - Vector8(truncate(x / y) * y);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Returns the euler angles in radians that correspond to that direction
	// The 4th component is undefined
	ECS_INLINE Vector8 ECS_VECTORCALL DirectionToRotationEulerRad(Vector8 direction) {
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

	// Returns the euler angles in angles that correspond to that direction
	// The 4th component is undefined
	ECS_INLINE Vector8 ECS_VECTORCALL DirectionToRotationEuler(Vector8 direction) {
		return RadToDeg(DirectionToRotationEulerRad(direction));
	}

	// Returns the euler angles in radians that correspond to that direction
	ECS_INLINE float3 DirectionToRotationEulerRad(float3 direction) {
		Vector8 vector_dir(direction);
		vector_dir = DirectionToRotationEuler(vector_dir);
		return vector_dir.AsFloat3Low();
	}

	// Returns the euler angles in angles that correspond to that direction
	ECS_INLINE float3 DirectionToRotationEuler(float3 direction) {
		return RadToDeg(DirectionToRotationEulerRad(direction));
	}

	// --------------------------------------------------------------------------------------------------------------

	// Constructs an orthonormal basis out of a given direction
	ECS_INLINE void ECS_VECTORCALL OrthonormalBasis(Vector8 direction_normalized, Vector8* direction_first, Vector8* direction_second) {
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

	// Converts from clip space to NDC
	ECS_INLINE Vector8 ECS_VECTORCALL ClipSpaceToNDC(Vector8 vector) {
		// We just need to divide xyz / w, but here we also divide w by w
		// so as to not add another masking instruction
		return vector / PerLaneBroadcast<3>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
	
	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECS_INLINE Vector8 ECS_VECTORCALL GetRightVectorForDirection(Vector8 direction_normalized) {
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

	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECS_INLINE Vector8 ECS_VECTORCALL GetUpVectorForDirection(Vector8 direction_normalized) {
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

}