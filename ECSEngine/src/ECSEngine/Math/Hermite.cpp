#include "ecspch.h"
#include "Hermite.h"
#include "../Utilities/PointerUtilities.h"

namespace ECSEngine {

	//namespace SIMDHelpers {

	//	ECS_INLINE float2 ECS_VECTORCALL InterpolateHermite(Vector3 curve_vector, Vector3 percentage) {
	//		ECS_ASSERT(FORWARD(IsInRangeMask<true, true>(percentage, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>()));

	//		// Point1 ((1 + 2 * percentage)(1-percentage)^2) + slope1 (((1-percentage) ^ 2) * percentage) +
	//		// point2 ((3 - 2 * percentage)percentage ^ 2) + slope2 * (percentage ^ 2 (percentage - 1))
	//		Vector3 percentages(percentage);
	//		Vector3 one = VectorGlobals::ONE;

	//		Vector3 one_minus = one - percentages;
	//		Vector3 percentage_minus_one = percentages - one_minus;
	//		Vector3 three(3.0f);
	//		Vector3 two(2.0f);
	//		Vector3 two_percentages = two * percentages;

	//		Vector3 shuffle_1 = PerLaneBlend<0, 1, 6, 7>(one_minus, percentages);
	//		Vector3 shuffle_2 = shuffle_1;
	//		Vector3 point2_value = three - two_percentages;
	//		Vector3 point1_value = one + two_percentages;
	//		Vector3 shuffle_3_point1_slope1 = PerLaneBlend<4, 1, V_DC, V_DC>(percentages, point1_value);
	//		Vector3 shuffle_3_point2_slope2 = PerLaneBlend<V_DC, V_DC, 6, 3>(percentage_minus_one, point2_value);
	//		Vector3 shuffle_3 = PerLaneBlend<0, 1, 6, 7>(shuffle_3_point1_slope1, shuffle_3_point2_slope2);

	//		return PerLaneHorizontalAdd(curve_vector * shuffle_1 * shuffle_2 * shuffle_3).GetFirsts();
	//	}

	//}

	//float ECS_VECTORCALL InterpolateHermite(HermiteFloat curve, Vector3 percentage) {
	//	return SIMDHelpers::InterpolateHermite(Vector3((const float*)&curve), percentage).x;
	//}

	//float2 ECS_VECTORCALL InterpolateHermite(HermiteFloat curve0, HermiteFloat curve1, Vector3 percentage) {
	//	Vector3 curve_vector = Vector3(float4((const float*)&curve0), float4((const float*)&curve1));
	//	return SIMDHelpers::InterpolateHermite(curve_vector, percentage);
	//}

	//float2 ECS_VECTORCALL InterpolateHermite(HermiteFloat2 curve, Vector3 percentage) {
	//	bool is_in_range = IsInRangeMask<true, true>(percentage, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>();
	//	ECS_ASSERT(is_in_range);

	//	// Point1 ((1 + 2 * percentage)(1-percentage)^2) + slope1 (((1-percentage) ^ 2) * percentage) +
	//	// point2 ((3 - 2 * percentage)percentage ^ 2) + slope2 * (percentage ^ 2 (percentage - 1))
	//	Vector3 curve_vector(&curve);
	//	Vector3 percentages(percentage);
	//	Vector3 one = VectorGlobals::ONE;

	//	Vector3 one_minus = one - percentages;
	//	Vector3 percentage_minus_one = percentages - one_minus;
	//	Vector3 three(3.0f);
	//	Vector3 two(2.0f);
	//	Vector3 two_percentages = two * percentages;

	//	Vector3 shuffle_1 = BlendLowAndHigh(one_minus, percentages);
	//	Vector3 shuffle_2 = shuffle_1;
	//	Vector3 point2_value = three - two_percentages;
	//	Vector3 point1_value = one + two_percentages;
	//	Vector3 shuffle_3_point1_slope1 = blend8<8, 9, 2, 3, V_DC, V_DC, V_DC, V_DC>(percentages, point1_value);
	//	Vector3 shuffle_3_point2_slope2 = blend8<V_DC, V_DC, V_DC, V_DC, 12, 13, 6, 7>(percentage_minus_one, point2_value);
	//	Vector3 shuffle_3 = BlendLowAndHigh(shuffle_3_point1_slope1, shuffle_3_point2_slope2);

	//	alignas(ECS_SIMD_BYTE_SIZE) float values[curve_vector.value.size()];
	//	Vector3 result = curve_vector * shuffle_1 * shuffle_2 * shuffle_3;
	//	result.StoreAligned(values);

	//	// We need to add together 0, 2, 4 and 6
	//	// And in the other lane 1, 3, 5, 7
	//	return { values[0] + values[2] + values[4] + values[6], values[1] + values[3] + values[5] + values[7] };
	//}

	//float3 ECS_VECTORCALL InterpolateHermite(const HermiteFloat3& curve, Vector3 percentage) {
	//	bool is_in_range = IsInRangeMask<true, true>(percentage, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>();
	//	ECS_ASSERT(is_in_range);

	//	// Point1 ((1 + 2 * percentage)(1-percentage)^2) + slope1 (((1-percentage) ^ 2) * percentage) +
	//	// point2 ((3 - 2 * percentage)percentage ^ 2) + slope2 * (percentage ^ 2 (percentage - 1))

	//	// use 2 SIMD vectors: one for point1 and slope1 and the other one for point2 and slope2
	//	float3 result;
	//	Vector3 curve_low(&curve);
	//	Vector3 curve_high(OffsetPointer(&curve, sizeof(float) * 6));

	//	Vector3 percentages(percentage);
	//	Vector3 one = VectorGlobals::ONE;
	//	Vector3 one_minus = one - percentages;

	//	Vector3 percentage_minus_one = percentages - one;
	//	Vector3 three(3.0f);
	//	Vector3 two(2.0f);
	//	Vector3 two_percentages = two * percentages;
	//	Vector3 point1_value = one + two_percentages;
	//	Vector3 point2_value = three - two_percentages;
	//	Vector3 slope2_value = percentage_minus_one;

	//	Vector3 shuffle_1_low = one_minus;
	//	Vector3 shuffle_2_low = one_minus;
	//	Vector3 shuffle_3_low = blend8<0, 1, 2, 11, 12, 13, V_DC, V_DC>(point1_value, percentages);

	//	Vector3 shuffle_1_high = blend8<0, 1, 2, 11, 12, 13, V_DC, V_DC>(point2_value, slope2_value);
	//	Vector3 shuffle_2_high = percentages;
	//	Vector3 shuffle_3_high = percentages;

	//	Vector3 low = curve_low * shuffle_1_low * shuffle_2_low * shuffle_3_low;
	//	Vector3 high = curve_high * shuffle_1_high * shuffle_2_high * shuffle_3_high;
	//	Vector3 semi_added = low + high;
	//	Vector3 shuffle_add = permute8<4, 5, 6, V_DC, V_DC, V_DC, V_DC, V_DC>(semi_added);
	//	Vector3 vector_result = semi_added + shuffle_add;
	//	return vector_result.AsFloat3Low();
	//}

	//float4 ECS_VECTORCALL InterpolateHermite(const HermiteFloat4& curve, Vector3 percentage) {
	//	bool is_in_range = IsInRangeMask<true, true>(percentage, ZeroVector(), VectorGlobals::ONE).MaskResultWhole<4>();
	//	ECS_ASSERT(is_in_range);

	//	// Point1 ((1 + 2 * percentage)(1-percentage)^2) + slope1 (((1-percentage) ^ 2) * percentage) +
	//	// point2 ((3 - 2 * percentage)percentage ^ 2) + slope2 * (percentage ^ 2 (percentage - 1))
	//	float4 result;
	//	Vector3 curve_low(&curve);
	//	Vector3 curve_high(OffsetPointer(&curve, sizeof(float) * 8));

	//	Vector3 percentages(percentage);
	//	Vector3 one = VectorGlobals::ONE;
	//	Vector3 one_minus = one - percentages;
	//	Vector3 two(2.0f);
	//	Vector3 three(3.0f);

	//	Vector3 percentage_minus_one = percentages - one;
	//	Vector3 two_percentages = two * percentages;
	//	Vector3 point2_value = three - two_percentages;
	//	Vector3 point1_value = one + two_percentages;

	//	Vector3 shuffle_1_low = one_minus;
	//	Vector3 shuffle_2_low = one_minus;
	//	Vector3 shuffle_3_low = BlendLowAndHigh(point1_value, percentages);

	//	Vector3 shuffle_1_high = percentages;
	//	Vector3 shuffle_2_high = percentages;
	//	Vector3 shuffle_3_high = BlendLowAndHigh(point2_value, percentage_minus_one);

	//	Vector3 low = curve_low * shuffle_1_low * shuffle_2_low * shuffle_3_low;
	//	Vector3 high = curve_high * shuffle_1_high * shuffle_2_high * shuffle_3_high;
	//	Vector3 semi_addition = low + high;
	//	Vector3 permutation = permute8<4, 5, 6, 7, V_DC, V_DC, V_DC, V_DC>(semi_addition);
	//	Vector3 vector_result = semi_addition + permutation;
	//	return vector_result.AsFloat4Low();
	//}

}