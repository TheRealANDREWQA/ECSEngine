#pragma once
#include "../Core.h"
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

	struct ECSENGINE_API Vector8 {
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
			value = _mm256_castsi256_ps(Vec8i(_value));
		}
		ECS_INLINE Vector8(float x0, float y0, float z0, float w0, float x1, float y1, float z1, float w1) {
			value = Vec8f(x0, y0, z0, w0, x1, y1, z1, w1);
		}
		// Useful for masks
		ECS_INLINE Vector8(int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1) {
			value = _mm256_castsi256_ps(Vec8i(x0, y0, z0, w0, x1, y1, z1, w1));
		}
		ECS_INLINE Vector8(Vec4f _low, Vec4f _high) : value(_low, _high) {}
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
		ECS_INLINE Vector8(const float3* values) {
			value = PerLaneLoad3((const float*)values);
		}

		Vector8(const Vector8& other) = default;
		Vector8& ECS_VECTORCALL operator = (const Vector8& other) = default;

		ECS_INLINE bool ECS_VECTORCALL operator == (Vector8 other) const {
			return horizontal_and(value == other.value);
		}

		ECS_INLINE bool ECS_VECTORCALL operator != (Vector8 other) const {
			return !(*this == other);
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
		void StorePartialConstant(void* destination) const {
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

		void StoreFloat3(float3* low, float3* high) const;

		// Stores both the low and the high part
		ECS_INLINE void StoreFloat3(float3* destinations) const {
			StoreFloat3(destinations, destinations + 1);
		}

		// Returns only the float3 from the low
		float3 AsFloat3Low() const;

		float3 AsFloat3High() const;

		float4 AsFloat4Low() const;

		float4 AsFloat4High() const;

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
		float Last() const;

		// Returns the 8th float (the last float in the high 4 components)
		float LastHigh() const;

		float2 GetLasts() const;

		ECS_INLINE Vec4fb ECS_VECTORCALL AsMaskLow() const {
			return value.get_low().operator __m128();
		}

		ECS_INLINE Vec4fb ECS_VECTORCALL AsMaskHigh() const {
			return value.get_high().operator __m128();
		}

		ECS_INLINE Vec8fb ECS_VECTORCALL AsMask() const {
			return Vec8fb(value.operator __m256());
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
			return SplatLowLane(value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL SplatHigh() const {
			return SplatHighLane(value);
		}

		Vec8f value;
	};

#define ECS_SIMD_VECTOR_EPSILON_VALUE 0.0001f

	namespace VectorGlobals {
		ECSENGINE_API extern Vector8 ONE;
		ECSENGINE_API extern Vector8 INFINITY_MASK;
		ECSENGINE_API extern Vector8 SIGN_MASK;
		ECSENGINE_API extern Vector8 EPSILON;

		// Investigate if having a global stored value
		// is faster than doing a blend. If the one vector
		// is used somewhere else we can have that value already in a register
		// instead of having to do a cold read and a blend is basically 1 cycle
		// This is the case for RIGHT, UP, FORWARD, QuaternionIdentity
	}

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector8 ECS_VECTORCALL DegToRad(Vector8 angles);

	ECSENGINE_API Vector8 ECS_VECTORCALL RadToDeg(Vector8 angles);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector8 ECS_VECTORCALL ZeroVector();

	ECSENGINE_API Vector8 ECS_VECTORCALL RightVector();

	ECS_INLINE float3 GetRightVector() {
		return float3(1.0f, 0.0f, 0.0f);
	}

	ECSENGINE_API Vector8 ECS_VECTORCALL UpVector();

	ECS_INLINE float3 GetUpVector() {
		return float3(0.0f, 1.0f, 0.0f);
	}

	ECSENGINE_API Vector8 ECS_VECTORCALL ForwardVector();

	ECS_INLINE float3 GetForwardVector() {
		return float3(0.0f, 0.0f, 1.0f);
	}

	ECSENGINE_API Vector8 ECS_VECTORCALL LastElementOneVector();

	ECS_INLINE Vector8 MinusOneVector() {
		return ZeroVector() - VectorGlobals::ONE;
	}

	ECS_INLINE Vector8 TrueMaskVector() {
		return Vector8(INT_MAX);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector8 ECS_VECTORCALL IsInfiniteMask(Vector8 vector);

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

	ECS_INLINE Vector8 ECS_VECTORCALL Abs(Vector8 vector) {
		return abs(vector.value);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL AbsoluteDifference(Vector8 a, Vector8 b) {
		return Abs(a - b);
	}

	// --------------------------------------------------------------------------------------------------------------

	// By default it will only operate on the first 3 elements (as it is the
	// case for 3D operations) but can force it to use all 4 channels
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL Dot(Vector8 first, Vector8 second);

	// Add the float2 variant as well
	ECSENGINE_API float Dot(float2 first, float2 second);

	// --------------------------------------------------------------------------------------------------------------

	// Returns a mask that can be used to select some values. The difference is compared to an epsilon
	// The template here is needed to comply with the rule of thumb for use full lane
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL CompareMask(Vector8 first, Vector8 second, Vector8 epsilon = VectorGlobals::EPSILON);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		Compare,
		FORWARD(Vector8 first, Vector8 second, Vector8 epsilon = VectorGlobals::EPSILON),
		FORWARD(first, second, epsilon)
	);

	// Both directions need to be normalized beforehand
	// The angle must be given in radians
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL CompareAngleNormalizedRadMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		CompareAngleNormalizedRad,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians),
		FORWARD(first_normalized, second_normalized, radians)
	);

	// The angle must be given in radians
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL CompareAngleRadMask(Vector8 first, Vector8 second, Vector8 radians);

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
	ECSENGINE_API Vector8 ECS_VECTORCALL ProjectPointOnLineDirection(Vector8 line_point, Vector8 line_direction, Vector8 point);

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL ProjectPointOnLineDirectionNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point);

	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL ProjectPointOnLine(Vector8 line_a, Vector8 line_b, Vector8 point);

	// --------------------------------------------------------------------------------------------------------------

	// Cross for 3 element wide register
	ECSENGINE_API Vector8 ECS_VECTORCALL Cross(Vector8 a, Vector8 b);

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	ECS_INLINE Vector8 ECS_VECTORCALL SquareLength(Vector8 vector) {
		return Dot<use_full_lane>(vector, vector);
	}

	// Add the float2 variant as well
	ECSENGINE_API float SquareLength(float3 vector);

	// Add the float2 variant as well
	ECSENGINE_API float SquareLength(float2 vector);

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL Length(Vector8 vector);

	ECS_INLINE float Length(float3 vector) {
		return sqrt(SquareLength(vector));
	}

	ECS_INLINE float Length(float2 vector) {
		return sqrt(SquareLength(vector));
	}

	// --------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL ReciprocalLength(Vector8 vector);

	// --------------------------------------------------------------------------------------------------------------

	// Project the point on the line and then get the distance between these 2 points

	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL SquaredDistanceToLine(Vector8 line_point, Vector8 line_direction, Vector8 point);

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL SquaredDistanceToLineNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point);

	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL DistanceToLine(Vector8 line_point, Vector8 line_direction, Vector8 point);

	// This version is much faster than the other one since it avoids a dot and a division
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL DistanceToLineNormalized(Vector8 line_point, Vector8 line_direction_normalized, Vector8 point);

	// --------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL Normalize(Vector8 vector);

	ECSENGINE_API float3 Normalize(float3 vector);

	// Add the float2 variant as well
	ECSENGINE_API float2 Normalize(float2 vector);

	// The vector needs to have 2 packed float2
	// It will normalize both at the same time
	ECSENGINE_API Vector8 Normalize2x2(Vector8 vector);

	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL IsNormalizedSquareLengthMask(Vector8 squared_length);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		IsNormalizedSquareLength,
		Vector8 squared_length,
		squared_length
	);

	// Returns true if the vector is already normalized
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL IsNormalizedMask(Vector8 vector);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_USE_FULL_LANE(
		IsNormalized,
		Vector8 vector,
		vector
	);

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly)
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL NormalizeIfNot(Vector8 vector);

	// Returns 1.0f / value when using precise mode
	// And an approximate version when using ACCURATE/FAST
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vector8 ECS_VECTORCALL OneDividedVector(Vector8 value);

	// --------------------------------------------------------------------------------------------------------------

	// For both Is Parallel and Is Perpendicular we need to normalize both directions
	// Since we need to compare against a fixed epsilon and different magnitudes could
	// produce different results

	// Both directions need to be normalized beforehand - if you want consistent behaviour
	// Returns a SIMD mask that can be used to perform a selection
	ECSENGINE_API Vector8 ECS_VECTORCALL IsParallelMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon = VectorGlobals::EPSILON);

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
	ECSENGINE_API Vector8 ECS_VECTORCALL IsParallelAngleMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsParallelAngle,
		3,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians),
		FORWARD(first_normalized, second_normalized, radians)
	);

	// --------------------------------------------------------------------------------------------------------------

	// The cosine (dot) between them needs to be 0

	// This only compares the first 3 elements
	ECSENGINE_API Vector8 ECS_VECTORCALL IsPerpendicularMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon = VectorGlobals::EPSILON);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsPerpendicular,
		3,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 epsilon = VectorGlobals::EPSILON),
		FORWARD(first_normalized, second_normalized, epsilon)
	);

	// --------------------------------------------------------------------------------------------------------------

	// This only compares the first 3 elements
	// The radians angle needs to be smaller than half pi
	ECSENGINE_API Vector8 ECS_VECTORCALL IsPerpendicularAngleMask(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsPerpendicularAngle,
		3,
		FORWARD(Vector8 first_normalized, Vector8 second_normalized, Vector8 radians),
		FORWARD(first_normalized, second_normalized, radians)
	);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector8 ECS_VECTORCALL Clamp(Vector8 value, Vector8 min, Vector8 max);

	template<bool equal_min = false, bool equal_max = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL IsInRangeMask(Vector8 value, Vector8 min, Vector8 max);

	// --------------------------------------------------------------------------------------------------------------

	// Operates on the full vector
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL Reflect(Vector8 incident, Vector8 normal);

	// --------------------------------------------------------------------------------------------------------------

	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, Vector8 refraction_index);

	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL RefractLow(Vector8 incident, Vector8 normal, Vector8 refraction_index);

	// --------------------------------------------------------------------------------------------------------------

	// The vectors need to be normalized before
	// Returns the value of the angle between the 2 vectors in radians
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL AngleBetweenVectorsNormalizedRad(Vector8 a_normalized, Vector8 b_normalized);

	// Returns the value of the angle between the 2 vectors in radians
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL AngleBetweenVectorsRad(Vector8 a, Vector8 b);

	// Return the value of the angle between the 2 vectors in degrees
	template<bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL AngleBetweenVectorsNormalized(Vector8 a_normalized, Vector8 b_normalized);

	// Return the value of the angle between the 2 vectors in degrees
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE, bool use_full_lane = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL AngleBetweenVectors(Vector8 a, Vector8 b);

	// --------------------------------------------------------------------------------------------------------------

	// x % y
	ECSENGINE_API Vector8 ECS_VECTORCALL VectorFmod(Vector8 x, Vector8 y);

	// --------------------------------------------------------------------------------------------------------------

	// Returns the euler angles in radians that correspond to that direction
	// The 4th component is undefined
	ECSENGINE_API Vector8 ECS_VECTORCALL DirectionToRotationEulerRad(Vector8 direction);

	// Returns the euler angles in angles that correspond to that direction
	// The 4th component is undefined
	ECS_INLINE Vector8 ECS_VECTORCALL DirectionToRotationEuler(Vector8 direction) {
		return RadToDeg(DirectionToRotationEulerRad(direction));
	}

	// Returns the euler angles in radians that correspond to that direction
	ECSENGINE_API float3 DirectionToRotationEulerRad(float3 direction);

	// Returns the euler angles in angles that correspond to that direction
	ECS_INLINE float3 DirectionToRotationEuler(float3 direction) {
		return RadToDeg(DirectionToRotationEulerRad(direction));
	}

	// --------------------------------------------------------------------------------------------------------------

	// Constructs an orthonormal basis out of a given direction
	ECSENGINE_API void ECS_VECTORCALL OrthonormalBasis(Vector8 direction_normalized, Vector8* direction_first, Vector8* direction_second);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector8 ECS_VECTORCALL CullClipSpaceXMask(Vector8 vector);

	ECSENGINE_API Vector8 ECS_VECTORCALL CullClipSpaceYMask(Vector8 vector);

	ECSENGINE_API Vector8 ECS_VECTORCALL CullClipSpaceZMask(Vector8 vector);

	ECSENGINE_API Vector8 ECS_VECTORCALL CullClipSpaceWMask(Vector8 vector);

	// Returns a mask where vertices which should be kept have their lane set to true
	ECSENGINE_API Vector8 ECS_VECTORCALL CullClipSpaceMask(Vector8 vector);

	// Returns true if the point is in the camera frustum
	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		CullClipSpaceX,
		4,
		FORWARD(Vector8 vector),
		FORWARD(vector)
	);

	// Returns true if the point is in the camera frustum
	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		CullClipSpaceY,
		4,
		FORWARD(Vector8 vector),
		FORWARD(vector)
	);

	// Returns true if the point is in the camera frustum
	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		CullClipSpaceZ,
		4,
		FORWARD(Vector8 vector),
		FORWARD(vector)
	);

	// Returns true if the point is in the camera frustum
	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		CullClipSpaceW,
		4,
		FORWARD(Vector8 vector),
		FORWARD(vector)
	);

	// Returns true if the point is in the camera frustum
	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		CullClipSpace,
		4,
		FORWARD(Vector8 vector),
		FORWARD(vector)
	);

	// Converts from clip space to NDC
	ECSENGINE_API Vector8 ECS_VECTORCALL ClipSpaceToNDC(Vector8 vector);

	ECS_INLINE Vector8 ECS_VECTORCALL PerspectiveDivide(Vector8 vector) {
		// Same as ClipSpaceToNDC
		return ClipSpaceToNDC(vector);
	}

	// --------------------------------------------------------------------------------------------------------------

	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECSENGINE_API Vector8 ECS_VECTORCALL GetRightVectorForDirection(Vector8 direction_normalized);

	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECSENGINE_API Vector8 ECS_VECTORCALL GetUpVectorForDirection(Vector8 direction_normalized);

	// --------------------------------------------------------------------------------------------------------------

	// The origin needs to be splatted in both the low and high
	// If there are an odd count of points (3D ones, not vector), 
	// then the last value needs to be splatted to the high lane as well
	// The point will be splatted in both the low and high
	ECSENGINE_API Vector8 ECS_VECTORCALL ClosestPoint(Stream<Vector8> points, Vector8 origin);

	// --------------------------------------------------------------------------------------------------------------

	// This assumes that both a and b are in the 360 range
	ECSENGINE_API bool SameQuadrantRange360(float degrees_a, float degrees_b);

	ECSENGINE_API bool SameQuadrant(float degrees_a, float degrees_b);

	// --------------------------------------------------------------------------------------------------------------

}