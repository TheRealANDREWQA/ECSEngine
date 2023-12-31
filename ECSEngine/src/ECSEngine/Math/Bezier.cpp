#include "ecspch.h"
#include "Bezier.h"
#include "../Utilities/PointerUtilities.h"

namespace ECSEngine {

	//namespace SIMDHelpers {

	//	ECS_INLINE float2 ECS_VECTORCALL InterpolateBezier(Vector3 curve_vector, Vector3 percentages) {
	//		bool is_in_range = IsInRangeMask<true, true>(percentages, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>();
	//		ECS_ASSERT(is_in_range);

	//		// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
	//		// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)

	//		Vector3 one = VectorGlobals::ONE;

	//		Vector3 one_minus = one - percentages;
	//		Vector3 shuffle_1 = PerLaneBlend<4, 5, 6, 3>(percentages, one_minus);
	//		Vector3 shuffle_2 = PerLaneBlend<4, 5, 2, 3>(percentages, one_minus);
	//		Vector3 shuffle_3 = PerLaneBlend<4, 1, 2, 3>(percentages, one_minus);

	//		Vector3 three = 3.0f;
	//		Vector3 scalar_factors = PerLaneBlend<0, 5, 6, 3>(one, three);
	//		Vector3 factors = shuffle_1 * shuffle_2 * shuffle_3 * scalar_factors;
	//		return PerLaneHorizontalAdd(factors * curve_vector).GetFirsts();
	//	}

	//}

	//float ECS_VECTORCALL InterpolateBezier(BezierFloat curve, Vector3 percentage) {
	//	return SIMDHelpers::InterpolateBezier(Vector3(&curve), percentage).x;
	//}

	//float2 ECS_VECTORCALL InterpolateBezier(BezierFloat curve0, BezierFloat curve1, Vector3 percentage) {
	//	Vector3 curve_vector = Vector3(float4((const float*)&curve0), float4((const float*)&curve1));
	//	return SIMDHelpers::InterpolateBezier(curve_vector, percentage);
	//}

	//float2 ECS_VECTORCALL InterpolateBezier(BezierFloat2 curve, Vector3 percentage) {
	//	Vector3 curve_vector(&curve);

	//	bool is_in_range = IsInRangeMask<true, true>(percentage, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>();
	//	ECS_ASSERT(is_in_range);

	//	// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
	//	// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)

	//	Vector3 one = VectorGlobals::ONE;

	//	Vector3 one_minus = one - percentage;
	//	Vector3 shuffle_1 = blend8<8, 9, 10, 11, 12, 13, 6, 7>(percentage, one_minus);
	//	Vector3 shuffle_2 = blend8<8, 9, 10, 11, 4, 5, 6, 7>(percentage, one_minus);
	//	Vector3 shuffle_3 = blend8<8, 9, 2, 3, 4, 5, 6, 7>(percentage, one_minus);

	//	Vector3 three = 3.0f;
	//	Vector3 scalar_factors = blend8<0, 1, 10, 11, 12, 13, 6, 7>(one, three);
	//	Vector3 factors = shuffle_1 * shuffle_2 * shuffle_3 * scalar_factors;
	//	return PerLaneHorizontalAdd(factors * curve_vector).GetFirsts();
	//}


	//float3 ECS_VECTORCALL InterpolateBezier(const BezierFloat3& curve, Vector3 percentages) {
	//	// Only the first 6 values will be used, the other two will be hoisted into
	//	// the other SIMD register
	//	Vector3 curve_vector1(&curve);
	//	Vector3 curve_vector2(OffsetPointer(&curve, sizeof(float) * 6));

	//	bool is_in_range = IsInRangeMask<true, true>(percentages, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>();
	//	ECS_ASSERT(is_in_range);
	//	// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
	//	// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)


	//	Vector3 one = VectorGlobals::ONE;
	//	Vector3 one_minus = one - percentages;

	//	Vector3 shuffle_low_1 = one_minus;
	//	Vector3 shuffle_low_2 = one_minus;
	//	Vector3 shuffle_low_3 = blend8<8, 9, 10, 3, 4, 5, V_DC, V_DC>(percentages, one_minus);

	//	Vector3 shuffle_high_1 = blend8<8, 9, 10, 3, 4, 5, V_DC, V_DC>(percentages, one_minus);
	//	Vector3 shuffle_high_2 = percentages;
	//	Vector3 shuffle_high_3 = percentages;

	//	Vector3 three = 3.0f;
	//	Vector3 scalar_factors = blend8<0, 1, 2, 11, 12, 13, 6, 7>(one, three);
	//	Vector3 scalar_factors_low = scalar_factors;
	//	// 3.0f, 3.0f, 3.0f, 1.0f, 1.0f, 1.0f
	//	Vector3 scalar_factors_high = permute8<3, 3, 3, 0, 7, 7, V_DC, V_DC>(scalar_factors);

	//	Vector3 factors_low = shuffle_low_1 * shuffle_low_2 * shuffle_low_3 * scalar_factors_low * curve_vector1;
	//	Vector3 factors_high = shuffle_high_1 * shuffle_high_2 * shuffle_high_3 * scalar_factors_high * curve_vector2;

	//	Vector3 factors_semi_added = factors_low + factors_high;
	//	Vector3 permutation = permute8<3, 4, 5, V_DC, V_DC, V_DC, V_DC, V_DC>(factors_semi_added);
	//	Vector3 values = factors_semi_added + permutation;

	//	return values.AsFloat3Low();
	//}

	//float4 ECS_VECTORCALL InterpolateBezier(const BezierFloat4& curve, Vector3 percentages) {
	//	Vector3 curve_low(&curve);
	//	Vector3 curve_high(OffsetPointer(&curve, sizeof(float) * 8));

	//	bool is_in_range = IsInRangeMask<true, true>(percentages, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>();
	//	ECS_ASSERT(is_in_range);
	//	// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
	//	// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)

	//	Vector3 one = VectorGlobals::ONE;
	//	Vector3 one_minus = one - percentages;

	//	Vector3 shuffle_low_1 = one_minus;
	//	Vector3 shuffle_low_2 = one_minus;
	//	Vector3 shuffle_low_3 = blend8<8, 9, 10, 11, 4, 5, 6, 7>(percentages, one_minus);

	//	Vector3 shuffle_high_1 = blend8<8, 9, 10, 11, 4, 5, 6, 7>(percentages, one_minus);
	//	Vector3 shuffle_high_2 = percentages;
	//	Vector3 shuffle_high_3 = percentages;

	//	Vector3 three = 3.0f;
	//	Vector3 scalar_factors = BlendLowAndHigh(one, three);
	//	Vector3 scalar_factors_low = scalar_factors;
	//	Vector3 scalar_factors_high = permute8<4, 5, 6, 7, 0, 1, 2, 3>(scalar_factors);

	//	Vector3 factors_low = shuffle_low_1 * shuffle_low_2 * shuffle_low_3 * scalar_factors_low * curve_low;
	//	Vector3 factors_high = shuffle_high_1 * shuffle_high_2 * shuffle_high_3 * scalar_factors_high * curve_high;

	//	Vector3 factors_semi_added = factors_low + factors_high;
	//	Vector3 factors_permuted = permute8<4, 5, 6, 7, V_DC, V_DC, V_DC, V_DC>(factors_semi_added);
	//	factors_semi_added += factors_permuted;

	//	return factors_semi_added.AsFloat4Low();
	//}

}