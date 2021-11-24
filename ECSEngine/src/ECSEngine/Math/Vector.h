#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "ecspch.h"
#include "VCLExtensions.h"

namespace ECSEngine {

	struct Vector4 {
		ECS_INLINE Vector4() {}
		ECS_INLINE Vector4(const void* values) { value.load((float*)values); }
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
		ECS_INLINE Vector4(float4 values) {
			value.load((const float*)&values);
		}
		ECS_INLINE Vector4(float3 values) {
			value.load((const float*)&values);
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

		ECS_INLINE Vector4 ECS_VECTORCALL operator + (const Vector4 other) const {
			return Vector4(value + other.value);
		}

		ECS_INLINE Vector4 ECS_VECTORCALL operator - (const Vector4 other) const {
			return Vector4(value - other.value);
		}

		ECS_INLINE Vector4 ECS_VECTORCALL operator * (const Vector4 other) const {
			return Vector4(value * other.value);
		}

		ECS_INLINE Vector4 ECS_VECTORCALL operator / (const Vector4 other) const {
			return Vector4(value / other.value);
		}

		ECS_INLINE Vector4 ECS_VECTORCALL operator * (float _value) const {
			return Vector4(value * Vec4f(_value));
		}

		ECS_INLINE Vector4 ECS_VECTORCALL operator / (float _value) const {
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

		ECS_INLINE Vector4& Load(const void* data) {
			value.load((float*)data);
			return *this;
		}

		ECS_INLINE Vector4& LoadAligned(const void* data) {
			value.load_a((float*)data);
			return *this;
		}

		ECS_INLINE Vector4& LoadStreamed(const void* data) {
			value = _mm_castsi128_ps(_mm_stream_load_si128((const __m128i*)data));
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

		template<int count>
		void StorePartialConstant(void* destination) const {
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
				Store(destination);
			}
		}

		ECS_INLINE float First() const {
			return _mm_cvtss_f32(value);
		}

		Vec4f value;
	};

	struct Vector8 {
		ECS_INLINE Vector8() {}
		ECS_INLINE Vector8(const void* values) { value.load((float*)values); }
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

		ECS_INLINE Vector8 ECS_VECTORCALL operator * (const Vector8 other) const {
			return Vector8(value * other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator / (const Vector8 other) const {
			return Vector8(value / other.value);
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator * (float _value) const {
			return Vector8(value * Vec8f(_value));
		}

		ECS_INLINE Vector8 ECS_VECTORCALL operator / (float _value) const {
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

		template<int count>
		void StorePartialConstant(void* destination) const {
			float* float_destination = (float*)destination;
			
			if constexpr (count <= 4) {
				Vector4 low = value.get_low();
				low.StorePartialConstant<count>(destination);
			}
			else if constexpr (count <= 8) {
				Vector4 low = value.get_low();
				low.Store(destination);
				Vector4 high = value.get_high();
				high.StorePartialConstant<count - 4>(float_destination + 4);
			}
		}

		ECS_INLINE float First() const {
			return _mm256_cvtss_f32(value);
		}

		ECS_INLINE float FirstHigh() const {
			return _mm_cvtss_f32(value.get_high());
		}

		ECS_INLINE Vector4 Low() const {
			return value.get_low();
		}

		ECS_INLINE Vector4 High() const {
			return value.get_high();
		}

		Vec8f value;
	};

	namespace VectorGlobals {
		inline Vector4 ONE_4 = Vector4(1.0f);
		inline Vector4 MINUS_ONE_4 = Vector4(-1.0f);
		inline Vector4 INFINITY_MASK_4 = Vector4(0x7F800000);
		inline Vector4 SIGN_MASK_4 = Vector4(0x7FFFFFFF);
		inline Vector4 RIGHT_4 = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
		inline Vector4 UP_4 = Vector4(0.0f, 1.0f, 0.0f, 0.0f);
		inline Vector4 FORWARD_4 = Vector4(0.0f, 0.0f, 1.0f, 0.0f);
		inline Vector4 QUATERNION_IDENTITY_4 = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

		inline Vector8 ONE_8 = Vector8(1.0f);
		inline Vector8 MINUS_ONE_8 = Vector8(-1.0f);
		inline Vector8 INFINITY_MASK_8 = Vector8(0x7F800000);
		inline Vector8 SIGN_MASK_8 = Vector8(0x7FFFFFFF);
		inline Vector8 RIGHT_8 = Vector8(RIGHT_4, RIGHT_4);
		inline Vector8 UP_8 = Vector8(UP_4, UP_4);
		inline Vector8 FORWARD_8 = Vector8(FORWARD_4, FORWARD_4);
		inline Vector8 QUATERNION_IDENTITY_8 = Vector8(QUATERNION_IDENTITY_4, QUATERNION_IDENTITY_4);
	}

#pragma region DegToRad
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL DegToRad(Vector4 angles) {
		Vector4 factor(DEG_TO_RAD_FACTOR);
		return angles * factor;
	}

	ECS_INLINE Vector8 ECS_VECTORCALL DegToRad(Vector8 angles) {
		Vector8 factor(DEG_TO_RAD_FACTOR);
		return angles * factor;
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region RadToDeg
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL RadToDeg(Vector4 angles) {
		Vector4 factor(RAD_TO_DEG_FACTOR);
		return angles * factor;
	}

	ECS_INLINE Vector8 ECS_VECTORCALL RadToDeg(Vector8 angles) {
		Vector8 factor(RAD_TO_DEG_FACTOR);
		return angles * factor;
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Zero Vector
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ZeroVector4() {
		return _mm_setzero_ps();
	}

	ECS_INLINE Vector8 ZeroVector8() {
		return _mm256_setzero_ps();
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region IsInfinite
	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE bool ECS_VECTORCALL IsInfinite(Vector4 vector) {
		vector.value &= VectorGlobals::SIGN_MASK_4;
		return vector == VectorGlobals::INFINITY_MASK_4;
	}

	ECS_INLINE bool ECS_VECTORCALL IsInfinity(Vector8 vector) {
		vector.value &= VectorGlobals::SIGN_MASK_8;
		return vector == VectorGlobals::INFINITY_MASK_8;
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
		return ZeroVector4() - vector;
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Negate(Vector8 vector) {
		return ZeroVector8() - vector;
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

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL Dot(Vector4 vector1, Vector4 vector2) {
		return Dot<0xFF>(vector1, vector2);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL Dot(Vector8 vector1, Vector8 vector2) {
		return Dot<0xFF>(vector1, vector2);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL Dot3(Vector4 vector1, Vector4 vector2) {
		return Dot<0x7F>(vector1, vector2);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL Dot3(Vector8 vector1, Vector8 vector2) {
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

	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *DotFunction)(Vector4, Vector4)>
		Vector4 ECS_VECTORCALL SquareLength(Vector4 vector) {
			return DotFunction(vector, vector);
		}

		template<Vector8 (ECS_VECTORCALL *DotFunction)(Vector8, Vector8)>
		Vector8 ECS_VECTORCALL SquareLength(Vector8 vector) {
			return DotFunction(vector, vector);
		}
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL SquareLength(Vector4 vector) {
		return SIMDHelpers::SquareLength<Dot>(vector);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL SquareLength(Vector8 vector) {
		return SIMDHelpers::SquareLength<Dot>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL SquareLength3(Vector4 vector) {
		return SIMDHelpers::SquareLength<Dot3>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL SquareLength3(Vector8 vector) {
		return SIMDHelpers::SquareLength<Dot3>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Length
	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *SquareFunction)(Vector4)>
		Vector4 ECS_VECTORCALL Length(Vector4 vector) {
			return sqrt(SquareFunction(vector));
		}

		template<Vector8 (ECS_VECTORCALL* SquareFunction)(Vector8)>
		Vector8 ECS_VECTORCALL Length(Vector8 vector) {
			return sqrt(SquareFunction(vector));
		}
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL Length(Vector4 vector) {
		return SIMDHelpers::Length<SquareLength>(vector);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL Length(Vector8 vector) {
		return SIMDHelpers::Length<SquareLength>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL Length3(Vector4 vector) {
		return SIMDHelpers::Length<SquareLength3>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL Length3(Vector8 vector) {
		return SIMDHelpers::Length<SquareLength3>(vector);
	}

// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Reciprocal Length Estimate - rsqrt
	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *SquareFunction)(Vector4)>
		Vector4 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt(Vector4 vector) {
			Vector4 square_length = SquareFunction(vector);
			return approx_rsqrt(square_length);
		}

		template<Vector8 (ECS_VECTORCALL* SquareFunction)(Vector8)>
		Vector8 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt(Vector8 vector) {
			Vector8 square_length = SquareFunction(vector);
			return approx_rsqrt(square_length);
		}
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt(Vector4 vector) {
		return SIMDHelpers::ReciprocalLengthEstimateRsqrt<SquareLength>(vector);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt(Vector8 vector) {
		return SIMDHelpers::ReciprocalLengthEstimateRsqrt<SquareLength>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt3(Vector4 vector) {
		return SIMDHelpers::ReciprocalLengthEstimateRsqrt<SquareLength3>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLengthEstimateRsqrt3(Vector8 vector) {
		return SIMDHelpers::ReciprocalLengthEstimateRsqrt<SquareLength3>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Reciprocal Length Estimate
	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *SquareFunction)(Vector4)>
		Vector4 ECS_VECTORCALL ReciprocalLengthEstimate(Vector4 vector) {
			Vector4 square_length = SquareFunction(vector);
			return approx_recipr(sqrt(square_length));
		}

		template<Vector8 (ECS_VECTORCALL* SquareFunction)(Vector8)>
		Vector8 ECS_VECTORCALL ReciprocalLengthEstimate(Vector8 vector) {
			Vector8 square_length = SquareFunction(vector);
			return approx_recipr(sqrt(square_length));
		}
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLengthEstimate(Vector4 vector) {
		return SIMDHelpers::ReciprocalLengthEstimate<SquareLength>(vector);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLengthEstimate(Vector8 vector) {
		return SIMDHelpers::ReciprocalLengthEstimate<SquareLength>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLengthEstimate3(Vector4 vector) {
		return SIMDHelpers::ReciprocalLengthEstimate<SquareLength3>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLengthEstimate3(Vector8 vector) {
		return SIMDHelpers::ReciprocalLengthEstimate<SquareLength3>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Reciprocal Length
	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *SquareFunction)(Vector4)>
		Vector4 ECS_VECTORCALL ReciprocalLength(Vector4 vector) {
			Vector4 square_length = SquareFunction(vector);
			return VectorGlobals::ONE_4 / Vector4(sqrt(square_length));
		}

		template<Vector8 (ECS_VECTORCALL* SquareFunction)(Vector8)>
		Vector8 ECS_VECTORCALL ReciprocalLength(Vector8 vector) {
			Vector8 square_length = SquareFunction(vector);
			return VectorGlobals::ONE_8 / Vector8(sqrt(square_length));
		}
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLength(Vector4 vector) {
		return SIMDHelpers::ReciprocalLength<SquareLength>(vector);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLength(Vector8 vector) {
		return SIMDHelpers::ReciprocalLength<SquareLength>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL ReciprocalLength3(Vector4 vector) {
		return SIMDHelpers::ReciprocalLength<SquareLength3>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL ReciprocalLength3(Vector8 vector) {
		return SIMDHelpers::ReciprocalLength<SquareLength3>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Normalize Estimate
	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<Vector4(ECS_VECTORCALL* LengthFunction)(Vector4 vector)>
		Vector4 ECS_VECTORCALL Normalize(Vector4 vector) {
			return vector * ReciprocalLength(vector);
		}

		template<Vector8(ECS_VECTORCALL* LengthFunction)(Vector8 vector)>
		Vector8 ECS_VECTORCALL Normalize(Vector8 vector) {
			return vector * ReciprocalLength(vector);
		}
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL NormalizeEstimate(Vector4 vector) {
		return SIMDHelpers::Normalize<ReciprocalLengthEstimateRsqrt>(vector);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL NormalizeEstimate(Vector8 vector) {
		return SIMDHelpers::Normalize<ReciprocalLengthEstimateRsqrt>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL NormalizeEstimate3(Vector4 vector) {
		return SIMDHelpers::Normalize<ReciprocalLengthEstimateRsqrt3>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL NormalizeEstimate3(Vector8 vector) {
		return SIMDHelpers::Normalize<ReciprocalLengthEstimateRsqrt3>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Normalize
	// --------------------------------------------------------------------------------------------------------------

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL Normalize(Vector4 vector) {
		return SIMDHelpers::Normalize<ReciprocalLength>(vector);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL Normalize(Vector8 vector) {
		return SIMDHelpers::Normalize<ReciprocalLength>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL Normalize3(Vector4 vector) {
		return SIMDHelpers::Normalize<ReciprocalLength3>(vector);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL Normalize3(Vector8 vector) {
		return SIMDHelpers::Normalize<ReciprocalLength3>(vector);
	}

	ECS_INLINE float3 Normalize(float3 vector) {
		float3 value;

		Vector4 simd_vector(vector);
		simd_vector = Normalize3(simd_vector);
		simd_vector.StorePartialConstant<3>(&value);

		return value;
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
		return select(is_greater, vector_max, Vec4f(select(is_smaller, vector_min, value)));
	}

	ECS_INLINE Vector4 ECS_VECTORCALL Clamp(Vector4 value, Vector4 min, Vector4 max) {
		auto is_smaller = value < min;
		auto is_greater = value > max;
		return select(is_greater, max, Vec4f(select(is_smaller, min, value)));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Clamp(Vector8 value, float min, float max) {
		Vector8 vector_min = Vector8(min);
		Vector8 vector_max = Vector8(max);
		auto is_smaller = value < vector_min;
		auto is_greater = value > vector_max;
		return select(is_greater, vector_max, Vec8f(select(is_smaller, vector_min, value)));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL Clamp(Vector8 value, Vector8 min, Vector8 max) {
		auto is_smaller = value < min;
		auto is_greater = value > max;
		return select(is_greater, max, Vec8f(select(is_smaller, min, value)));
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion


#pragma region Reflect
	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *DotFunction)(Vector4, Vector4)>
		Vector4 ECS_VECTORCALL Reflect(Vector4 incident, Vector4 normal) {
			// result = incident - (2 * Dot(incident, normal)) * normal
			Vector4 dot = DotFunction(incident, normal);
			return incident - Vector4(((dot + dot) * normal));
		}

		template<Vector8 (ECS_VECTORCALL *DotFunction)(Vector8, Vector8)>
		Vector8 ECS_VECTORCALL Reflect(Vector8 incident, Vector8 normal) {
			// result = incident - (2 * Dot(incident, normal)) * normal
			Vector8 dot = DotFunction(incident, normal);
			return incident - Vector8(((dot + dot) * normal));
		}
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL Reflect(Vector4 incident, Vector4 normal) {
		return SIMDHelpers::Reflect<Dot>(incident, normal);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL Reflect(Vector8 incident, Vector8 normal) {
		return SIMDHelpers::Reflect<Dot>(incident, normal);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL Reflect3(Vector4 incident, Vector4 normal) {
		return SIMDHelpers::Reflect<Dot3>(incident, normal);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL Reflect3(Vector8 incident, Vector8 normal) {
		return SIMDHelpers::Reflect<Dot3>(incident, normal);
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Refract
	// --------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *DotFunction)(Vector4, Vector4)>
		Vector4 ECS_VECTORCALL Refract(Vector4 incident, Vector4 normal, Vector4 refraction_index) {
			// result = refraction_index * incident - normal * (refraction_index * Dot(incident, normal) +
			// sqrt(1 - refraction_index * refraction_index * (1 - Dot(incident, normal) * Dot(incident, normal))))

			Vector4 dot = DotFunction(incident, normal);
			Vector4 one = VectorGlobals::ONE_4;
			Vector4 zero = ZeroVector4();
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

		template<Vector8 (ECS_VECTORCALL *DotFunction)(Vector8, Vector8)>
		Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
			// result = refraction_index * incident - normal * (refraction_index * Dot(incident, normal) +
			// sqrt(1 - refraction_index * refraction_index * (1 - Dot(incident, normal) * Dot(incident, normal))))

			Vector8 dot = DotFunction(incident, normal);
			Vector8 one = VectorGlobals::ONE_8;
			Vector8 zero = ZeroVector8();
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
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL Refract(Vector4 incident, Vector4 normal, Vector4 refraction_index) {
		return SIMDHelpers::Refract<Dot>(incident, normal, refraction_index);
	}

	// Operates on the full vector
	ECS_INLINE Vector4 ECS_VECTORCALL Refract(Vector4 incident, Vector4 normal, float refraction_index) {
		return Refract(incident, normal, Vector4(refraction_index));
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
		return SIMDHelpers::Refract<Dot>(incident, normal, refraction_index);
	}

	// Operates on the full vector
	ECS_INLINE Vector8 ECS_VECTORCALL Refract(Vector8 incident, Vector8 normal, float refraction_index1, float refraction_index2) {
		return Refract(incident, normal, Vector8(refraction_index1, refraction_index1, refraction_index1, refraction_index1, refraction_index2, refraction_index2, refraction_index2, refraction_index2));
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL Refract3(Vector4 incident, Vector4 normal, Vector4 refraction_index) {
		return SIMDHelpers::Refract<Dot3>(incident, normal, refraction_index);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector4 ECS_VECTORCALL Refract3(Vector4 incident, Vector4 normal, float refraction_index) {
		return Refract3(incident, normal, Vector4(refraction_index));
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL Refract3(Vector8 incident, Vector8 normal, Vector8 refraction_index) {
		return SIMDHelpers::Refract<Dot3>(incident, normal, refraction_index);
	}

	// Operates on the first 3 elements
	ECS_INLINE Vector8 ECS_VECTORCALL Refract3(Vector8 incident, Vector8 normal, float refraction_index1, float refraction_index2) {
		return Refract3(incident, normal, Vector8(refraction_index1, refraction_index1, refraction_index1, refraction_index1, refraction_index2, refraction_index2, refraction_index2, refraction_index2));
	}

	// --------------------------------------------------------------------------------------------------------------
#pragma endregion

#pragma region Angle

	// --------------------------------------------------------------------------------------------------------------
	
	namespace SIMDHelpers {
		template<Vector4 (ECS_VECTORCALL *ReciprocalLengthFunction)(Vector4), Vector4 (ECS_VECTORCALL *DotFunction)(Vector4, Vector4)>
		Vector4 ECS_VECTORCALL Angle(Vector4 a, Vector4 b) {
			auto a_reciprocal_length = ReciprocalLengthFunction(a);
			auto b_reciprocal_length = ReciprocalLengthFunction(b);

			auto dot = DotFunction(a, b);

			auto intermediate = a_reciprocal_length * b_reciprocal_length;

			auto cos_angle = dot * intermediate;
			Vector4 minus_one = VectorGlobals::MINUS_ONE_4;
			Vector4 one = VectorGlobals::ONE_4;

			cos_angle = Clamp(cos_angle, minus_one, one);

			return acos(cos_angle);
		}

		template<Vector8 (ECS_VECTORCALL *ReciprocalLengthFunction)(Vector8), Vector8 (ECS_VECTORCALL *DotFunction)(Vector8, Vector8)>
		Vector8 ECS_VECTORCALL Angle(Vector8 a, Vector8 b) {
			auto a_reciprocal_length = ReciprocalLengthFunction(a);
			auto b_reciprocal_length = ReciprocalLengthFunction(b);

			auto dot = DotFunction(a, b);

			auto intermediate = a_reciprocal_length * b_reciprocal_length;

			auto cos_angle = dot * intermediate;
			Vector8 minus_one = VectorGlobals::MINUS_ONE_8;
			Vector8 one = VectorGlobals::ONE_8;

			cos_angle = Clamp(cos_angle, minus_one, one);

			return acos(cos_angle);
		}
	}

	// Operates on the full vector; The value of the angle between the 2 vectors in radians
	ECS_INLINE Vector4 ECS_VECTORCALL AngleBetweenVectors(Vector4 a, Vector4 b) {
		return SIMDHelpers::Angle<ReciprocalLength, Dot>(a, b);
	}

	// Operates on the full vector; The value of the angle between the 2 vectors in radians
	ECS_INLINE Vector8 ECS_VECTORCALL AngleBetweenVectors(Vector8 a, Vector8 b) {
		return SIMDHelpers::Angle<ReciprocalLength, Dot>(a, b);
	}
	
	// Operates on the first 3 elements; The value of the angle between the 2 vectors in radians
	ECS_INLINE Vector4 ECS_VECTORCALL AngleBetweenVectors3(Vector4 a, Vector4 b) {
		return SIMDHelpers::Angle<ReciprocalLength3, Dot3>(a, b);
	}

	// Operates on the first 3 elements; The value of the angle between the 2 vectors in radians
	ECS_INLINE Vector8 ECS_VECTORCALL AngleBetweenVectors3(Vector8 a, Vector8 b) {
		return SIMDHelpers::Angle<ReciprocalLength3, Dot3>(a, b);
	}

	// --------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Fmod

	// --------------------------------------------------------------------------------------------------------------

	// x % y
	ECS_INLINE Vector4 ECS_VECTORCALL VectorFmod(Vector4 x, Vector4 y) {
		// Formula x - truncate(x / y) * y;

		// Fmadd version might have better precision but slightly slower because of the _mm_set1_ps
		//return Fmadd(truncate(x / y), -y, x);
		 
		// Cannot use approximate reciprocal of y and multiply instead of division
		// since the error is big enough that it will break certain use cases
		return x - Vector4(truncate(x / y) * y);
	}

	// x % y
	ECS_INLINE Vector8 ECS_VECTORCALL VectorFmod(Vector8 x, Vector8 y) {
		// Formula x - truncate(x / y) * y;

		// Fmadd version might have better precision but slightly slower because of the _mm256_set1_ps
		//return Fmadd(truncate(x / y), -y, x);

		// Cannot use approximate reciprocal of y and multiply instead of division
		// since the error is big enough that it will break certain use cases
		return x - Vector8(truncate(x / y) * y);
	}

	// --------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Direction To Rotation

	// --------------------------------------------------------------------------------------------------------------

	// Returns the euler angles in radians that correspond to that direction
	ECS_INLINE Vector4 ECS_VECTORCALL DirectionToRotationRad(Vector4 direction) {
		/* Scalar algorithm
		float x_angle = fmod(atan2(direction.y, direction.z), PI);
		float y_angle = fmod(atan2(direction.z, direction.x), PI);
		float z_angle = fmod(atan2(direction.y, direction.x), PI);*/

		Vector4 tangent_numerator = permute4<1, 2, 1, V_DC>(direction);
		Vector4 tangent_denominator = permute4<2, 0, 0, V_DC>(direction);

		Vector4 radians = atan2(tangent_numerator, tangent_denominator);
		return VectorFmod(radians, Vector4(PI));
	}

	// Returns the euler angles in angles that correspond to that direction
	ECS_INLINE Vector4 ECS_VECTORCALL DirectionToRotation(Vector4 direction) {	
		return RadToDeg(DirectionToRotationRad(direction));
	}

	// Returns the euler angles in radians that correspond to that direction; 2 packed direction
	ECS_INLINE Vector8 ECS_VECTORCALL DirectionToRotationRad(Vector8 direction) {
		/* Scalar algorithm
		float x_angle = fmod(atan2(direction.y, direction.z), PI);
		float y_angle = fmod(atan2(direction.z, direction.x), PI);
		float z_angle = fmod(atan2(direction.y, direction.x), PI);*/

		Vector8 tangent_numerator = permute8<1, 2, 1, V_DC, 5, 6, 5, V_DC>(direction);
		Vector8 tangent_denominator = permute8<2, 0, 0, V_DC, 6, 0, 0, V_DC>(direction);

		Vector8 radians = atan2(tangent_numerator, tangent_denominator);
		return VectorFmod(radians, Vector8(PI));
	}

	// Returns the euler angles in angles that correspond to that direction; 2 packed direction
	ECS_INLINE Vector8 ECS_VECTORCALL DirectionToRotation(Vector8 direction) {
		return RadToDeg(DirectionToRotationRad(direction));
	}

	// Returns the euler angles in radians that correspond to that direction
	ECS_INLINE float3 DirectionToRotation(float3 direction) {
		float3 result;

		Vector4 vector_dir(direction);
		vector_dir = DirectionToRotation(vector_dir);
		vector_dir.StorePartialConstant<3>(&result);

		return result;
	}

	// --------------------------------------------------------------------------------------------------------------

#pragma endregion

	// --------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------

}