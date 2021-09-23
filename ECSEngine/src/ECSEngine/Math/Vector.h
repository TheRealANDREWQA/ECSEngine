#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "ecspch.h"
#include "VCLExtensions.h"

namespace ECSEngine {

	struct Vector4 {
		ECS_INLINE Vector4() {}
		// will be splatted across all lanes
		ECS_INLINE Vector4(float _value) {
			value = Vec4f(_value);
		}
		// Useful for masks
		ECS_INLINE Vector4(int _value) {
			value = _mm_castsi128_ps(_mm_set1_epi32(_value));
		}
		ECS_INLINE Vector4(float x, float y, float z, float w) {
			value = Vec4f(x, y, z, w);
		}
		// Useful for masks
		ECS_INLINE Vector4(int x, int y, int z, int w) {
			value = _mm_castsi128_ps(_mm_setr_epi32(x, y, z, w));
		}
		ECS_INLINE ECS_VECTORCALL Vector4(Vec4f _value) : value(_value) {}
		ECS_INLINE Vector4(const float* values) {
			value.load(values);
		}
		ECS_INLINE Vector4(float4& values) {
			value.load((const float*)&values);
		}
		ECS_INLINE Vector4(float3& values) {
			value.load((const float*)&value);
			Vec4f zero = _mm_setzero_ps();
			value = blend4<0, 1, 2, 4>(value, zero);
		}

		ECS_INLINE Vector4(const Vector4& other) = default;
		ECS_INLINE Vector4& ECS_VECTORCALL operator = (const Vector4& other) = default;

		ECS_INLINE bool ECS_VECTORCALL operator == (Vector4 other) {
			return horizontal_and(value == other.value);
		}

		ECS_INLINE bool ECS_VECTORCALL operator != (Vector4 other) {
			return !horizontal_and(value == other.value);
		}

		ECS_INLINE Vector4 ECS_VECTORCALL operator + (const Vector4 other) {
			return Vector4(value + other.value);
		}
		ECS_INLINE Vector4 ECS_VECTORCALL operator - (const Vector4 other) {
			return Vector4(value - other.value);
		}
		ECS_INLINE Vector4 ECS_VECTORCALL operator * (const Vector4 other) {
			return Vector4(value * other.value);
		}
		ECS_INLINE Vector4 ECS_VECTORCALL operator / (const Vector4 other) {
			return Vector4(value / other.value);
		}

		ECS_INLINE Vector4 ECS_VECTORCALL operator * (float _value) {
			return Vector4(value * Vec4f(_value));
		}
		ECS_INLINE Vector4 ECS_VECTORCALL operator / (float _value) {
			return Vector4(value / Vec4f(_value));
		}

		ECS_INLINE Vector4& ECS_VECTORCALL operator += (const Vector4 other) {
			value += other.value;
			return *this;
		}
		ECS_INLINE Vector4& ECS_VECTORCALL operator -= (const Vector4 other) {
			value -= other.value;
			return *this;
		}
		ECS_INLINE Vector4& ECS_VECTORCALL operator *= (const Vector4 other) {
			value *= other.value;
			return *this;
		}
		ECS_INLINE Vector4& ECS_VECTORCALL operator /= (const Vector4 other) {
			value /= other.value;
			return *this;
		}
		ECS_INLINE Vector4& ECS_VECTORCALL operator *= (float _value) {
			value *= Vec4f(_value);
			return *this;
		}
		ECS_INLINE Vector4& ECS_VECTORCALL operator /= (float _value) {
			value /= Vec4f(_value);
			return *this;
		}

		ECS_INLINE operator Vec4f() const {
			return value;
		}
		ECS_INLINE operator __m128() const {
			return value.operator __m128();
		}

		ECS_INLINE Vector4& Load(const float* data) {
			value.load(data);
		}

		ECS_INLINE Vector4& LoadAligned(const float* data) {
			value.load_a(data);
		}
		ECS_INLINE Vector4& LoadStreamed(const float* data) {
			value = _mm_castsi128_ps(_mm_stream_load_si128((const __m128i*)data));
		}
		ECS_INLINE void Store(float* destination) const {
			value.store(destination);
		}
		ECS_INLINE void StoreAligned(float* destination) const {
			value.store_a(destination);
		}
		ECS_INLINE void StoreStreamed(float* destination) const {
			value.store_nt(destination);
		}

		Vec4f value;
	};

	struct Vector8 {
		ECS_INLINE Vector8() {}
		ECS_INLINE Vector8(float _value) {
			value = Vec8f(_value);
		}
		// Useful for masks
		ECS_INLINE Vector8(int _value) {
			value = _mm256_castsi256_ps(_mm256_set1_epi32(_value));
		}
		ECS_INLINE Vector8(float x0, float y0, float z0, float w0, float x1, float y1, float z1, float w1) {
			value = Vec8f(x0, y0, z0, w0, x1, y1, z1, w1);
		}
		// Useful for masks
		ECS_INLINE Vector8(int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1) {
			value = _mm256_castsi256_ps(_mm256_setr_epi32(x0, y0, z0, w0, x1, y1, z1, w1));
		}
		ECS_INLINE ECS_VECTORCALL Vector8(Vec8f _value) : value(_value) {}
		ECS_INLINE Vector8(const float* values) {
			value.load(values);
		}
		ECS_INLINE Vector8(float4 values0, float4 values1) {
			value = Vec8f(values0.x, values0.y, values0.z, values0.w, values1.x, values1.y, values1.z, values1.w);
		}
		ECS_INLINE Vector8(float3 values0, float3 values1) {
			value = Vec8f(values0.x, values0.y, values0.z, 0.0f, values1.x, values1.y, values1.z, 0.0f);
		}
		ECS_INLINE Vector8(Vector4 low, Vector4 high) {
			value = Vec8f(low.value, high.value);
		}

		ECS_INLINE Vector8(const Vector8& other) = default;
		ECS_INLINE Vector8& ECS_VECTORCALL operator = (const Vector8& other) = default;

		ECS_INLINE bool ECS_VECTORCALL operator == (Vector8 other) {
			return horizontal_and(value == other.value);
		}

		ECS_INLINE bool ECS_VECTORCALL operator != (Vector8 other) {
			return !horizontal_and(value == other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator + (const Vector8 other) {
			return Vector8(value + other.value);
		}
		ECS_INLINE Vector8 ECS_VECTORCALL operator - (const Vector8 other) {
			return Vector8(value - other.value);
		}
		ECS_INLINE Vector8 ECS_VECTORCALL operator * (const Vector8 other) {
			return Vector8(value * other.value);
		}
		ECS_INLINE Vector8 ECS_VECTORCALL operator / (const Vector8 other) {
			return Vector8(value / other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator * (float _value) {
			return Vector8(value * Vec8f(_value));
		}
		ECS_INLINE Vector8 ECS_VECTORCALL operator / (float _value) {
			return Vector8(value / Vec8f(_value));
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

		ECS_INLINE operator Vec8f() const {
			return value;
		}
		ECS_INLINE operator __m256() const {
			return value.operator __m256();
		}

		ECS_INLINE Vector8& Load(const float* data) {
			value.load(data);
		}

		ECS_INLINE Vector8& LoadAligned(const float* data) {
			value.load_a(data);
		}
		ECS_INLINE Vector8& LoadStreamed(const float* data) {
			value = _mm256_castsi256_ps(_mm256_stream_load_si256((const __m256i*)data));
		}
		ECS_INLINE void Store(float* destination) const {
			value.store(destination);
		}
		ECS_INLINE void StoreAligned(float* destination) const {
			value.store_a(destination);
		}
		ECS_INLINE void StoreStreamed(float* destination) const {
			value.store_nt(destination);
		}

		Vec8f value;
	};

	namespace VectorGlobals {
		inline Vector4 ZERO_4 = Vector4(0.0f);
		inline Vector4 ONE_4 = Vector4(1.0f);
		inline Vector4 MINUS_ONE_4 = Vector4(-1.0f);
		inline Vector4 INFINITY_MASK_4 = Vector4(0x7F800000);
		inline Vector4 SIGN_MASK_4 = Vector4(0x7FFFFFFF);

		inline Vector8 ZERO_8 = Vector8(0.0f);
		inline Vector8 ONE_8 = Vector8(1.0f);
		inline Vector8 MINUS_ONE_8 = Vector8(-1.0f);
		inline Vector8 INFINITY_MASK_8 = Vector8(0x7F800000);
		inline Vector8 SIGN_MASK_8 = Vector8(0x7FFFFFFF);
	}

	using namespace VectorGlobals;

#pragma region IsInfinite
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE bool ECS_VECTORCALL IsInfinite(Vector4 vector) {
		vector.value &= SIGN_MASK_4;
		return vector == INFINITY_MASK_4;
	}

	ECS_INLINE bool ECS_VECTORCALL IsInfinity(Vector8 vector) {
		vector.value &= SIGN_MASK_8;
		return vector == INFINITY_MASK_8;
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Select
	// --------------------------------------------------------------------------------------------------------------
	ECS_INLINE Vector4 ECS_VECTORCALL Select(Vector4 mask, Vector4 a, Vector4 b) {
		return select(Vec4fb(mask.value), a.value, b.value);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Select(Vector8 mask, Vector8 a, Vector8 b) {
		return select(Vec8fb(mask.value), a.value, b.value);
	}
	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region IsNan
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE bool ECS_VECTORCALL IsNan(Vector4 vector) {
		return horizontal_and(vector.value != vector.value);
	}

	ECS_INLINE bool ECS_VECTORCALL IsNan(Vector8 vector) {
		return horizontal_and(vector.value != vector.value);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Negate
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Negate(Vector4 vector) {
		return ZERO_4 - vector;
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Negate(Vector8 vector) {
		return ZERO_8 - vector;
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Lerp
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Lerp(Vector4 a, Vector4 b, float percentage) {
		Vector4 percentages = Vector4(percentage);
		return Fmadd(b - a, percentages, a);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Lerp(Vector8 a, Vector8 b, float percentage) {
		Vector8 percentages = Vector8(percentage);
		return Fmadd(b - a, percentages, a);
	}

	ECS_INLINE Vector4 ECS_VECTORCALL Lerp(Vector4 a, Vector4 b, Vector4 percentages) {
		return Fmadd(b - a, percentages, a);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Lerp(Vector8 a, Vector8 b, Vector8 percentages) {
		return Fmadd(b - a, percentages, a);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Dot
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Dot(Vector4 vector1, Vector4 vector2) {
		return Dot<0x7F>(vector1, vector2);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Dot(Vector8 vector1, Vector8 vector2) {
		return Dot<0x7F>(vector1, vector2);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Cross
	// --------------------------------------------------------------------------------------------------------------

	// Cross for 3 element wide register
	ECS_INLINE Vector4 ECS_VECTORCALL Cross(Vector4 vector1, Vector4 vector2) {
		// v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x
		Vector4 vector1_yzx = permute4<1, 2, 0, 3>(vector1);
		Vector4 vector1_zxy = permute4<2, 0, 1, 3>(vector1);
		Vector4 vector2_zxy = permute4<2, 0, 1, 3>(vector2);
		Vector4 vector2_yzx = permute4<1, 2, 0, 3>(vector2);

		Vector4 second_multiply = vector1_zxy * vector2_yzx;
		return Fmsub(vector1_yzx, vector2_zxy, second_multiply);
	}

	// Cross for 3 element wide registers in low and upper parts of the ymm
	ECS_INLINE Vector8 ECS_VECTORCALL Cross(Vector8 vector1, Vector8 vector2) {
		// v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x

		Vector8 vector1_yzx = permute8<1, 2, 0, 3, 5, 6, 0, 7>(vector1);
		Vector8 vector1_zxy = permute8<2, 0, 1, 3, 6, 4, 5, 7>(vector1);
		Vector8 vector2_zxy = permute8<2, 0, 1, 3, 6, 4, 5, 7>(vector2);
		Vector8 vector2_yzx = permute8<1, 2, 0, 3, 5, 6, 4, 7>(vector2);

		Vector8 second_multiply = vector1_zxy * vector2_yzx;
		return Fmsub(vector1_yzx, vector2_zxy, second_multiply);
	}

	// --------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Square Length
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL SquareLength(Vector4 vector) {
		return Dot(vector, vector);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL SquareLength(Vector8 vector) {
		return Dot(vector, vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Length
// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Length(Vector4 vector) {
		return sqrt(SquareLength(vector));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Length(Vector8 vector) {
		return sqrt(SquareLength(vector));
	}

// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Reciprocal Length Estimate - rsqrt
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt(Vector4 vector) {
		Vector4 square_length = SquareLength(vector);
		return approx_rsqrt(square_length);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt(Vector8 vector) {
		Vector8 square_length = SquareLength(vector);
		return approx_rsqrt(square_length);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Reciprocal Length Estimate
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLengthEstimate(Vector4 vector) {
		Vector4 square_length = SquareLength(vector);
		return approx_recipr(sqrt(vector));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLengthEstimate(Vector8 vector) {
		Vector8 square_length = SquareLength(vector);
		return approx_recipr(sqrt(vector));
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Reciprocal Length
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLength(Vector4 vector) {
		Vector4 square_length = SquareLength(vector);
		return ONE_4 / Vector4(sqrt(vector));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLength(Vector8 vector) {
		Vector8 square_length = SquareLength(vector);
		return ONE_8 / Vector8(sqrt(vector));
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Normalize Estimate
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL NormalizeEstimate(Vector4 vector) {
		return vector * ReciprocalLengthEstimateRsqrt(vector);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL NormalizeEstimate(Vector8 vector) {
		return vector * ReciprocalLengthEstimateRsqrt(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Normalize
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Normalize(Vector4 vector) {
		return vector * ReciprocalLength(vector);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Normalize(Vector8 vector) {
		return vector * ReciprocalLength(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Clamp
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Clamp(Vector4 value, float min, float max) {
		Vector4 vector_min = Vector4(min);
		Vector4 vector_max = Vector4(max);
		auto is_smaller = value < vector_min;
		auto is_greater = value > vector_max;
		return select(is_greater, vector_max, Vector4(select(is_smaller, vector_min, value)));
	}

	ECS_INLINE Vector4 ECS_VECTORCALL Clamp(Vector4 value, Vector4 min, Vector4 max) {
		auto is_smaller = value < min;
		auto is_greater = value > max;
		return select(is_greater, max, Vector4(select(is_smaller, min, value)));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Clamp(Vector8 value, float min, float max) {
		Vector8 vector_min = Vector8(min);
		Vector8 vector_max = Vector8(max);
		auto is_smaller = value < vector_min;
		auto is_greater = value > vector_max;
		return select(is_greater, vector_max, Vector8(select(is_smaller, vector_min, value)));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Clamp(Vector8 value, Vector8 min, Vector8 max) {
		auto is_smaller = value < min;
		auto is_greater = value > max;
		return select(is_greater, max, Vector8(select(is_smaller, min, value)));
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion


#pragma region Reflect
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Reflect(Vector4 incident, Vector4 normal) {
		// result = incident - (2 * Dot(incident, normal)) * normal
		Vector4 dot = Dot(incident, normal);
		return incident - Vector4(((dot + dot) * normal));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Reflect(Vector8 incident, Vector8 normal) {
		// result = incident - (2 * Dot(incident, normal)) * normal
		Vector8 dot = Dot(incident, normal);
		return incident - Vector8(((dot + dot) * normal));
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Refract
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Refract(Vector4 incident, Vector4 normal, Vector4 refraction_index) {
		// result = refraction_index * incident - normal * (refraction_index * Dot(incident, normal) +
		// sqrt(1 - refraction_index * refraction_index * (1 - Dot(incident, normal) * Dot(incident, normal))))

		Vector4 dot = Dot(incident, normal);
		Vector4 one = ONE_4;
		Vector4 zero = ZERO_4;
		Vector4 sqrt_dot_factor = one - dot * dot;
		Vector4 refraction_index_squared = refraction_index * refraction_index;

		Vector4 sqrt_value = one - refraction_index_squared * sqrt_dot_factor;
		auto is_less_or_zero = sqrt_value <= zero;
		// total reflection
		if (_mm_movemask_ps(is_less_or_zero) == 0x0F) {
			return zero;
		}
		else {
			return Fmsub(refraction_index, incident, normal * Vector4(Fmadd(refraction_index, dot, Vector4(sqrt(sqrt_value)))));
		}
	}

	ECS_INLINE Vector4 ECS_VECTORCALL Refract(Vector4 incident, Vector4 normal, float refraction_index) {
		return Refract(incident, normal, Vector4(refraction_index));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
		// result = refraction_index * incident - normal * (refraction_index * Dot(incident, normal) +
		// sqrt(1 - refraction_index * refraction_index * (1 - Dot(incident, normal) * Dot(incident, normal))))

		Vector8 dot = Dot(incident, normal);
		Vector8 one = ONE_8;
		Vector8 zero = ZERO_8;
		Vector8 sqrt_dot_factor = one - dot * dot;
		Vector8 refraction_index_squared = refraction_index * refraction_index;

		Vector8 sqrt_value = one - refraction_index_squared * sqrt_dot_factor;
		auto is_less_or_zero = sqrt_value <= zero;

		Vector8 result = sqrt_value;

		int mask = _mm256_movemask_ps(is_less_or_zero);
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

	ECS_INLINE Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, float refraction_index) {
		return Refract(incident, normal, Vector8(refraction_index));
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Angle
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL Angle(Vector4 a, Vector4 b) {
		auto a_reciprocal_length = ReciprocalLength(a);
		auto b_reciprocal_length = ReciprocalLength(b);

		auto dot = Dot(a, b);

		auto intermediate = a_reciprocal_length * b_reciprocal_length;

		auto cos_angle = dot * intermediate;
		Vector4 minus_one = MINUS_ONE_4;
		Vector4 one = ONE_4;

		cos_angle = Clamp(cos_angle, minus_one, one);

		return acos(cos_angle);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Angle(Vector8 a, Vector8 b) {
		auto a_reciprocal_length = ReciprocalLength(a);
		auto b_reciprocal_length = ReciprocalLength(b);

		auto dot = Dot(a, b);

		auto intermediate = a_reciprocal_length * b_reciprocal_length;

		auto cos_angle = dot * intermediate;
		Vector8 minus_one = MINUS_ONE_8;
		Vector8 one = ONE_8;

		// for floating point errors, if they overshoot or undershoot
		cos_angle = Clamp(cos_angle, minus_one, one);

		return acos(cos_angle);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region
	// --------------------------------------------------------------------------------------------------------------



	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

}