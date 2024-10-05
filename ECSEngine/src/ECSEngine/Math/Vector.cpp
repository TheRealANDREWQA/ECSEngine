#include "ecspch.h"
#include "BaseVector.h"
#include "Vector.h"

namespace ECSEngine {

#define INFINITY_MASK_VALUE 0x7F800000
#define SIGN_MASK_VALUE 0x7FFFFFFF

#define EXPORT_TEMPLATE_PRECISION(return_value, function_name, ...) \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_PRECISE>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_ACCURATE>(__VA_ARGS__); \
	template ECSENGINE_API return_value function_name<ECS_VECTOR_FAST>(__VA_ARGS__);

#define EXPORT_TEMPLATE_PRECISION_VECTORCALL(return_value, function_name, ...) \
	template ECSENGINE_API return_value ECS_VECTORCALL function_name<ECS_VECTOR_PRECISE>(__VA_ARGS__); \
	template ECSENGINE_API return_value ECS_VECTORCALL function_name<ECS_VECTOR_ACCURATE>(__VA_ARGS__); \
	template ECSENGINE_API return_value ECS_VECTORCALL function_name<ECS_VECTOR_FAST>(__VA_ARGS__);
	

	namespace VectorGlobals {
		Vec8f ONE = Vec8f(1.0f);
		Vec8f EPSILON = Vec8f(ECS_SIMD_VECTOR_EPSILON_VALUE);
		Vec8i INFINITY_MASK = Vec8i(INFINITY_MASK_VALUE);
		Vec8i SIGN_MASK = Vec8i(SIGN_MASK_VALUE);
	}

	// We could use these templates to refer to the types in the Impl functions, but they are
	// A bit cumbersome. If at a later time this method is preferred, they can be simply uncommented
	/*template<typename VectorType>
	struct Mask {};

	template<>
	struct Mask<Vector3> {
		typedef SIMDVectorMask T;
	};

	template<>
	struct Mask<Vector4> {
		typedef SIMDVectorMask T;
	};

	template<>
	struct Mask<float3> {
		typedef bool T;
	};

	template<typename VectorType>
	struct SingleValue {};

	template<>
	struct SingleValue<Vector3> {
		typedef Vec8f T;
	};

	template<>
	struct SingleValue<float3> {
		typedef float T;
	};*/

	ECS_INLINE static float approx_recipr(float value) {
		__m128 temp_simd = _mm_set1_ps(value);
		__m128 result = _mm_rcp_ss(temp_simd);
		return _mm_cvtss_f32(result);
	}

	ECS_INLINE static float approx_rsqrt(float squared_value) {
		__m128 temp_simd = _mm_set1_ps(squared_value);
		__m128 result = _mm_rsqrt_ss(temp_simd);
		return _mm_cvtss_f32(result);
	}

	// To conform to the SIMD vector function
	struct SingleMask {
		ECS_INLINE bool AreAllSet(size_t count) const {
			return value;
		}

		bool value;
	};

	// Used to conform to the to_bits vector function
	ECS_INLINE static SingleMask to_bits(bool value) {
		return SingleMask{ value };
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL DegToRadImpl(Vector angles) {
		return angles * DEG_TO_RAD_FACTOR;
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL RadToDegImpl(Vector angles) {
		return angles * RAD_TO_DEG_FACTOR;
	}

	Vec8f ECS_VECTORCALL DegToRad(Vec8f angles)
	{
		return DegToRadImpl(angles);
	}

	Vec8f ECS_VECTORCALL RadToDeg(Vec8f angles) {
		return RadToDegImpl(angles);
	}

	Vector3 ECS_VECTORCALL DegToRad(Vector3 angles) {
		return DegToRadImpl(angles);
	}

	Vector3 ECS_VECTORCALL RadToDeg(Vector3 angles) {
		return RadToDegImpl(angles);
	}

	Vector4 ECS_VECTORCALL DegToRad(Vector4 angles) {
		return DegToRadImpl(angles);
	}

	Vector4 ECS_VECTORCALL RadToDeg(Vector4 angles) {
		return RadToDegImpl(angles);
	}

	// --------------------------------------------------------------------------------------------------------------

	bool IsInfiniteMask(float value)
	{
		int int_value = *(int*)&value;
		return (int_value & SIGN_MASK_VALUE) == INFINITY_MASK_VALUE;
	}

	SIMDVectorMask ECS_VECTORCALL IsInfiniteMask(Vec8f vector) {
		Vec8i integer_representation = CastToInteger(vector);
		integer_representation &= VectorGlobals::SIGN_MASK;
		return integer_representation == VectorGlobals::INFINITY_MASK;
	}

	// --------------------------------------------------------------------------------------------------------------

	Vector3 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector3 a, Vector3 b) {
		return Vector3(select(mask, a.x, b.x), select(mask, a.y, b.y), select(mask, a.z, b.z));
	}

	Vector3 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector3 a, Vector3 b, ECS_SIMD_VECTOR_COMPONENT component) {
		Vector3 result = a;
		result[component] = select(mask, a[component], b[component]);
		return result;
	}

	Vector4 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector4 a, Vector4 b) {
		return Vector4(select(mask, a.x, b.x), select(mask, a.y, b.y), select(mask, a.z, b.z), select(mask, a.w, b.w));
	}

	Vector4 ECS_VECTORCALL Select(SIMDVectorMask mask, Vector4 a, Vector4 b, ECS_SIMD_VECTOR_COMPONENT component) {
		Vector4 result = a;
		result[component] = select(mask, a[component], b[component]);
		return result;
	}
	
	// --------------------------------------------------------------------------------------------------------------

	Vector3 ECS_VECTORCALL Min(Vector3 a, Vector3 b) {
		return { min(a.x, b.x), min(a.y, b.y), min(a.z, b.z) };
	}

	Vector4 ECS_VECTORCALL Min(Vector4 a, Vector4 b) {
		return { min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w) };
	}

	Vector3 ECS_VECTORCALL Max(Vector3 a, Vector3 b) {
		return { max(a.x, b.x), max(a.y, b.y), max(a.z, b.z) };
	}

	Vector4 ECS_VECTORCALL Max(Vector4 a, Vector4 b) {
		return { max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w) };
	}

	// --------------------------------------------------------------------------------------------------------------

	bool IsNanMask(float value) {
		return isnan(value);
	}

	SIMDVectorMask ECS_VECTORCALL IsNanMask(Vec8f value) {
		return value != value;
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL AbsImpl(Vector vector) {
		Vector result;
		for (size_t index = 0; index < Vector::Count(); index++) {
			result[index] = abs(vector[index]);
		}
		return result;
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL AbsoluteDifferenceImpl(Vector a, Vector b) {
		Vector result;
		for (size_t index = 0; index < Vector::Count(); index++) {
			result[index] = abs(a[index] - b[index]);
		}
		return result;
	}

	Vector3 ECS_VECTORCALL Abs(Vector3 vector) {
		return AbsImpl(vector);
	}

	Vector4 ECS_VECTORCALL Abs(Vector4 vector) {
		return AbsImpl(vector);
	}

	Vec8f ECS_VECTORCALL AbsoluteDifference(Vec8f a, Vec8f b) {
		return abs(a - b);
	}
	
	Vector3 ECS_VECTORCALL AbsoluteDifference(Vector3 a, Vector3 b) {
		return AbsoluteDifferenceImpl(a, b);
	}

	Vector4 ECS_VECTORCALL AbsoluteDifference(Vector4 a, Vector4 b) {
		return AbsoluteDifferenceImpl(a, b);
	}

	// --------------------------------------------------------------------------------------------------------------

	float ECS_VECTORCALL Dot(float2 first, float2 second) {
		return first.x * second.x + first.y * second.y;
	}

	float ECS_VECTORCALL Dot(float3 first, float3 second) {
		return first.x * second.x + first.y * second.y + first.z * second.z;
	}

	float ECS_VECTORCALL Dot(float4 first, float4 second) {
		return first.x * second.x + first.y * second.y + first.z * second.z + first.w * second.w;
	}

	template<typename Vector>
	ECS_INLINE static Vec8f ECS_VECTORCALL DotImpl(Vector first, Vector second) {
		Vec8f result = first[0] * second[0];
		for (size_t index = 1; index < Vector::Count(); index++) {
			result += first[index] * second[index];
		}
		return result;
	}

	Vec8f ECS_VECTORCALL Dot(Vector3 first, Vector3 second) {
		return DotImpl(first, second);
	}

	Vec8f ECS_VECTORCALL Dot(Vector4 first, Vector4 second) {
		return DotImpl(first, second);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static SIMDVectorMask ECS_VECTORCALL CompareMaskImpl(Vector first, Vector second, Vec8f epsilon) {
		SIMDVectorMask result = CompareMask(first[0], second[0], epsilon);
		for (size_t index = 0; index < Vector::Count(); index++) {
			result &= CompareMask(first[index], second[index], epsilon);
		}
		return result;
	}

	bool ECS_VECTORCALL CompareMask(float3 first, float3 second, float3 epsilon) {
		return BasicTypeLess(AbsoluteDifference(first, second), epsilon);
	}

	bool ECS_VECTORCALL CompareMask(float4 first, float4 second, float4 epsilon)
	{
		return BasicTypeLess(AbsoluteDifference(first, second), epsilon);
	}

	SIMDVectorMask ECS_VECTORCALL CompareMask(Vec8f first, Vec8f second, Vec8f epsilon)
	{
		return AbsoluteDifference(first, second) < epsilon;
	}

	SIMDVectorMask ECS_VECTORCALL CompareMask(Vector3 first, Vector3 second, Vec8f epsilon)
	{
		return CompareMaskImpl(first, second, epsilon);
	}

	SIMDVectorMask ECS_VECTORCALL CompareMask(Vector4 first, Vector4 second, Vec8f epsilon)
	{
		return CompareMaskImpl(first, second, epsilon);
	}

	// --------------------------------------------------------------------------------------------------------------
	
	template<typename ReturnType, typename Vector, typename SingleValue>
	ECS_INLINE static ReturnType ECS_VECTORCALL CompareAngleNormalizedCosineMaskImpl(Vector first_normalized, Vector second_normalized, SingleValue cosine) {
		SingleValue direction_cosine = AbsSingle(Dot(first_normalized, second_normalized));
		return direction_cosine > cosine;
	}

	bool ECS_VECTORCALL CompareAngleRadMask(float3 first, float3 second, float radians)
	{
		return CompareAngleNormalizedRadMask(Normalize(first), Normalize(second), radians);
	}

	bool ECS_VECTORCALL CompareAngleNormalizedCosineMask(float3 first_normalized, float3 second_normalized, float cosine) {
		return CompareAngleNormalizedCosineMaskImpl<bool>(first_normalized, second_normalized, cosine);
	}

	SIMDVectorMask ECS_VECTORCALL CompareAngleRadMask(Vector3 first, Vector3 second, Vec8f radians) {
		return CompareAngleNormalizedRadMask(Normalize(first), Normalize(second), radians);
	}

	SIMDVectorMask ECS_VECTORCALL CompareAngleNormalizedCosineMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f cosine) {
		return CompareAngleNormalizedCosineMaskImpl<SIMDVectorMask>(first_normalized, second_normalized, cosine);
	}

	// --------------------------------------------------------------------------------------------------------------

	float3 ECS_VECTORCALL Lerp(float3 a, float3 b, float percentage)
	{
		return Fmadd(a, b - a, float3::Splat(percentage));
	}

	float4 ECS_VECTORCALL Lerp(float4 a, float4 b, float percentage)
	{
		return Fmadd(a, b - a, float4::Splat(percentage));
	}

	Vector3 ECS_VECTORCALL Lerp(Vector3 a, Vector3 b, Vec8f percentage)
	{
		return Fmadd(a, b - a, Vector3::Splat(percentage));
	}

	Vector4 ECS_VECTORCALL Lerp(Vector4 a, Vector4 b, Vec8f percentage)
	{
		return Fmadd(a, b - a, Vector4::Splat(percentage));
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL ProjectPointOnLineDirectionImpl(Vector line_point, Vector line_direction, Vector point) {
		// Formula A + (dot(AP,line_direction) / dot(line_direction, line_direction)) * line_direction
		// Where A is a point on the line and P is the point that we want to project
		// Add a pair of paranthesis such that the dot division is performed before the multiplication
		// With line_direction. The overloaded operator does take this into account, but it is
		// More efficient like this since we perform a single division with a splat of 3 multiplications,
		// While the other order of operations would be 3 multiplications, 1 division and another 3 multiplications
		return line_point + line_direction * ((Dot(point - line_point, line_direction) / Dot(line_direction, line_direction)));
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL ProjectPointOnLineDirectionNormalizedImpl(Vector line_point, Vector line_direction_normalized, Vector point) {
		// In this version we don't need to divide by the magnitude of the line direction
		// Formula A + line_direction * dot(AP,line_direction)
		// Where A is a point on the line and P is the point that we want to project
		return line_point + line_direction_normalized * Dot(point - line_point, line_direction_normalized);
	}

	float3 ECS_VECTORCALL ProjectPointOnLineDirection(float3 line_point, float3 line_direction, float3 point) {
		return ProjectPointOnLineDirectionImpl(line_point, line_direction, point);
	}

	Vector3 ECS_VECTORCALL ProjectPointOnLineDirection(Vector3 line_point, Vector3 line_direction, Vector3 point) {
		return ProjectPointOnLineDirectionImpl(line_point, line_direction, point);
	}

	float3 ECS_VECTORCALL ProjectPointOnLineDirectionNormalized(float3 line_point, float3 line_direction_normalized, float3 point) {
		return ProjectPointOnLineDirectionNormalizedImpl(line_point, line_direction_normalized, point);
	}

	Vector3 ECS_VECTORCALL ProjectPointOnLineDirectionNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 point) {
		return ProjectPointOnLineDirectionNormalizedImpl(line_point, line_direction_normalized, point);
	}

	float3 ECS_VECTORCALL ProjectPointOnLine(float3 line_a, float3 line_b, float3 point) {
		float3 line_direction = line_b - line_a;
		return ProjectPointOnLineDirection(line_a, line_direction, point);
	}

	Vector3 ECS_VECTORCALL ProjectPointOnLine(Vector3 line_a, Vector3 line_b, Vector3 point) {
		Vector3 line_direction = line_b - line_a;
		return ProjectPointOnLineDirection(line_a, line_direction, point);
	}

	// --------------------------------------------------------------------------------------------------------------

	float3 ECS_VECTORCALL ClampPointToSegment(float3 line_a, float3 line_b, float3 point) {
		float3 PA = point - line_a;
		float3 PB = point - line_b;
		float3 line = line_b - line_a;
		float dot_PA = Dot(PA, line);
		// If the dot product is negative, it belongs to that vertex voronoi region
		if (dot_PA < 0.0f) {
			return line_a;
		}

		float dot_PB = Dot(PB, line);
		// If the dot product is positive, since we are still using AB as direction, then it belongs
		// To the B vertex region
		if (dot_PA > 0.0f) {
			return line_b;
		}

		return ProjectPointOnLine(line_a, line_b, point);
	}

	//Vector3 ECS_VECTORCALL ClampPointToSegment(Vector3 line_a, Vector3 line_b, Vector3 point) {

	//}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL CrossImpl(Vector a, Vector b) {
		// a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x
		return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	}

	float3 ECS_VECTORCALL Cross(float3 a, float3 b)
	{
		return CrossImpl(a, b);
	}

	Vector3 ECS_VECTORCALL Cross(Vector3 a, Vector3 b) {
		return CrossImpl(a, b);
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL CrossTripleProductImpl(Vector line_a, Vector line_b, Vector point) {
		// This is simply 2 cross products - the line direction with a direction vector from the point
		// To one of the line points and then cross product with the line direction again
		Vector line_direction = line_b - line_a;
		return Cross(Cross(line_direction, point - line_a), line_direction);
	}

	float3 ECS_VECTORCALL CrossTripleProduct(float3 line_a, float3 line_b, float3 point) {
		return CrossTripleProductImpl(line_a, line_b, point);
	}

	Vector3 ECS_VECTORCALL CrossTripleProduct(Vector3 line_a, Vector3 line_b, Vector3 point) {
		return CrossTripleProductImpl(line_a, line_b, point);
	}

	float3 ECS_VECTORCALL CrossTripleProduct(float3 line_direction, float3 line_point_direction) {
		return Cross(Cross(line_direction, line_point_direction), line_direction);
	}

	Vector3 ECS_VECTORCALL CrossTripleProduct(Vector3 line_direction, Vector3 line_point_direction) {
		return Cross(Cross(line_direction, line_point_direction), line_direction);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL LengthImpl(Vector vector) {
		return sqrt(SquareLength(vector));
	}

	Vec8f ECS_VECTORCALL Length(Vector3 vector) {
		return LengthImpl<Vec8f>(vector);
	}

	Vec8f ECS_VECTORCALL Length(Vector4 vector) {
		return LengthImpl<Vec8f>(vector);
	}

	float ECS_VECTORCALL Length(float2 vector) {
		return LengthImpl<float>(vector);
	}

	float ECS_VECTORCALL Length(float3 vector) {
		return LengthImpl<float>(vector);
	}

	float ECS_VECTORCALL Length(float4 vector) {
		return LengthImpl<float>(vector);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename ReturnValue, ECS_VECTOR_PRECISION precision, typename Vector>
	ECS_INLINE static ReturnValue ECS_VECTORCALL ReciprocalLengthImpl(Vector vector) {
		auto square_length = SquareLength(vector);
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			return OneVector<Vector>() / sqrt(square_length);
		}
		else if constexpr (precision == ECS_VECTOR_ACCURATE) {
			return approx_recipr(sqrt(square_length));
		}
		else if constexpr (precision == ECS_VECTOR_FAST) {
			return approx_rsqrt(square_length);
		}
		else {
			static_assert(false, "Invalid Vector precision");
		}
	}

	template<ECS_VECTOR_PRECISION precision>
	float ECS_VECTORCALL ReciprocalLength(float3 vector) {
		return ReciprocalLengthImpl<float, precision>(vector);
	}

	EXPORT_TEMPLATE_PRECISION(float, ReciprocalLength, float3);

	template<ECS_VECTOR_PRECISION precision>
	float ECS_VECTORCALL ReciprocalLength(float4 vector) {
		return ReciprocalLengthImpl<float, precision>(vector);
	}

	EXPORT_TEMPLATE_PRECISION(float, ReciprocalLength, float4);

	template<ECS_VECTOR_PRECISION precision>
	Vec8f ECS_VECTORCALL ReciprocalLength(Vector3 vector) {
		return ReciprocalLengthImpl<Vec8f, precision>(vector);
	}

	EXPORT_TEMPLATE_PRECISION(Vec8f, ReciprocalLength, Vector3);

	template<ECS_VECTOR_PRECISION precision>
	Vec8f ECS_VECTORCALL ReciprocalLength(Vector4 vector)
	{
		return ReciprocalLengthImpl<Vec8f, precision>(vector);
	}

	EXPORT_TEMPLATE_PRECISION(Vec8f, ReciprocalLength, Vector4);

	// --------------------------------------------------------------------------------------------------------------

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL SquaredDistanceToLineImpl(Vector line_point, Vector line_direction, Vector point) {
		Vector projected_point = ProjectPointOnLineDirection(line_point, line_direction, point);
		return SquareLength(projected_point - point);
	}

	float ECS_VECTORCALL SquaredDistanceToLine(float3 line_point, float3 line_direction, float3 point) {
		return SquaredDistanceToLineImpl<float>(line_point, line_direction, point);
	}

	Vec8f ECS_VECTORCALL SquaredDistanceToLine(Vector3 line_point, Vector3 line_direction, Vector3 point) {
		return SquaredDistanceToLineImpl<Vec8f>(line_point, line_direction, point);
	}

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL SquaredDistanceToLineNormalizedImpl(Vector line_point, Vector line_direction_normalized, Vector point) {
		Vector projected_point = ProjectPointOnLineDirectionNormalized(line_point, line_direction_normalized, point);
		return SquareLength(projected_point - point);
	}

	float ECS_VECTORCALL SquaredDistanceToLineNormalized(float3 line_point, float3 line_direction_normalized, float3 point) {
		return SquaredDistanceToLineNormalizedImpl<float>(line_point, line_direction_normalized, point);
	}

	Vec8f ECS_VECTORCALL SquaredDistanceToLineNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 point) {
		return SquaredDistanceToLineNormalizedImpl<Vec8f>(line_point, line_direction_normalized, point);
	}

	Vec8f ECS_VECTORCALL DistanceToLine(Vector3 line_point, Vector3 line_direction, Vector3 point) {
		return sqrt(SquaredDistanceToLine(line_point, line_direction, point));
	}

	Vec8f ECS_VECTORCALL DistanceToLineNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 point) {
		return sqrt(SquaredDistanceToLineNormalized(line_point, line_direction_normalized, point));
	}

	// --------------------------------------------------------------------------------------------------------------

	template<ECS_VECTOR_PRECISION precision, typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL NormalizeImpl(Vector vector) {
		//if constexpr (precision == ECS_VECTOR_PRECISE) {
		//	// Don't use reciprocal length so as to not add an extra read to the
		//	// VectorGlobals::ONE and then a multiplication
		//	auto divisor = Length(vector);
		//	return vector / divisor;
		//}
		//else {
		//	auto divisor = ReciprocalLength<precision>(vector);
		//	return vector * divisor;
		//}

		auto divisor = ReciprocalLength<precision>(vector);
		return vector * divisor;
	}

	template<ECS_VECTOR_PRECISION precision>
	float3 ECS_VECTORCALL Normalize(float3 vector) {
		return NormalizeImpl<precision>(vector);
	}

	EXPORT_TEMPLATE_PRECISION_VECTORCALL(float3, Normalize, float3);

	template<ECS_VECTOR_PRECISION precision>
	float4 ECS_VECTORCALL Normalize(float4 vector) {
		return NormalizeImpl<precision>(vector);
	}

	EXPORT_TEMPLATE_PRECISION_VECTORCALL(float4, Normalize, float4);

	template<ECS_VECTOR_PRECISION precision>
	Vector3 ECS_VECTORCALL Normalize(Vector3 vector) {
		return NormalizeImpl<precision>(vector);
	}

	EXPORT_TEMPLATE_PRECISION_VECTORCALL(Vector3, Normalize, Vector3);

	template<ECS_VECTOR_PRECISION precision>
	Vector4 ECS_VECTORCALL Normalize(Vector4 vector) {
		return NormalizeImpl<precision>(vector);
	}
	
	EXPORT_TEMPLATE_PRECISION_VECTORCALL(Vector4, Normalize, Vector4);

	float2 ECS_VECTORCALL Normalize(float2 vector) {
		return vector * float2::Splat(1.0f / Length(vector));
	}

	SIMDVectorMask ECS_VECTORCALL IsNormalizedSquareLengthMask(Vec8f squared_length) {
		Vec8f one = VectorGlobals::ONE;
		return CompareMaskSingle(squared_length, one);
	}

	SIMDVectorMask ECS_VECTORCALL IsNormalizedMask(Vec8f length) {
		Vec8f one = VectorGlobals::ONE;
		return CompareMaskSingle(length, one);
	}

	template<ECS_VECTOR_PRECISION precision, typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL NormalizeIfNotImpl(Vector vector, size_t vector_count) {
		if constexpr (precision == ECS_VECTOR_PRECISE || precision == ECS_VECTOR_ACCURATE) {
			auto square_length = SquareLength(vector);
			auto mask = IsNormalizedSquareLengthMask(square_length);
			bool bool_mask = SIMDMaskAreAllSet(mask, vector_count);
			if (!bool_mask) {
				if constexpr (precision == ECS_VECTOR_PRECISE) {
					return vector / Vector::Splat(sqrt(square_length));
				}
				else {
					return vector * Vector::Splat(approx_recipr(square_length));
				}
			}
			else {
				return vector;
			}
		}
		else {
			return Normalize<ECS_VECTOR_FAST>(vector);
		}
	}

	template<ECS_VECTOR_PRECISION precision>
	float3 ECS_VECTORCALL NormalizeIfNot(float3 vector) {
		// The count is not used
		return NormalizeIfNotImpl<precision>(vector, 1);
	}

	EXPORT_TEMPLATE_PRECISION(float3, NormalizeIfNot, float3);

	template<ECS_VECTOR_PRECISION precision>
	float4 ECS_VECTORCALL NormalizeIfNot(float4 vector) {
		// The count is not used
		return NormalizeIfNotImpl<precision>(vector, 1);
	}

	EXPORT_TEMPLATE_PRECISION(float4, NormalizeIfNot, float4);

	template<ECS_VECTOR_PRECISION precision>
	Vector3 ECS_VECTORCALL NormalizeIfNot(Vector3 vector, size_t vector_count) {
		return NormalizeIfNotImpl<precision>(vector, vector_count);
	}

	EXPORT_TEMPLATE_PRECISION(Vector3, NormalizeIfNot, Vector3, size_t);

	template<ECS_VECTOR_PRECISION precision>
	Vector4 ECS_VECTORCALL NormalizeIfNot(Vector4 vector, size_t vector_count) {
		return NormalizeIfNotImpl<precision>(vector, vector_count);
	}

	EXPORT_TEMPLATE_PRECISION(Vector4, NormalizeIfNot, Vector4, size_t);

	template<ECS_VECTOR_PRECISION precision>
	float OneDividedVector(float value) {
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			return 1.0f / value;
		}
		else {
			return value * approx_recipr(value);
		}
	}

	EXPORT_TEMPLATE_PRECISION(float, OneDividedVector, float);

	template<ECS_VECTOR_PRECISION precision>
	Vec8f ECS_VECTORCALL OneDividedVector(Vec8f value) {
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			return VectorGlobals::ONE / value;
		}
		else {
			return value * approx_recipr(value);
		}
	}

	EXPORT_TEMPLATE_PRECISION(Vec8f, OneDividedVector, Vec8f);

	template<ECS_VECTOR_PRECISION precision, typename Vector>
	Vector ECS_VECTORCALL OneDividedVectorImpl(Vector value) {
		if constexpr (precision == ECS_VECTOR_PRECISE) {
			if constexpr (std::is_same_v<Vector, Vector3>) {
				return Vector::Splat(VectorGlobals::ONE) / value;
			}
			else {
				return float3::Splat(1.0f) / value;
			}
		}
		else {
			Vector return_value;
			for (size_t index = 0; index < Vector::Count(); index++) {
				return_value[index] = value[index] * approx_recipr(value[index]);
			}
			return return_value;
		}
	}

	template<ECS_VECTOR_PRECISION precision>
	float3 ECS_VECTORCALL OneDividedVector(float3 value) {
		return OneDividedVectorImpl<precision>(value);
	}

	EXPORT_TEMPLATE_PRECISION(float3, OneDividedVector, float3);

	template<ECS_VECTOR_PRECISION precision>
	Vector3 ECS_VECTORCALL OneDividedVector(Vector3 value) {
		return OneDividedVectorImpl<precision>(value);
	}

	EXPORT_TEMPLATE_PRECISION(Vector3, OneDividedVector, Vector3);

	// --------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL IsParallelMask(float3 first_normalized, float3 second_normalized, float epsilon) {
		return CompareMask(first_normalized, second_normalized, float3::Splat(epsilon)) 
			|| CompareMask(first_normalized, -second_normalized, float3::Splat(epsilon));
	}

	SIMDVectorMask ECS_VECTORCALL IsParallelMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f epsilon) {
		return CompareMask(first_normalized, second_normalized, epsilon)
			|| CompareMask(first_normalized, -second_normalized, epsilon);
	}

	// --------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL IsParallelAngleCosineMask(float3 first_normalized, float3 second_normalized, float cosine) {
		return CompareAngleNormalizedCosineMask(first_normalized, second_normalized, cosine);
	}

	SIMDVectorMask ECS_VECTORCALL IsParallelAngleCosineMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f cosine) {
		return CompareAngleNormalizedCosineMask(first_normalized, second_normalized, cosine);
	}

	// --------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL IsPerpendicularMask(float3 first_normalized, float3 second_normalized, float epsilon) {
		return AbsoluteDifferenceSingle(Dot(first_normalized, second_normalized), 0.0f) < epsilon;
	}

	SIMDVectorMask ECS_VECTORCALL IsPerpendicularMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f epsilon) {
		return CompareMask(Dot(first_normalized, second_normalized), ZeroVectorFloat(), epsilon);
	}

	// --------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL IsPerpendicularAngleCosineMask(float3 first_normalized, float3 second_normalized, float radians) {
		return !CompareAngleNormalizedCosineMask(first_normalized, second_normalized, cos((PI / 2) - radians));
	}

	SIMDVectorMask ECS_VECTORCALL IsPerpendicularAngleCosineMask(Vector3 first_normalized, Vector3 second_normalized, Vec8f radians) {
		return !CompareAngleNormalizedCosineMask(first_normalized, second_normalized, cos(Vec8f(PI / 2) - radians));
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL ClampImpl(Vector value, Vector min, Vector max) {
		Vector return_value;
		for (size_t index = 0; index < Vector::Count(); index++) {
			auto is_smaller = value[index] < min[index];
			auto is_greater = value[index] > max[index];
			return_value[index] = SelectSingle(is_greater, max[index], SelectSingle(is_smaller, min[index], value[index]));
		}
		return return_value;
	}

	Vector3 ECS_VECTORCALL Clamp(Vector3 value, Vector3 min, Vector3 max) {
		return ClampImpl(value, min, max);
	}

	Vector4 ECS_VECTORCALL Clamp(Vector4 value, Vector4 min, Vector4 max) {
		return ClampImpl(value, min, max);
	}

	template<typename ReturnType, bool equal_min, bool equal_max, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL IsInRangeMaskImpl(Vector value, Vector min, Vector max) {
		ReturnType all_is_lower, all_is_greater;

		auto compare = [&](ReturnType& is_lower, ReturnType& is_greater, size_t index) {
			if constexpr (equal_min) {
				is_lower = value[index] >= min[index];
			}
			else {
				is_lower = value[index] > min[index];
			}

			if constexpr (equal_max) {
				is_greater = value[index] <= max[index];
			}
			else {
				is_greater = value[index] < max[index];
			}
		};

		compare(all_is_lower, all_is_greater, 0);
		for (size_t index = 1; index < Vector::Count(); index++) {
			ReturnType current_is_lower, current_is_greater;
			compare(current_is_lower, current_is_greater, index);
			all_is_lower &= current_is_lower;
			all_is_greater &= current_is_greater;
		}
		return all_is_lower && all_is_greater;
	}

	template<bool equal_min, bool equal_max>
	bool ECS_VECTORCALL IsInRangeMask(float3 value, float3 min, float3 max) {
		return IsInRangeMaskImpl<bool, equal_min, equal_max>(value, min, max);
	}

	ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL(bool, IsInRangeMask, float3, float3, float3);

	template<bool equal_min, bool equal_max>
	bool ECS_VECTORCALL IsInRangeMask(float4 value, float4 min, float4 max) {
		return IsInRangeMaskImpl<bool, equal_min, equal_max>(value, min, max);
	}

	ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL(bool, IsInRangeMask, float4, float4, float4);

	template<bool equal_min, bool equal_max>
	SIMDVectorMask ECS_VECTORCALL IsInRangeMask(Vector3 value, Vector3 min, Vector3 max) {
		return IsInRangeMaskImpl<SIMDVectorMask, equal_min, equal_max>(value, min, max);
	}

	ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL(SIMDVectorMask, IsInRangeMask, Vector3, Vector3, Vector3);

	template<bool equal_min, bool equal_max>
	SIMDVectorMask ECS_VECTORCALL IsInRangeMask(Vector4 value, Vector4 min, Vector4 max) {
		return IsInRangeMaskImpl<SIMDVectorMask, equal_min, equal_max>(value, min, max);
	}

	ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL(SIMDVectorMask, IsInRangeMask, Vector4, Vector4, Vector4);

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL ReflectImpl(Vector incident, Vector normal) {
		// result = incident - (2 * Dot(incident, normal)) * normal
		auto dot = Dot(incident, normal);
		return incident - Vector::Splat(dot + dot) * normal;
	}

	float3 ECS_VECTORCALL Reflect(float3 incident, float3 normal) {
		return ReflectImpl(incident, normal);
	}

	Vector3 ECS_VECTORCALL Reflect(Vector3 incident, Vector3 normal) {
		return ReflectImpl(incident, normal);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename MaskType, typename Vector, typename RefractionType>
	ECS_INLINE static Vector ECS_VECTORCALL RefractImpl(Vector incident, Vector normal, RefractionType refraction_index, size_t vector_count) {
		// result = refraction_index * incident - normal * (refraction_index * Dot(incident, normal) +
		// sqrt(1 - refraction_index * refraction_index * (1 - Dot(incident, normal) * Dot(incident, normal))))

		RefractionType dot = Dot(incident, normal);
		RefractionType one;
		RefractionType zero;
		if constexpr (std::is_same_v<Vector, float3>) {
			one = 1.0f;
			zero = 0.0f;
		}
		else {
			one = VectorGlobals::ONE;
			zero = ZeroVectorFloat();
		}
		RefractionType sqrt_dot_factor = one - dot * dot;
		RefractionType refraction_index_squared = refraction_index * refraction_index;

		RefractionType sqrt_value = one - refraction_index_squared * sqrt_dot_factor;
		auto is_less_or_zero = sqrt_value <= zero;

		RefractionType result = sqrt_value;

		MaskType mask = { is_less_or_zero };

		// Total reflection
		if (mask.AreAllSet(vector_count)) {
			return Vector::Splat(zero);
		}

		return Fmsub(Vector::Splat(refraction_index), incident, normal * Fmadd(refraction_index, dot, sqrt(result)));
	}

	float3 ECS_VECTORCALL Refract(float3 incident, float3 normal, float refraction_index) {
		return RefractImpl<SingleMask>(incident, normal, refraction_index, 1);
	}

	Vector3 ECS_VECTORCALL Refract(Vector3 incident, Vector3 normal, Vec8f refraction_index, size_t vector_count) {
		return RefractImpl<VectorMask>(incident, normal, refraction_index, vector_count);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL AngleBetweenVectorsNormalizedRadImpl(Vector a_normalized, Vector b_normalized) {
		// Just take the cosine (dot) between them and return the acos result
		ReturnType dot = Dot(a_normalized, b_normalized);
		// We need to clamp the result in the [-1, 1] range. Because of floating point inaccuracies,
		// It may go slightly above 1 or slightly below -1
		dot = ClampSingle(dot, SingleValueVector<Vector>(-1.0f), SingleValueVector<Vector>(1.0f));
		return acos(dot);
	}

	float ECS_VECTORCALL AngleBetweenVectorsNormalizedRad(float3 a_normalized, float3 b_normalized) {
		return AngleBetweenVectorsNormalizedRadImpl<float>(a_normalized, b_normalized);
	}

	Vec8f ECS_VECTORCALL AngleBetweenVectorsNormalizedRad(Vector3 a_normalized, Vector3 b_normalized) {
		return AngleBetweenVectorsNormalizedRadImpl<Vec8f>(a_normalized, b_normalized);
	}

	template<typename ReturnType, ECS_VECTOR_PRECISION precision, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL AngleBetweenVectorsRadImpl(Vector a, Vector b) {
		return AngleBetweenVectorsNormalizedRad(Normalize<precision>(a), Normalize<precision>(b));
	}

	template<ECS_VECTOR_PRECISION precision>
	float ECS_VECTORCALL AngleBetweenVectorsRad(float3 a, float3 b) {
		return AngleBetweenVectorsRadImpl<float, precision>(a, b);
	}

	EXPORT_TEMPLATE_PRECISION(float, AngleBetweenVectorsRad, float3, float3);

	template<ECS_VECTOR_PRECISION precision>
	Vec8f ECS_VECTORCALL AngleBetweenVectorsRad(Vector3 a, Vector3 b) {
		return AngleBetweenVectorsRadImpl<Vec8f, precision>(a, b);
	}

	EXPORT_TEMPLATE_PRECISION(Vec8f, AngleBetweenVectorsRad, Vector3, Vector3);

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL FmaddImpl(Vector a, Vector b, Vector c) {
		Vector result;
		for (size_t index = 0; index < Vector::Count(); index++) {
			result[index] = Fmadd(a[index], b[index], c[index]);
		}
		return result;
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL FmsubImpl(Vector a, Vector b, Vector c) {
		Vector result;
		for (size_t index = 0; index < Vector::Count(); index++) {
			result[index] = Fmsub(a[index], b[index], c[index]);
		}
		return result;
	}

	float3 ECS_VECTORCALL Fmadd(float3 a, float3 b, float3 c) {
		return FmaddImpl(a, b, c);
	}

	float4 ECS_VECTORCALL Fmadd(float4 a, float4 b, float4 c) {
		return FmaddImpl(a, b, c);
	}

	Vector3 ECS_VECTORCALL Fmadd(Vector3 a, Vector3 b, Vector3 c) {
		return FmaddImpl(a, b, c);
	}

	Vector4 ECS_VECTORCALL Fmadd(Vector4 a, Vector4 b, Vector4 c) {
		return FmaddImpl(a, b, c);
	}

	float3 ECS_VECTORCALL Fmsub(float3 a, float3 b, float3 c) {
		return FmsubImpl(a, b, c);
	}

	float4 ECS_VECTORCALL Fmsub(float4 a, float4 b, float4 c) {
		return FmsubImpl(a, b, c);
	}

	Vector3 ECS_VECTORCALL Fmsub(Vector3 a, Vector3 b, Vector3 c) {
		return FmsubImpl(a, b, c);
	}

	Vector4 ECS_VECTORCALL Fmsub(Vector4 a, Vector4 b, Vector4 c) {
		return FmsubImpl(a, b, c);
	}

	Vec8f ECS_VECTORCALL Fmod(Vec8f a, Vec8f b) {
		// Formula a - truncate(a / b) * b;

		// Fmadd version might have better precision but slightly slower because of the _mm_set1_ps
		//return Fmadd(truncate(a / b), -b, a);

		// Cannot use approximate reciprocal of b and multiply instead of division
		// since the error is big enough that it will break certain use cases
		return a - truncate(a / b) * b;
	}

	float3 ECS_VECTORCALL Fmod(float3 a, float3 b) {
		float3 result;
		for (size_t index = 0; index < result.Count(); index++) {
			result[index] = Fmod(a[index], b[index]);
		}
		return result;
	}

	Vector3 ECS_VECTORCALL Fmod(Vector3 a, Vector3 b) {
		Vector3 result;
		for (size_t index = 0; index < result.Count(); index++) {
			result[index] = Fmod(a[index], b[index]);
		}
		return result;
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename SingleValue, typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL DirectionToRotationEulerRadImpl(Vector direction) {
		/* Scalar algorithm
		float x_angle = fmod(atan2(direction.y, direction.z), PI);
		float y_angle = fmod(atan2(direction.z, direction.x), PI);
		float z_angle = fmod(atan2(direction.y, direction.x), PI);*/

		const float EPSILON = 0.001f;
		SingleValue pi;
		SingleValue zero;
		SingleValue epsilon;
		if constexpr (std::is_same_v<Vector, float3>) {
			pi = PI;
			zero = 0.0f;
			epsilon = EPSILON;
		}
		else {
			pi = Vec8f(PI);
			zero = ZeroVectorFloat();
			epsilon = Vec8f(EPSILON);
		}
		// It can happen that atan2 is unstable when values are really small and returns 
		// something close to PI when it should be 0
		// Handle this case - we need a slightly larger epsilon
		auto x_radians_base = atan2(direction.y, direction.z);
		auto y_radians_base = atan2(direction.z, direction.x);
		auto z_radians_base = atan2(direction.y, direction.x);

		auto x_angle = Fmod(x_radians_base, pi);
		auto y_angle = Fmod(y_radians_base, pi);
		auto z_angle = Fmod(z_radians_base, pi);

		Vector result = Vector(x_angle, y_angle, z_angle);
		for (size_t index = 0; index < Vector::Count(); index++) {
			result[index] = SelectSingle(CompareMaskSingle(result[index], pi, epsilon), zero, result[index]);
		}
		return result;
	}

	float3 ECS_VECTORCALL DirectionToRotationEulerRad(float3 direction) {
		return DirectionToRotationEulerRadImpl<float>(direction);
	}

	Vector3 ECS_VECTORCALL DirectionToRotationEulerRad(Vector3 direction) {
		return DirectionToRotationEulerRadImpl<Vec8f>(direction);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	void ECS_VECTORCALL OrthonormalBasisImpl(Vector direction_normalized, Vector* direction_first, Vector* direction_second) {
		/*
		*   Scalar algorithm - Credit to Building an Orthonormal Basis, Revisited - 2017
			Here n maps to direction normalized
			if(n.z<0.0f){
				const float a = 1.0f / (1.0f - n.z);
				const float b = n.x * n.y * a;
				b1 = Vec3f(1.0f - n.x * n.x * a, -b, n.x);
				b2 = Vec3f(b, n.y * n.y * a - 1.0f, -n.y);
			}
			else{
				const float a = 1.0f / (1.0f + n.z);
				const float b = -n.x * n.y * a;
				b1 = Vec3f(1.0f - n.x * n.x * a, b, -n.x);
				b2 = Vec3f(b, 1.0f - n.y * n.y * a, -n.y);
			}
		*/

		if constexpr (std::is_same_v<Vector, float3>) {
			if (direction_normalized.z < 0.0f) {
				float a = 1.0f / (1.0f - direction_normalized.z);
				float b = direction_normalized.x * direction_normalized.y * a;
				*direction_first = float3(1.0f - direction_normalized.x * direction_normalized.x * a, -b, direction_normalized.x);
				*direction_second = float3(b, direction_normalized.y * direction_normalized.y * a - 1.0f, -direction_normalized.y);
			}
			else {
				float a = 1.0f / (1.0f + direction_normalized.z);
				float b = -direction_normalized.x * direction_normalized.y * a;
				*direction_first = float3(1.0f - direction_normalized.x * direction_normalized.x * a, b, -direction_normalized.x);
				*direction_second = float3(b, 1.0f - direction_normalized.y * direction_normalized.y * a, -direction_normalized.y);
			}
		}
		else {
			SIMDVectorMask less_than_zero = direction_normalized.z < ZeroVectorFloat();
			// Use this mask to generate an inversion mask that can be used to invert some values
			Vec8f invert_mask = ChangeSignMask(less_than_zero);
			// We'll basically use the else branch and invert with the mask in case it doesn't fit in that branch
			Vec8f a = 1.0f / (1.0f + ChangeSign(direction_normalized.z, invert_mask));
			Vec8f b = direction_normalized.x * direction_normalized.y * ChangeSign(a, invert_mask);
			*direction_first = Vector3(1.0f - direction_normalized.x * direction_normalized.x * a, ChangeSign(b, invert_mask), ChangeSign(-direction_normalized.x, invert_mask));
			*direction_second = Vector3(b, ChangeSign(VectorGlobals::ONE - direction_normalized.y * direction_normalized.y * a, invert_mask), -direction_normalized.y);
		}
	}

	void ECS_VECTORCALL OrthonormalBasis(float3 direction_normalized, float3* direction_first, float3* direction_second) {
		OrthonormalBasisImpl(direction_normalized, direction_first, direction_second);
	}

	void ECS_VECTORCALL OrthonormalBasis(Vector3 direction_normalized, Vector3* direction_first, Vector3* direction_second) {
		OrthonormalBasisImpl(direction_normalized, direction_first, direction_second);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL CullClipSpaceXMaskImpl(Vector vector) {
		return (-vector.w) <= vector.x && vector.x <= vector.w;
	}

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL CullClipSpaceYMaskImpl(Vector vector) {
		return (-vector.w) <= vector.y && vector.y <= vector.w;
	}

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL CullClipSpaceZMaskImpl(Vector vector) {
		if constexpr (std::is_same_v<Vector, float4>) {
			return 0.0f <= vector.z && vector.z <= vector.w;
		}
		else {
			return ZeroVectorFloat() <= vector.z && vector.z <= vector.w;
		}
	}

	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL CullClipSpaceWMaskImpl(Vector vector) {
		if constexpr (std::is_same_v<Vector, float4>) {
			return vector.w > 0.0f;
		}
		else {
			return vector.w > ZeroVectorFloat();
		}
	}

	bool CullClipSpaceXMask(float4 vector) {
		return CullClipSpaceXMaskImpl<bool>(vector);
	}

	SIMDVectorMask ECS_VECTORCALL CullClipSpaceXMask(Vector4 vector) {
		return CullClipSpaceXMaskImpl<SIMDVectorMask>(vector);
	}

	bool CullClipSpaceYMask(float4 vector) {
		return CullClipSpaceYMaskImpl<bool>(vector);
	}

	SIMDVectorMask ECS_VECTORCALL CullClipSpaceYMask(Vector4 vector) {
		return CullClipSpaceYMaskImpl<SIMDVectorMask>(vector);
	}

	bool CullClipSpaceZMask(float4 vector) {
		return CullClipSpaceZMaskImpl<bool>(vector);
	}

	SIMDVectorMask ECS_VECTORCALL CullClipSpaceZMask(Vector4 vector) {
		return CullClipSpaceZMaskImpl<SIMDVectorMask>(vector);
	}

	bool CullClipSpaceWMask(float4 vector) {
		return CullClipSpaceWMaskImpl<bool>(vector);
	}

	SIMDVectorMask ECS_VECTORCALL CullClipSpaceWMask(Vector4 vector) {
		return CullClipSpaceWMaskImpl<SIMDVectorMask>(vector);
	}

	// Returns a mask where vertices which should be kept have their lane set to true
	template<typename ReturnType, typename Vector>
	ECS_INLINE static ReturnType ECS_VECTORCALL CullClipSpaceMaskImpl(Vector vector) {
		// The entire conditions are like this
		/*
			w > 0
			-w <= x <= w
			-w <= y <= w
			0 <= z <= w
		*/

		ReturnType x_mask = CullClipSpaceXMask(vector);
		ReturnType y_mask = CullClipSpaceYMask(vector);
		ReturnType z_mask = CullClipSpaceZMask(vector);
		ReturnType w_mask = CullClipSpaceWMask(vector);
		return x_mask && y_mask && z_mask && w_mask;
	}

	bool CullClipSpaceMask(float4 vector) {
		return CullClipSpaceMaskImpl<bool>(vector);
	}

	SIMDVectorMask ECS_VECTORCALL CullClipSpaceMask(Vector4 vector) {
		return CullClipSpaceMaskImpl<SIMDVectorMask>(vector);
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL ClipSpaceToNDCImpl(Vector vector) {
		// We just need to divide xyz / w, but here we also divide w by w
		// so as to not add another masking instruction
		return vector / Vector::Splat(vector.w);
	}

	float4 ECS_VECTORCALL ClipSpaceToNDC(float4 vector) {
		return ClipSpaceToNDCImpl(vector);
	}

	Vector4 ECS_VECTORCALL ClipSpaceToNDC(Vector4 vector) {
		return ClipSpaceToNDCImpl(vector);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL GetVectorForDirectionImpl(Vector direction_normalized, Vector world_compute_product, Vector world_same_direction) {
		// Compute the cross product between the a world axis and the given direction
		// If they are parallel, then the return vector is the world same direction if the direction
		// is positive, else the negative world same direction vector if the direction is negative
		auto is_parallel_to_up = IsParallelMask(direction_normalized, world_compute_product);
		
		// In case it is parallel to the world compute product, we can return the corresponding world direction
		// With the appropriate Sign. We can use Dot to get the corresponding sign
		Vector parallel_result = Vector::Splat(Dot(direction_normalized, world_compute_product)) * world_same_direction;

		// Compute the cross product
		Vector cross_product = Cross(direction_normalized, world_compute_product);
		cross_product = Normalize(cross_product);
		return Select(is_parallel_to_up, parallel_result, cross_product);
	}

	float3 ECS_VECTORCALL GetRightVectorForDirection(float3 direction_normalized) {
		return GetVectorForDirectionImpl(direction_normalized, GetUpVector(), GetRightVector());
	}
	
	Vector3 ECS_VECTORCALL GetRightVectorForDirection(Vector3 direction_normalized) {
		return GetVectorForDirectionImpl(direction_normalized, UpVector(), RightVector());
	}

	float3 ECS_VECTORCALL GetUpVectorForDirection(float3 direction_normalized) {
		return GetVectorForDirectionImpl(direction_normalized, GetRightVector(), GetUpVector());
	}

	Vector3 ECS_VECTORCALL GetUpVectorForDirection(Vector3 direction_normalized) {
		return GetVectorForDirectionImpl(direction_normalized, RightVector(), UpVector());
	}

	// --------------------------------------------------------------------------------------------------------------

	float3 ECS_VECTORCALL ClosestPoint(Stream<float3> points, float3 origin) {
		Vector3 closest_points = Vector3().Splat(float3::Splat(FLT_MAX));
		Vec8f smallest_distance = Vec8f(FLT_MAX);
		Vector3 vector_origin = Vector3().Splat(origin);

		float smallest_dist = FLT_MAX;
		size_t smallest_simd_index = 0;
		float3 smallest_dist_point;
		size_t simd_count = points.size - (points.size % Vec8f().size());
		if (simd_count > 0) {
			for (size_t index = 0; index < simd_count; index++) {
				Vector3 simd_points = Vector3().Gather(points.buffer + index);
				Vec8f distance = SquareLength(simd_points - vector_origin);
				SIMDVectorMask is_closer = smallest_distance > distance;
				smallest_distance = select(is_closer, distance, smallest_distance);
				closest_points = Select(is_closer, simd_points, closest_points);
			}

			alignas(ECS_SIMD_BYTE_SIZE) float scalar_smallest_distance[Vec8f::size()];
			smallest_distance.store_a(scalar_smallest_distance);
			for (size_t index = 0; index < Vec8f::size(); index++) {
				if (smallest_dist > scalar_smallest_distance[index]) {
					smallest_dist = scalar_smallest_distance[index];
					smallest_simd_index = index;
				}
			}
		}

		for (size_t index = simd_count; index < points.size; index++) {
			float distance = SquareLength(points[index] - origin);
			if (distance < smallest_dist) {
				smallest_dist = distance;
				smallest_simd_index = -1;
				smallest_dist_point = points[index];
			}
		}

		return smallest_simd_index == -1 ? smallest_dist_point : closest_points.At(smallest_simd_index);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Mask, typename Vector>
	ECS_INLINE Mask ECS_VECTORCALL PointSameLineHalfPlaneImpl(Vector line_a, Vector line_b, Vector reference_point, Vector test_point) {
		return PointSameLineHalfPlaneNormalized(line_a, Normalize(line_b - line_a), reference_point, test_point);
	}

	bool ECS_VECTORCALL PointSameLineHalfPlane(float3 line_a, float3 line_b, float3 reference_point, float3 test_point) {
		return PointSameLineHalfPlaneImpl<bool>(line_a, line_b, reference_point, test_point);
	}

	SIMDVectorMask ECS_VECTORCALL PointSameLineHalfPlane(Vector3 line_a, Vector3 line_b, Vector3 reference_point, Vector3 test_point) {
		return PointSameLineHalfPlaneImpl<SIMDVectorMask>(line_a, line_b, reference_point, test_point);
	}

	template<typename Mask, typename Vector>
	ECS_INLINE Mask ECS_VECTORCALL PointSameLineHalfPlaneNormalizedImpl(Vector line_point, Vector line_direction_normalized, Vector reference_point, Vector test_point) {
		// If multiple points were to be tested like this, an accelerated version can be used where the perpendicular line
		// Is calculated and cached such that it doesn't have to be recalculated every time
		Vector projected_point = ProjectPointOnLineDirectionNormalized(line_point, line_direction_normalized, test_point);
		return PointSameLineHalfPlaneProjected(line_point, reference_point, test_point, projected_point);
	}

	bool ECS_VECTORCALL PointSameLineHalfPlaneNormalized(float3 line_point, float3 line_direction_normalized, float3 reference_point, float3 test_point) {
		return PointSameLineHalfPlaneNormalizedImpl<bool>(line_point, line_direction_normalized, reference_point, test_point);
	}

	SIMDVectorMask ECS_VECTORCALL PointSameLineHalfPlaneNormalized(Vector3 line_point, Vector3 line_direction_normalized, Vector3 reference_point, Vector3 test_point) {
		return PointSameLineHalfPlaneNormalizedImpl<SIMDVectorMask>(line_point, line_direction_normalized, reference_point, test_point);
	}

	template<typename Mask, typename Vector>
	ECS_INLINE Mask ECS_VECTORCALL PointSameLineHalfPlaneProjectedImpl(
		Vector line_point,
		Vector reference_point,
		Vector test_point,
		Vector projected_test_point
	) {
		// If the dot product between the perpendicular line from the test point and the vector from
		// One point from the line to the reference point is negative, then the points are on opposite halfplanes
		// To be on the same halfplane, they need to have the dot product positive. A dot product of zero means
		// The test_point is the line
		Vector perpendicular_line = test_point - projected_test_point;
		return Dot(perpendicular_line, reference_point - line_point) > SingleZeroVector<Vector>();
	}

	bool ECS_VECTORCALL PointSameLineHalfPlaneProjected(
		float3 line_point,
		float3 reference_point,
		float3 test_point,
		float3 projected_test_point
	) {
		return PointSameLineHalfPlaneProjectedImpl<bool>(line_point, reference_point, test_point, projected_test_point);
	}

	SIMDVectorMask ECS_VECTORCALL PointSameLineHalfPlaneProjected(
		Vector3 line_point,
		Vector3 reference_point,
		Vector3 test_point,
		Vector3 projected_test_point
	) {
		return PointSameLineHalfPlaneProjectedImpl<SIMDVectorMask>(line_point, reference_point, test_point, projected_test_point);
	}

	// --------------------------------------------------------------------------------------------------------------

	// TODO: Should we keep the fast normalization? For the angle version probably yes, for the epsilon
	// One, the difference between fast and precise can have an impact

	bool ECS_VECTORCALL IsPointCollinear(float3 line_a, float3 line_b, float3 point, float epsilon) {
		return IsPointCollinearDirection(line_a, Normalize<ECS_VECTOR_FAST>(line_b - line_a), point, epsilon);
	}

	bool ECS_VECTORCALL IsPointCollinearDirection(float3 line_start, float3 line_direction_normalized, float3 point, float epsilon) {
		return IsParallelMask(line_direction_normalized, Normalize<ECS_VECTOR_FAST>(point - line_start), epsilon);
	}

	bool ECS_VECTORCALL IsPointCollinearByCosine(float3 line_a, float3 line_b, float3 point, float degrees) {
		return IsPointCollinearDirectionByCosine(line_a, Normalize<ECS_VECTOR_FAST>(line_b - line_a), point, degrees);
	}

	bool ECS_VECTORCALL IsPointCollinearDirectionByCosine(float3 line_start, float3 line_direction_normalized, float3 point, float cosine) {
		return IsParallelAngleCosineMask(line_direction_normalized, Normalize<ECS_VECTOR_FAST>(point - line_start), cosine);
	}

	// --------------------------------------------------------------------------------------------------------------

	template<typename Vector>
	ECS_INLINE static void ECS_VECTORCALL ClosestLinesPointsImpl(
		Vector first_line_point,
		Vector first_line_direction,
		Vector second_line_point,
		Vector second_line_direction,
		Vector* first_closest_point,
		Vector* second_closest_point
	) {
		// The line between the closest points must be perpendicular to both lines
		// Plugging this condition in the parametric representation of the line,
		// We get (thanks to Real-time Collision Detection)
		// s = (bf - ce) / d
		// t = (af - bc) / d
		// Where s is the interpolation factor for the first line, t for the second line
		// r = first_line_point - second_line_point
		// a = first_line_direction * first_line_direction
		// b = first_line_direction * second_line_direction
		// c = first_line_direction * r
		// e = second_line_direction * second_line_direction
		// f = second_line_direction * r
		// d = ae - b^2
		// If d is 0, then the lines are parallel, and this case needs to be handled separately
	
		auto epsilon = SingleValueVector<Vector>(ECS_SIMD_VECTOR_EPSILON_VALUE);

		// For the scalar code, we could firstly calculate d, and branch if it is parallel
		// But in order to keep a single code path, implement the SIMD version where we select
		// The value at the end
		auto a = Dot(first_line_direction, first_line_direction);
		auto e = Dot(second_line_direction, second_line_direction);
		auto b = Dot(first_line_direction, second_line_direction);
		auto d = a * e - b * b;
		// D technically needs to be >= 0.0f, but it can happen to have a small negative value
		// Due to floating point inaccuracies
		auto d_zero_mask = d < epsilon;

		auto r = first_line_point - second_line_point;
		auto c = Dot(first_line_direction, r);
		auto f = Dot(second_line_direction, r);

		// The general solution
		auto one_over_d = OneDividedVector(d);
		auto s = (b * f - c * e) * one_over_d;
		auto t = (a * f - b * c) * one_over_d;
		Vector first_general_closest_point = Fmadd(first_line_direction, Vector::Splat(s), first_line_point);
		Vector second_general_closest_point = Fmadd(second_line_direction, Vector::Splat(t), second_line_point);

		// The parallel solution
		// Choose a point on one of the lines, and project it on the other line
		Vector first_parallel_closest_point = first_line_point;
		// We can save a Dot here, since the internal function here needs e
		// So, write it inline
		//Vector second_parallel_closest_point = ProjectPointOnLineDirection(second_line_point, second_line_direction, first_parallel_closest_point);
		Vector second_parallel_closest_point = second_line_point + second_line_direction * (Dot(first_parallel_closest_point - second_line_point, second_line_direction) / e);

		*first_closest_point = Select(d_zero_mask, first_parallel_closest_point, first_general_closest_point);
		*second_closest_point = Select(d_zero_mask, second_parallel_closest_point, second_general_closest_point);
	}

	void ECS_VECTORCALL ClosestLinesPoints(
		float3 first_line_point,
		float3 first_line_direction,
		float3 second_line_point,
		float3 second_line_direction,
		float3* first_closest_point,
		float3* second_closest_point
	) {
		ClosestLinesPointsImpl(
			first_line_point,
			first_line_direction,
			second_line_point,
			second_line_direction,
			first_closest_point,
			second_closest_point
		);
	}

	void ECS_VECTORCALL ClosestLinesPoints(
		Vector3 first_line_point,
		Vector3 first_line_direction,
		Vector3 second_line_point,
		Vector3 second_line_direction,
		Vector3* first_closest_point,
		Vector3* second_closest_point
	) {
		ClosestLinesPointsImpl(
			first_line_point,
			first_line_direction,
			second_line_point,
			second_line_direction,
			first_closest_point,
			second_closest_point
		);
	}

	template<typename Vector>
	ECS_INLINE static void ECS_VECTORCALL ClosestSegmentPointsImpl(
		Vector first_line_A,
		Vector first_line_B,
		Vector second_line_A,
		Vector second_line_B,
		Vector* first_closest_point,
		Vector* second_closest_point
	) {
		// The line of thought for this function is similar to that of the line case
		// We compute the line closest points, and then we clamp those points to the
		// Segments 2 times. Due to some simplications, the solutions are (thanks to
		// Real-time Collision Detection)
		// s = (bf - ce) / d; // This is from the general case
		// s = (bt - c) / a;
		// t = (bs + f) / e;
		// a = d1 * d1; b = d1 * d2; c = d1 * r; d = ae - b^2; e = d2 * d2; f = d2 * r; r = second_line_A - first_line_A;
		// Here, we don't handle the degenerate case where the pair of points of a segment
		// Overlap each other
		
		auto d1 = first_line_B - first_line_A;
		auto d2 = second_line_B - second_line_A;
		auto r = first_line_A - second_line_A;
		auto a = Dot(d1, d1);
		auto b = Dot(d1, d2);
		auto c = Dot(d1, r);
		auto e = Dot(d2, d2);
		auto f = Dot(d2, r);

		auto epsilon = ECS_SIMD_VECTOR_EPSILON_VALUE;
		auto d = a * e - b * b;
		// Here we cannot branch on the d value, compute both branches and select
		auto greater_than_zero = d > epsilon;

		auto zero_value = SingleZeroVector<Vector>();
		auto one_value = OneVector<Vector>();

		// Use the first s formula
		auto non_parallel_s_value = ClampSingle((b * f - c * e) / d, zero_value, one_value);
		// For the parallel case, we can choose any s value we want. Here, 0.0f is the
		// Most convenient since we are already using it inside the clamping
		auto parallel_s_value = SingleZeroVector<Vector>();
		auto s = SelectSingle(greater_than_zero, non_parallel_s_value, parallel_s_value);

		// Now calculate t based on s and check to see if it is in the [0.0f, 1.0f] bounds
		// If not, we need to clamp it and recalculate s based on the t formula
		auto t = (b * s + f) / e;

		// Use a separate code path for the scalar case, since it can branch
		if constexpr (std::is_same_v<Vector, float3>) {
			if (t < 0.0f) {
				t = 0.0f;
				s = ClampSingle(-c / a, 0.0f, 1.0f);
			}
			else if (t > 1.0f) {
				t = 1.0f;
				s = ClampSingle((b - c) / a, 0.0f, 1.0f);
			}
		}
		else {
			// We cannot branch here, just clamp t, and use the general s formula
			// Unfortunately, we cannot skip calculations in the case that the
			// t is already in the [0.0f, 1.0f] range
			t = ClampSingle(t, zero_value, one_value);
			s = ClampSingle((b * t - c) / a, zero_value, one_value);
		}

		*first_closest_point = Fmadd(d1, Vector::Splat(s), first_line_A);
		*second_closest_point = Fmadd(d2, Vector::Splat(t), second_line_A);
	}

	void ECS_VECTORCALL ClosestSegmentPoints(
		float3 first_line_A,
		float3 first_line_B,
		float3 second_line_A,
		float3 second_line_B,
		float3* first_closest_point,
		float3* second_closest_point
	) {
		ClosestSegmentPointsImpl(first_line_A, first_line_B, second_line_A, second_line_B, first_closest_point, second_closest_point);
	}

	void ECS_VECTORCALL ClosestSegmentPoints(
		Vector3 first_line_A,
		Vector3 first_line_B,
		Vector3 second_line_A,
		Vector3 second_line_B,
		Vector3* first_closest_point,
		Vector3* second_closest_point
	) {
		ClosestSegmentPointsImpl(first_line_A, first_line_B, second_line_A, second_line_B, first_closest_point, second_closest_point);
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

	size_t FindFirstCloseFloat3(Stream<float3> values, float3 search_value, float3 epsilon) {
		for (size_t index = 0; index < values.size; index++) {
			if (CompareMask(values[index], search_value, epsilon)) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------

}