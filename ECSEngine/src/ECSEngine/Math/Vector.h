#pragma once
#include "../Core.h"
#include "VCLExtensions.h"
#include "../../Dependencies/VCL-version2/vectormath_trig.h"
#include "BaseVector.h"
#include "../Containers/Stream.h"

#define ECS_SIMD_VECTOR_EPSILON_VALUE 0.00001f
#define ECS_SIMD_VECTOR_PARALLEL_EPSILON_VALUE 0.0015f

namespace ECSEngine {

	enum VectorOperationPrecision {
		ECS_VECTOR_PRECISE,
		ECS_VECTOR_ACCURATE,
		ECS_VECTOR_FAST
	};

	enum ECS_SIMD_VECTOR_COMPONENT : unsigned char {
		ECS_SIMD_VECTOR_X,
		ECS_SIMD_VECTOR_Y,
		ECS_SIMD_VECTOR_Z,
		ECS_SIMD_VECTOR_W
	};

	namespace VectorGlobals {
		ECSENGINE_API extern Vec8f ONE;
		ECSENGINE_API extern Vec8f EPSILON;
		ECSENGINE_API extern Vec8i INFINITY_MASK;
		ECSENGINE_API extern Vec8i SIGN_MASK;

		// Investigate if having a global stored value
		// is faster than doing a blend. If the one vector
		// is used somewhere else we can have that value already in a register
		// instead of having to do a cold read and a blend is basically 1 cycle
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL DegToRad(float3 angles) {
		return BasicTypeDegToRad(angles);
	}

	ECS_INLINE float3 ECS_VECTORCALL RadToDeg(float3 angles) {
		return BasicTypeRadToDeg(angles);
	}

	ECSENGINE_API Vec8f ECS_VECTORCALL DegToRad(Vec8f angles);

	ECSENGINE_API Vec8f ECS_VECTORCALL RadToDeg(Vec8f angles);

	ECSENGINE_API Vector3 ECS_VECTORCALL DegToRad(Vector3 angles);

	ECSENGINE_API Vector3 ECS_VECTORCALL RadToDeg(Vector3 angles);

	ECSENGINE_API Vector4 ECS_VECTORCALL DegToRad(Vector4 angles);

	ECSENGINE_API Vector4 ECS_VECTORCALL RadToDeg(Vector4 angles);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector3 ECS_VECTORCALL ZeroVector() {
		return Vector3(ZeroVectorFloat(), ZeroVectorFloat(), ZeroVectorFloat());
	}

	ECS_INLINE Vector3 ECS_VECTORCALL OneVector() {
		return Vector3(VectorGlobals::ONE, VectorGlobals::ONE, VectorGlobals::ONE);
	}

	// The value will be splatted along all values
	ECS_INLINE Vector3 ECS_VECTORCALL RightVector() {
		return Vector3(VectorGlobals::ONE, ZeroVectorFloat(), ZeroVectorFloat());
	}

	ECS_INLINE float3 ECS_VECTORCALL GetRightVector() {
		return float3(1.0f, 0.0f, 0.0f);
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL RightVector() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return RightVector();
		}
		else {
			return GetRightVector();
		}
	}

	// The value will be splatted along all values
	ECS_INLINE Vector3 ECS_VECTORCALL UpVector() {
		return Vector3(ZeroVectorFloat(), VectorGlobals::ONE, ZeroVectorFloat());
	}

	ECS_INLINE float3 ECS_VECTORCALL GetUpVector() {
		return float3(0.0f, 1.0f, 0.0f);
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL UpVector() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return UpVector();
		}
		else {
			return GetUpVector();
		}
	}

	// The value will be splatted along all values
	ECS_INLINE Vector3 ECS_VECTORCALL ForwardVector() {
		return Vector3(ZeroVectorFloat(), ZeroVectorFloat(), VectorGlobals::ONE);
	}

	ECS_INLINE float3 ECS_VECTORCALL GetForwardVector() {
		return float3(0.0f, 0.0f, 1.0f);
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL ForwardVector() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return ForwardVector();
		}
		else {
			return GetForwardVector();
		}
	}

	ECS_INLINE Vector3 ECS_VECTORCALL MinusOneVector() {
		return ZeroVector() - OneVector();
	}

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool IsInfiniteMask(float value);

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsInfiniteMask(Vec8f value);

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL OneVector() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return VectorGlobals::ONE;
		}
		else {
			return 1.0f;
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL SingleZeroVector() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return Vec8f(ZeroVectorFloat());
		}
		else {
			return 0.0f;
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL ZeroVectorGeneric() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return ZeroVector();
		}
		else {
			return float3::Splat(0.0f);
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL SingleValueVector(float value) {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return Vec8f(value);
		}
		else {
			return value;
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL TrueVectorMask() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vec8f>) {
			return Vec8fb(true);
		}
		else {
			return true;
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL FalseVectorMask() {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same<Vector, Vec8f>) {
			return Vec8fb(ZeroVectorFloat());
		}
		else {
			return false;
		}
	}

	template<typename Mask>
	ECS_INLINE auto ECS_VECTORCALL NegateMask(Mask mask) {
		if constexpr (std::is_same_v<Mask, SIMDVectorMask>) {
			// The comment says that for masks it is more efficient
			// To use ~ than !
			return ~mask;
		}
		else {
			return !mask;
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL VectorGenericLess(Vector a, Vector b) {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vector4> || std::is_same_v<Vector, Vec8f>) {
			return a < b;
		}
		else {
			return BasicTypeLess(a, b);
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL VectorGenericLessEqual(Vector a, Vector b) {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vector4> || std::is_same_v<Vector, Vec8f>) {
			return a < b;
		}
		else {
			return BasicTypeLessEqual(a, b);
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL VectorGenericGreater(Vector a, Vector b) {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vector4> || std::is_same_v<Vector, Vec8f>) {
			return a > b;
		}
		else {
			return BasicTypeGreater(a, b);
		}
	}

	template<typename Vector>
	ECS_INLINE auto ECS_VECTORCALL VectorGenericGreaterEqual(Vector a, Vector b) {
		if constexpr (std::is_same_v<Vector, Vector3> || std::is_same_v<Vector, Vector4> || std::is_same_v<Vector, Vec8f>) {
			return a > b;
		}
		else {
			return BasicTypeGreaterEqual(a, b);
		}
	}

	// --------------------------------------------------------------------------------------------------------------

	/*
		Unfortunately, having two functions Select with (bool, float, float) and
		(SIMDVectorMask, Vec8f, Vec8f) does not work since the Vec8f version for
		some reason converts the floats to their type and then it uses that overload
		To solve this, create a separate template function that makes distinction between
		These 2 types
	*/

	// Use only for floats or Vec8f - we cannot have 
	template<typename Type, typename MaskType>
	ECS_INLINE Type ECS_VECTORCALL SelectSingle(MaskType mask, Type a, Type b) {
		if constexpr (std::is_same_v<Type, float>) {
			return mask ? a : b;
		}
		else if constexpr (std::is_same_v<Type, Vec8f>) {
			return select(mask, a, b);
		}
		else {
			static_assert(false, "Invalid types for SelectSingle!");
		}
	}

	ECS_INLINE float3 ECS_VECTORCALL Select(bool mask, float3 a, float3 b) {
		return mask ? a : b;
	}

	ECS_INLINE float4 ECS_VECTORCALL Select(bool mask, float4 a, float4 b) {
		return mask ? a : b;
	}

	// The ternary operator ? :
	ECSENGINE_API Vector3 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector3 a, Vector3 b);

	// The ternary operator ? :
	// The result is the same as a with the component selected
	ECSENGINE_API Vector3 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector3 a, Vector3 b, ECS_SIMD_VECTOR_COMPONENT component);

	// The ternary operator ? :
	ECSENGINE_API Vector4 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector4 a, Vector4 b);

	// The ternary operator ? :
	// The result is the same as a with the component selected
	ECSENGINE_API Vector4 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector4 a, Vector4 b, ECS_SIMD_VECTOR_COMPONENT component);

	// We need a template to distinguish between the auto conversion of float to Vec8f
	template<typename SingleValue>
	ECS_INLINE SingleValue ECS_VECTORCALL Min(SingleValue a, SingleValue b) {
		if constexpr (std::is_same_v<float, SingleValue>) {
			return min(a, b);
		}
		else {
			return min(a, b);
		}
	}

	// We need a template to distinguish between the auto conversion of float to Vec8f
	template<typename SingleValue>
	ECS_INLINE SingleValue ECS_VECTORCALL Max(SingleValue a, SingleValue b) {
		if constexpr (std::is_same_v<float, SingleValue>) {
			return max(a, b);
		}
		else {
			return max(a, b);
		}
	}

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector3 ECS_VECTORCALL Min(Vector3 a, Vector3 b);

	ECSENGINE_API Vector4 ECS_VECTORCALL Min(Vector4 a, Vector4 b);

	ECSENGINE_API Vector3 ECS_VECTORCALL Max(Vector3 a, Vector3 b);

	ECSENGINE_API Vector4 ECS_VECTORCALL Max(Vector4 a, Vector4 b);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool IsNanMask(float value);

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsNanMask(Vec8f value);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL Abs(float3 vector) {
		return BasicTypeAbs(vector);
	}

	ECS_INLINE float4 ECS_VECTORCALL Abs(float4 vector) {
		return BasicTypeAbs(vector);
	}

	// Again conversions ruin these functions
	template<typename SingleValue>
	ECS_INLINE SingleValue ECS_VECTORCALL AbsSingle(SingleValue value) {
		if constexpr (std::is_same_v<SingleValue, float>) {
			return fabsf(value);
		}
		else if constexpr (std::is_same_v<SingleValue, Vec8f> || std::is_same_v<SingleValue, __m256>) {
			return abs(value);
		}
		else {
			static_assert(false, "Invalid single value for AbsSingle");
		}
	}

	ECSENGINE_API Vector3 ECS_VECTORCALL Abs(Vector3 vector);

	ECSENGINE_API Vector4 ECS_VECTORCALL Abs(Vector4 vector);

	template<typename SingleValue>
	ECS_INLINE SingleValue ECS_VECTORCALL AbsoluteDifferenceSingle(SingleValue a, SingleValue b) {
		if constexpr (std::is_same_v<SingleValue, float> || std::is_same_v<SingleValue, Vec8f> || std::is_same_v<SingleValue, __m256>) {
			return AbsSingle(a - b);
		}
		else {
			static_assert(false, "Invalid single value for AbsoluteDifferenceSingle");
		}
	}

	ECS_INLINE float3 ECS_VECTORCALL AbsoluteDifference(float3 a, float3 b) {
		return Abs(a - b);
	}

	ECS_INLINE float4 ECS_VECTORCALL AbsoluteDifference(float4 a, float4 b) {
		return Abs(a - b);
	}

	ECSENGINE_API Vector3 ECS_VECTORCALL AbsoluteDifference(Vector3 a, Vector3 b);
	
	ECSENGINE_API Vector4 ECS_VECTORCALL AbsoluteDifference(Vector4 a, Vector4 b);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float ECS_VECTORCALL Dot(float2 first, float2 second);

	ECSENGINE_API float ECS_VECTORCALL Dot(float3 first, float3 second);

	ECSENGINE_API float ECS_VECTORCALL Dot(float4 first, float4 second);

	ECSENGINE_API Vec8f ECS_VECTORCALL Dot(Vector3 first, Vector3 second);
	
	ECSENGINE_API Vec8f ECS_VECTORCALL Dot(Vector4 first, Vector4 second);

	// --------------------------------------------------------------------------------------------------------------

	template<typename SingleValue>
	ECS_INLINE auto CompareMaskSingle(SingleValue first, SingleValue second) {
		if constexpr (std::is_same_v<SingleValue, float>) {
			return abs(first - second) < ECS_SIMD_VECTOR_EPSILON_VALUE;
		}
		else if constexpr (std::is_same_v<SingleValue, Vec8f> || std::is_same_v<SingleValue, __m256>) {
			return abs(first - second) < VectorGlobals::EPSILON;
		}
		else {
			static_assert(false, "Invalid compare mask single value - only floats or Vec8f are accepted");
		}
	}

	template<typename SingleValue>
	ECS_INLINE auto CompareMaskSingle(SingleValue first, SingleValue second, SingleValue epsilon) {
		if constexpr (std::is_same_v<SingleValue, float>) {
			return abs(first - second) < epsilon;
		}
		else if constexpr (std::is_same_v<SingleValue, Vec8f> || std::is_same_v<SingleValue, __m256>) {
			return abs(first - second) < epsilon;
		}
		else {
			static_assert(false, "Invalid compare mask single value - only floats or Vec8f are accepted");
		}
	}

	ECSENGINE_API bool ECS_VECTORCALL CompareMask(float3 first, float3 second, float3 epsilon = float3::Splat(ECS_SIMD_VECTOR_EPSILON_VALUE));

	ECSENGINE_API bool ECS_VECTORCALL CompareMask(float4 first, float4 second, float4 epsilon = float4::Splat(ECS_SIMD_VECTOR_EPSILON_VALUE));

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CompareMask(Vector3 first, Vector3 second, Vec8f epsilon = VectorGlobals::EPSILON);

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CompareMask(Vector4 first, Vector4 second, Vec8f epsilon = VectorGlobals::EPSILON);

	// This version allow for caching of the cosine value outside a loop and reusing it across calls
	ECSENGINE_API bool ECS_VECTORCALL CompareAngleNormalizedCosineMask(float3 first_normalized, float3 second_normalized, float cosine);

	// This version allow for caching of the cosine value outside a loop and reusing it across calls
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CompareAngleNormalizedCosineMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f cosine);

	// Both directions need to be normalized beforehand
	ECS_INLINE bool ECS_VECTORCALL CompareAngleNormalizedRadMask(float3 first_normalized, float3 second_normalized, float radians) {
		return CompareAngleNormalizedCosineMask(first_normalized, second_normalized, cos(radians));
	}

	// Both directions need to be normalized beforehand
	ECS_INLINE SIMDVectorMask ECS_VECTORCALL CompareAngleNormalizedRadMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f radians) {
		return CompareAngleNormalizedCosineMask(first_normalized, second_normalized, cos(radians));
	}

	ECSENGINE_API bool ECS_VECTORCALL CompareAngleRadMask(float3 first, float3 second, float radians);

	// The angle must be given in radians
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CompareAngleRadMask(Vector3 first, Vector3 second, Vec8f radians);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 ECS_VECTORCALL Lerp(float3 a, float3 b, float percentage);

	ECSENGINE_API float4 ECS_VECTORCALL Lerp(float4 a, float4 b, float percentage);
	
	ECSENGINE_API Vector3 ECS_VECTORCALL Lerp(Vector3 a, Vector3 b, Vec8f percentages);

	ECSENGINE_API Vector4 ECS_VECTORCALL Lerp(Vector4 a, Vector4 b, Vec8f percentages);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 ECS_VECTORCALL ProjectPointOnLineDirection(float3 line_point, float3 line_direction, float3 point);

	ECSENGINE_API Vector3 ECS_VECTORCALL ProjectPointOnLineDirection(Vector3 line_point, Vector3 line_direction, Vector3 point);

	ECSENGINE_API float3 ECS_VECTORCALL ProjectPointOnLineDirectionNormalized(float3 line_point, float3 line_direction_normalized, float3 point);
	
	// This version is much faster than the other one since it avoids a dot and a division
	ECSENGINE_API Vector3 ECS_VECTORCALL ProjectPointOnLineDirectionNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 point);

	ECSENGINE_API float3 ECS_VECTORCALL ProjectPointOnLine(float3 line_a, float3 line_b, float3 point);

	ECSENGINE_API Vector3 ECS_VECTORCALL ProjectPointOnLine(Vector3 line_a, Vector3 line_b, Vector3 point);

	// --------------------------------------------------------------------------------------------------------------

	// For a point that is on the line AB, it clamps the point to be inside the AB segment
	ECSENGINE_API float3 ECS_VECTORCALL ClampPointToSegment(float3 line_a, float3 line_b, float3 point);


	// At the moment, this function is not yet implemented
	// For a point that is on the line AB, it clamps the point to be inside the AB segment
	//ECSENGINE_API Vector3 ECS_VECTORCALL ClampPointToSegment(Vector3 line_a, Vector3 line_b, Vector3 point);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 ECS_VECTORCALL Cross(float3 a, float3 b);

	ECSENGINE_API Vector3 ECS_VECTORCALL Cross(Vector3 a, Vector3 b);

	// Returns a perpendicular vector from the line to the point
	ECSENGINE_API float3 ECS_VECTORCALL TripleProduct(float3 line_a, float3 line_b, float3 point);

	// Returns a perpendicular vector from the line to the point
	ECSENGINE_API Vector3 ECS_VECTORCALL TripleProduct(Vector3 line_a, Vector3 line_b, Vector3 point);

	// Returns a perpendicular vector from the line to the point
	// When the line direction and line to point directions are known
	// If unsure about the correct order, use the variant with the 3 points
	ECSENGINE_API float3 ECS_VECTORCALL TripleProduct(float3 line_direction, float3 line_point_direction);

	// Returns a perpendicular vector from the line to the point
	// When the line direction and line to point directions are known
	// If unsure about the correct order, use the variant with the 3 points
	ECSENGINE_API Vector3 ECS_VECTORCALL TripleProduct(Vector3 line_direction, Vector3 line_point_direction);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float ECS_VECTORCALL SquareLength(float2 vector) {
		return Dot(vector, vector);
	}

	ECS_INLINE float ECS_VECTORCALL SquareLength(float3 vector) {
		return Dot(vector, vector);
	}

	ECS_INLINE float ECS_VECTORCALL SquareLength(float4 vector) {
		return Dot(vector, vector);
	}

	ECS_INLINE Vec8f ECS_VECTORCALL SquareLength(Vector3 vector) {
		return Dot(vector, vector);
	}

	ECS_INLINE Vec8f ECS_VECTORCALL SquareLength(Vector4 vector) {
		return Dot(vector, vector);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vec8f ECS_VECTORCALL Length(Vector3 vector);
	
	ECSENGINE_API Vec8f ECS_VECTORCALL Length(Vector4 vector);

	ECSENGINE_API float ECS_VECTORCALL Length(float2 vector);

	ECSENGINE_API float ECS_VECTORCALL Length(float3 vector);

	ECSENGINE_API float ECS_VECTORCALL Length(float4 vector);

	// --------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float ECS_VECTORCALL ReciprocalLength(float3 vector);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float ECS_VECTORCALL ReciprocalLength(float4 vector);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vec8f ECS_VECTORCALL ReciprocalLength(Vector3 vector);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vec8f ECS_VECTORCALL ReciprocalLength(Vector4 vector);

	// --------------------------------------------------------------------------------------------------------------

	// Project the point on the line and then get the distance between these 2 points

	ECSENGINE_API float ECS_VECTORCALL SquaredDistanceToLine(float3 line_point, float3 line_direction, float3 point);

	ECSENGINE_API Vec8f ECS_VECTORCALL SquaredDistanceToLine(Vector3 line_point, Vector3 line_direction, Vector3 point);

	// This version is much faster than the other one since it avoids a dot and a division
	ECSENGINE_API float ECS_VECTORCALL SquaredDistanceToLineNormalized(float3 line_point, float3 line_direction_normalized, float3 point);

	// This version is much faster than the other one since it avoids a dot and a division
	ECSENGINE_API Vec8f ECS_VECTORCALL SquaredDistanceToLineNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 point);

	ECS_INLINE float ECS_VECTORCALL DistanceToLine(float3 line_point, float3 line_direction, float3 point) {
		return sqrt(SquaredDistanceToLine(line_point, line_direction, point));
	}

	ECSENGINE_API Vec8f ECS_VECTORCALL DistanceToLine(Vector3 line_point, Vector3 line_direction, Vector3 point);

	// This version is much faster than the other one since it avoids a dot and a division
	ECS_INLINE float ECS_VECTORCALL DistanceToLineNormalized(float3 line_point, float3 line_direction_normalized, float3 point) {
		return sqrt(SquaredDistanceToLineNormalized(line_point, line_direction_normalized, point));
	}

	// This version is much faster than the other one since it avoids a dot and a division
	ECSENGINE_API Vec8f ECS_VECTORCALL DistanceToLineNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 point);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float2 ECS_VECTORCALL Normalize(float2 vector);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float3 ECS_VECTORCALL Normalize(float3 vector);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float4 ECS_VECTORCALL Normalize(float4 vector);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vector3 ECS_VECTORCALL Normalize(Vector3 vector);

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vector4 ECS_VECTORCALL Normalize(Vector4 vector);

	// Returns true if a vector's squared length is already normalized
	ECS_INLINE bool IsNormalizedSquareLengthMask(float squared_length) {
		return abs(squared_length - 1.0f) < ECS_SIMD_VECTOR_EPSILON_VALUE;
	}

	// Returns true if a vector's squared length is already normalized
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsNormalizedSquareLengthMask(Vec8f squared_length);

	// Returns true if a vector's length is already normalized
	ECS_INLINE bool IsNormalizedMask(float length) {
		return abs(length - 1.0f) < ECS_SIMD_VECTOR_EPSILON_VALUE;
	}

	// Returns true if a vector's length is already normalized
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsNormalizedMask(Vec8f length);

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly)
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float3 ECS_VECTORCALL NormalizeIfNot(float3 vector);

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly)
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float4 ECS_VECTORCALL NormalizeIfNot(float4 vector);

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly). It exists only to match the SIMD function
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECS_INLINE float3 ECS_VECTORCALL NormalizeIfNot(float3 vector, size_t vector_count) {
		return NormalizeIfNot<precision>(vector);
	}

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly). It exists only to match the SIMD function
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECS_INLINE float4 ECS_VECTORCALL NormalizeIfNot(float4 vector, size_t vector_count) {
		return NormalizeIfNot<precision>(vector);
	}

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly). The vector count is used to determine whether
	// Or not a normalization is needed or not
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vector3 ECS_VECTORCALL NormalizeIfNot(Vector3 vector, size_t vector_count);

	// This only performs the square root and the division if the vector is not normalized already
	// This only applies for the precise and the accurate versions (the fast version already produces
	// a value that can be used to multiply directly). The vector count is used to determine whether
	// Or not a normalization is needed or not
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vector4 ECS_VECTORCALL NormalizeIfNot(Vector4 vector, size_t vector_count);

	// Returns 1.0f / value when using precise mode
	// And an approximate version when using ACCURATE/FAST
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float OneDividedVector(float value);

	// Returns 1.0f / value when using precise mode
	// And an approximate version when using ACCURATE/FAST
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float3 ECS_VECTORCALL OneDividedVector(float3 value);

	// Returns 1.0f / value when using precise mode
	// And an approximate version when using ACCURATE/FAST
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vec8f ECS_VECTORCALL OneDividedVector(Vec8f value);

	// Returns 1.0f / value when using precise mode
	// And an approximate version when using ACCURATE/FAST
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vector3 ECS_VECTORCALL OneDividedVector(Vector3 value);

	// --------------------------------------------------------------------------------------------------------------

	// For both Is Parallel and Is Perpendicular we need to normalize both directions
	// Since we need to compare against a fixed epsilon and different magnitudes could
	// produce different results

	// Both directions need to be normalized beforehand - if you want consistent behaviour
	// The epsilon needs to be reasonable large
	ECSENGINE_API bool ECS_VECTORCALL IsParallelMask(float3 first_normalized, float3 second_normalized, float epsilon = ECS_SIMD_VECTOR_PARALLEL_EPSILON_VALUE);

	// Both directions need to be normalized beforehand - if you want consistent behaviour
	// The epsilon needs to be reasonable large
	// Returns a SIMD mask that can be used to perform a selection
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsParallelMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f epsilon = ECS_SIMD_VECTOR_PARALLEL_EPSILON_VALUE);

	// --------------------------------------------------------------------------------------------------------------

	// Both directions need to be normalized beforehand
	// This version allows caching of the cosine value
	ECSENGINE_API bool ECS_VECTORCALL IsParallelAngleCosineMask(float3 first_normalized, float3 second_normalized, float cosine);

	// Both directions need to be normalized beforehand
	// This version allows caching of the cosine value
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsParallelAngleCosineMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f cosine);

	// Both directions need to be normalized beforehand
	ECS_INLINE bool ECS_VECTORCALL IsParallelAngleMask(float3 first_normalized, float3 second_normalized, float radians) {
		return IsParallelAngleCosineMask(first_normalized, second_normalized, cos(radians));
	}

	// Both directions need to be normalized beforehand
	ECS_INLINE SIMDVectorMask ECS_VECTORCALL IsParallelAngleMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f radians) {
		return IsParallelAngleCosineMask(first_normalized, second_normalized, cos(radians));
	}

	// --------------------------------------------------------------------------------------------------------------

	// The cosine (dot) between them needs to be 0

	ECSENGINE_API bool ECS_VECTORCALL IsPerpendicularMask(float3 first_normalized, float3 second_normalized, float epsilon = ECS_SIMD_VECTOR_EPSILON_VALUE);

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsPerpendicularMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f epsilon = VectorGlobals::EPSILON);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool ECS_VECTORCALL IsPerpendicularAngleCosineMask(float3 first_normalized, float3 second_normalized, float cosine);

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsPerpendicularAngleCosineMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f cosine);

	// The radians angle needs to be smaller than half pi
	ECS_INLINE bool ECS_VECTORCALL IsPerpendicularAngleMask(float3 first_normalized, float3 second_normalized, float radians) {
		return IsPerpendicularAngleCosineMask(first_normalized, second_normalized, cos(radians));
	}

	// The radians angle needs to be smaller than half pi
	ECS_INLINE SIMDVectorMask ECS_VECTORCALL IsPerpendicularAngleMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f radians) {
		return IsPerpendicularAngleCosineMask(first_normalized, second_normalized, cos(radians));
	}

	// --------------------------------------------------------------------------------------------------------------
	
	// We already have a function in MathHelpers.h for normal floats, but to keep
	// The same overload with the vector one, write one here
	
	ECS_INLINE float ClampSingle(float value, float min, float max) {
		bool is_less = value < min;
		bool is_greater = value > max;
		return is_greater ? max : (is_less ? min : value);
	}

	ECS_INLINE Vec8f ECS_VECTORCALL ClampSingle(Vec8f value, Vec8f min, Vec8f max) {
		SIMDVectorMask less_mask = value < min;
		SIMDVectorMask greater_mask = value > max;
		return SelectSingle(greater_mask, max, SelectSingle(less_mask, min, value));
	}

	ECSENGINE_API Vector3 ECS_VECTORCALL Clamp(Vector3 value, Vector3 min, Vector3 max);

	ECSENGINE_API Vector4 ECS_VECTORCALL Clamp(Vector4 value, Vector4 min, Vector4 max);

	template<bool equal_min = false, bool equal_max = false>
	ECSENGINE_API bool ECS_VECTORCALL IsInRangeMask(float3 value, float3 min, float3 max);

	template<bool equal_min = false, bool equal_max = false>
	ECSENGINE_API bool ECS_VECTORCALL IsInRangeMask(float4 value, float4 min, float4 max);

	// Again, conversions from float to Vec8f take precedence...
	template<bool equal_min = false, bool equal_max = false, typename SingleValue>
	ECS_INLINE auto ECS_VECTORCALL IsInRangeMaskSingle(SingleValue value, SingleValue min, SingleValue max) {
		static_assert(std::is_same_v<SingleValue, float> || std::is_same_v<SingleValue, Vec8f> || std::is_same_v<SingleValue, __m256>,
			"Invalid single type for IsInRangeMaskSingle");
		if constexpr (equal_min) {
			if constexpr (equal_max) {
				return value >= min && value <= max;
			}
			else {
				return value >= min && value < max;
			}
		}
		else {
			if constexpr (equal_max) {
				return value > min && value <= max;
			}
			else {
				return value > min && value < max;
			}
		}
	}

	template<bool equal_min = false, bool equal_max = false>
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsInRangeMask(Vector3 value, Vector3 min, Vector3 max);

	template<bool equal_min = false, bool equal_max = false>
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsInRangeMask(Vector4 value, Vector4 min, Vector4 max);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 ECS_VECTORCALL Reflect(float3 incident, float3 normal);

	ECSENGINE_API Vector3 ECS_VECTORCALL Reflect(Vector3 incident, Vector3 normal);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 ECS_VECTORCALL Refract(float3 incident, float3 normal, float refraction_index);

	// It exists only to match the SIMD function
	ECS_INLINE float3 ECS_VECTORCALL Refract(float3 incident, float3 normal, float refraction_index, size_t vector_count) {
		return Refract(incident, normal, refraction_index);
	}

	ECSENGINE_API Vector3 ECS_VECTORCALL Refract(Vector3 incident, Vector3 normal, Vec8f refraction_index, size_t vector_count);

	// --------------------------------------------------------------------------------------------------------------

	// The vectors need to be normalized before
	// Returns the value of the angle between the 2 vectors in radians
	ECSENGINE_API float ECS_VECTORCALL AngleBetweenVectorsNormalizedRad(float3 a_normalized, float3 b_normalized);

	// The vectors need to be normalized before
	// Returns the value of the angle between the 2 vectors in radians
	ECSENGINE_API Vec8f ECS_VECTORCALL AngleBetweenVectorsNormalizedRad(Vector3 a_normalized, Vector3 b_normalized);

	// Returns the value of the angle between the 2 vectors in radians
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API float ECS_VECTORCALL AngleBetweenVectorsRad(float3 a, float3 b);
		
	// Returns the value of the angle between the 2 vectors in radians
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECSENGINE_API Vec8f ECS_VECTORCALL AngleBetweenVectorsRad(Vector3 a, Vector3 b);
	
	// Return the value of the angle between the 2 vectors in degrees
	ECS_INLINE float ECS_VECTORCALL AngleBetweenVectorsNormalized(float3 a_normalized, float3 b_normalized) {
		return RadToDeg(AngleBetweenVectorsNormalizedRad(a_normalized, b_normalized));
	}

	// Return the value of the angle between the 2 vectors in degrees
	ECS_INLINE Vec8f ECS_VECTORCALL AngleBetweenVectorsNormalized(Vector3 a_normalized, Vector3 b_normalized) {
		return RadToDeg(AngleBetweenVectorsNormalizedRad(a_normalized, b_normalized));
	}

	// Return the value of the angle between the 2 vectors in degrees
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECS_INLINE float ECS_VECTORCALL AngleBetweenVectors(float3 a, float3 b) {
		return RadToDeg(AngleBetweenVectorsRad<precision>(a, b));
	}

	// Return the value of the angle between the 2 vectors in degrees
	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECS_INLINE Vec8f ECS_VECTORCALL AngleBetweenVectors(Vector3 a, Vector3 b) {
		return RadToDeg(AngleBetweenVectorsRad<precision>(a, b));
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float Fmadd(float a, float b, float c) {
		return a * b + c;
	}

	ECS_INLINE float Fmsub(float a, float b, float c) {
		return a * b - c;
	}

	// a * b + c
	ECSENGINE_API float3 ECS_VECTORCALL Fmadd(float3 a, float3 b, float3 c);

	// a * b + c
	ECSENGINE_API float4 ECS_VECTORCALL Fmadd(float4 a, float4 b, float4 c);

	// a * b + c
	ECSENGINE_API Vector3 ECS_VECTORCALL Fmadd(Vector3 a, Vector3 b, Vector3 c);

	// a * b + c
	ECSENGINE_API Vector4 ECS_VECTORCALL Fmadd(Vector4 a, Vector4 b, Vector4 c);

	// a * b - c
	ECSENGINE_API float3 ECS_VECTORCALL Fmsub(float3 a, float3 b, float3 c);

	// a * b - c
	ECSENGINE_API float4 ECS_VECTORCALL Fmsub(float4 a, float4 b, float4 c);

	// a * b - c
	ECSENGINE_API Vector3 ECS_VECTORCALL Fmsub(Vector3 a, Vector3 b, Vector3 c);

	// a * b - c
	ECSENGINE_API Vector4 ECS_VECTORCALL Fmsub(Vector4 a, Vector4 b, Vector4 c);

	ECS_INLINE float Fmod(float a, float b) {
		return fmodf(a, b);
	}

	ECSENGINE_API Vec8f ECS_VECTORCALL Fmod(Vec8f a, Vec8f b);

	// a % b
	ECSENGINE_API float3 ECS_VECTORCALL Fmod(float3 a, float3 b);

	// a % b
	ECSENGINE_API Vector3 ECS_VECTORCALL Fmod(Vector3 a, Vector3 b);

	// --------------------------------------------------------------------------------------------------------------

	// Returns the euler angles in radians that correspond to that direction
	ECSENGINE_API Vector3 ECS_VECTORCALL DirectionToRotationEulerRad(Vector3 direction);

	// Returns the euler angles in angles that correspond to that direction
	ECS_INLINE Vector3 ECS_VECTORCALL DirectionToRotationEuler(Vector3 direction) {
		return RadToDeg(DirectionToRotationEulerRad(direction));
	}

	// Returns the euler angles in radians that correspond to that direction
	ECSENGINE_API float3 ECS_VECTORCALL DirectionToRotationEulerRad(float3 direction);

	// Returns the euler angles in angles that correspond to that direction
	ECS_INLINE float3 ECS_VECTORCALL DirectionToRotationEuler(float3 direction) {
		return RadToDeg(DirectionToRotationEulerRad(direction));
	}

	// --------------------------------------------------------------------------------------------------------------

	// Constructs an orthonormal basis out of a given direction
	ECSENGINE_API void ECS_VECTORCALL OrthonormalBasis(Vector3 direction_normalized, Vector3* direction_first, Vector3* direction_second);

	// --------------------------------------------------------------------------------------------------------------

	// Returns true if the point is in view, else false
	ECSENGINE_API bool CullClipSpaceXMask(float4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CullClipSpaceXMask(Vector4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API bool CullClipSpaceYMask(float4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CullClipSpaceYMask(Vector4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API bool CullClipSpaceZMask(float4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CullClipSpaceZMask(Vector4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API bool CullClipSpaceWMask(float4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CullClipSpaceWMask(Vector4 vector);

	// Returns true if the point is in view, else false
	ECSENGINE_API bool CullClipSpaceMask(float4 vector);

	// Returns a mask where vertices which should be kept have their lane set to true
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL CullClipSpaceMask(Vector4 vector);

	// Converts from clip space to NDC
	ECSENGINE_API float4 ECS_VECTORCALL ClipSpaceToNDC(float4 vector);

	// Converts from clip space to NDC
	ECSENGINE_API Vector4 ECS_VECTORCALL ClipSpaceToNDC(Vector4 vector);

	ECS_INLINE float4 ECS_VECTORCALL PerspectiveDivide(float4 vector) {
		// Same as ClipSpaceToNDC
		return ClipSpaceToNDC(vector);
	}

	ECS_INLINE Vector4 ECS_VECTORCALL PerspectiveDivide(Vector4 vector) {
		// Same as ClipSpaceToNDC
		return ClipSpaceToNDC(vector);
	}

	// --------------------------------------------------------------------------------------------------------------

	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECSENGINE_API float3 ECS_VECTORCALL GetRightVectorForDirection(float3 direction_normalized);

	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECSENGINE_API Vector3 ECS_VECTORCALL GetRightVectorForDirection(Vector3 direction_normalized);

	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECSENGINE_API float3 ECS_VECTORCALL GetUpVectorForDirection(float3 direction_normalized);


	// The direction needs to be normalized before hand
	// The output is normalized as well
	ECSENGINE_API Vector3 ECS_VECTORCALL GetUpVectorForDirection(Vector3 direction_normalized);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 ECS_VECTORCALL ClosestPoint(Stream<float3> points, float3 origin);

	// --------------------------------------------------------------------------------------------------------------

	// Returns true if the test point is on the same side of the line AB as the reference point, else false
	// All points must be coplanar
	ECSENGINE_API bool ECS_VECTORCALL PointSameLineHalfPlane(float3 line_a, float3 line_b, float3 reference_point, float3 test_point);

	// Returns true if the test point is on the same side of the line AB as the reference point, else false
	// All points must be coplanar
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL PointSameLineHalfPlane(Vector3 line_a, Vector3 line_b, Vector3 reference_point, Vector3 test_point);

	// Returns true if the test point is on the same side of the line AB as the reference point, else false
	// All points must be coplanar
	ECSENGINE_API bool ECS_VECTORCALL PointSameLineHalfPlaneNormalized(float3 line_point, float3 line_direction_normalized, float3 reference_point, float3 test_point);

	// Returns true if the test point is on the same side of the line AB as the reference point, else false
	// All points must be coplanar
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL PointSameLineHalfPlaneNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 reference_point, Vector3 test_point);

	// Returns true if the test point is on the same side of the line AB as the reference point, else false
	// All points must be coplanar. This version takes the projected test point as input. The projected test
	// Point must have been projected on the line before
	ECSENGINE_API bool ECS_VECTORCALL PointSameLineHalfPlaneProjected(
		float3 line_point,  
		float3 reference_point, 
		float3 test_point, 
		float3 projected_test_point
	);

	// Returns true if the test point is on the same side of the line AB as the reference point, else false
	// All points must be coplanar. This version takes the projected test point as input. The projected test
	// Point must have been projected on the line before
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL PointSameLineHalfPlaneProjected(
		Vector3 line_point,
		Vector3 reference_point,
		Vector3 test_point,
		Vector3 projected_test_point
	);

	// --------------------------------------------------------------------------------------------------------------

	// Returns true if the point is on the same line AB. A relatively large epsilon should be used.
	// It will normalize the distances such that the magnitude won't affect the comparison. This version
	// Is quite fiddly with the epsilon, you could try to use the angle version which is much more consistent
	// But slower
	ECSENGINE_API bool ECS_VECTORCALL IsPointCollinear(float3 line_a, float3 line_b, float3 point, float epsilon = ECS_SIMD_VECTOR_PARALLEL_EPSILON_VALUE);

	// Returns true if the point is on the same line AB. A relatively large epsilon should be used.
	// It will normalize the distances such that the magnitude won't affect the comparison. This version
	// Is quite fiddly with the epsilon, you could try to use the angle version which is much more consistent
	// But slower
	ECSENGINE_API bool ECS_VECTORCALL IsPointCollinearDirection(float3 line_start, float3 line_direction_normalized, float3 point, float epsilon = ECS_SIMD_VECTOR_PARALLEL_EPSILON_VALUE);

	// This version is quite consistent as opposed to the epsilon based one
	ECSENGINE_API bool ECS_VECTORCALL IsPointCollinearByCosine(float3 line_a, float3 line_b, float3 point, float cosine);

	// This version is quite consistent as opposed to the epsilon based one
	ECSENGINE_API bool ECS_VECTORCALL IsPointCollinearDirectionByCosine(float3 line_start, float3 line_direction_normalized, float3 point, float cosine);

	// This version is quite consistent as opposed to the epsilon based one
	ECS_INLINE bool ECS_VECTORCALL IsPointCollinearByAngle(float3 line_a, float3 line_b, float3 point, float degrees) {
		return IsPointCollinearByCosine(line_a, line_b, point, cos(DegToRad(degrees)));
	}

	// This version is quite consistent as opposed to the epsilon based one
	ECS_INLINE bool ECS_VECTORCALL IsPointCollinearDirectionByAngle(
		float3 line_start, 
		float3 line_direction_normalized, 
		float3 point, 
		float degrees
	) {
		return IsPointCollinearDirectionByCosine(line_start, line_direction_normalized, point, cos(DegToRad(degrees)));
	}

	// --------------------------------------------------------------------------------------------------------------

	// The directions don't have to be necessarly normalized
	// The lines here are infinite in both directions
	ECSENGINE_API void ECS_VECTORCALL ClosestLinesPoints(
		float3 first_line_point,
		float3 first_line_direction,
		float3 second_line_point,
		float3 second_line_direction,
		float3* first_closest_point,
		float3* second_closest_point
	);

	// The directions don't have to be necessarly normalized
	// The lines here are infinite in both directions
	ECSENGINE_API void ECS_VECTORCALL ClosestLinesPoints(
		Vector3 first_line_point,
		Vector3 first_line_direction,
		Vector3 second_line_point,
		Vector3 second_line_direction,
		Vector3* first_closest_point,
		Vector3* second_closest_point
	);


	// This function determines the closest points between the lines
	// And returns the squared distance between those 2 points
	ECS_INLINE float ECS_VECTORCALL SquaredDistanceBetweenLinesPoints(
		float3 first_line_point,
		float3 first_line_direction,
		float3 second_line_point,
		float3 second_line_direction
	) {
		float3 first_closest_point, second_closest_point;
		ClosestLinesPoints(first_line_point, first_line_direction, second_line_point, second_line_direction, &first_closest_point, &second_closest_point);
		return SquareLength(first_closest_point - second_closest_point);
	}

	// This function determines the closest points between the lines
	// And returns the squared distance between those 2 points
	ECS_INLINE Vec8f ECS_VECTORCALL SquaredDistanceBetweenLinesPoints(
		Vector3 first_line_point,
		Vector3 first_line_direction,
		Vector3 second_line_point,
		Vector3 second_line_direction
	) {
		Vector3 first_closest_point, second_closest_point;
		ClosestLinesPoints(first_line_point, first_line_direction, second_line_point, second_line_direction, &first_closest_point, &second_closest_point);
		return SquareLength(first_closest_point - second_closest_point);
	}

	// This functions returns the pair of points on the segments
	// That are closest on those 2 segments. This function does not
	// Handle degenerate cases where the 2 points of a segment overlap
	ECSENGINE_API void ECS_VECTORCALL ClosestSegmentPoints(
		float3 first_line_A,
		float3 first_line_B,
		float3 second_line_A,
		float3 second_line_B,
		float3* first_closest_point,
		float3* second_closest_point
	);

	// This functions returns the pair of points on the segments
	// That are closest on those 2 segments. This function does not
	// Handle degenerate cases where the 2 points of a segment overlap
	ECSENGINE_API void ECS_VECTORCALL ClosestSegmentPoints(
		Vector3 first_line_A,
		Vector3 first_line_B,
		Vector3 second_line_A,
		Vector3 second_line_B,
		Vector3* first_closest_point,
		Vector3* second_closest_point
	);

	// This function determines the closest points between the segments
	// And returns the squared distance between those 2 points
	ECS_INLINE float ECS_VECTORCALL SquaredDistanceBetweenSegmentPoints(
		float3 first_line_A,
		float3 first_line_B,
		float3 second_line_A,
		float3 second_line_B
	) {
		float3 first_closest_point, second_closest_point;
		ClosestSegmentPoints(first_line_A, first_line_B, second_line_A, second_line_B, &first_closest_point, &second_closest_point);
		return SquareLength(first_closest_point - second_closest_point);
	}

	// This function determines the closest points between the segments
	// And returns the squared distance between those 2 points
	ECS_INLINE Vec8f ECS_VECTORCALL SquaredDistanceBetweenSegmentPoints(
		Vector3 first_line_A,
		Vector3 first_line_B,
		Vector3 second_line_A,
		Vector3 second_line_B
	) {
		Vector3 first_closest_point, second_closest_point;
		ClosestSegmentPoints(first_line_A, first_line_B, second_line_A, second_line_B, &first_closest_point, &second_closest_point);
		return SquareLength(first_closest_point - second_closest_point);
	}

	// --------------------------------------------------------------------------------------------------------------

	// This assumes that both a and b are in the 360 range
	ECSENGINE_API bool SameQuadrantRange360(float degrees_a, float degrees_b);

	ECSENGINE_API bool SameQuadrant(float degrees_a, float degrees_b);

	// --------------------------------------------------------------------------------------------------------------

}