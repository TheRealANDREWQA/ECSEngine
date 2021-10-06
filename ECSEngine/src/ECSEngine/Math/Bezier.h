#pragma once
#include "Vector.h"
#include "../Utilities/Assert.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	template<typename BasicType>
	struct BezierBase {
		BezierBase() {}
		BezierBase(BasicType _point1, BasicType _point2, BasicType _control1, BasicType _control2) : point1(_point1),
			point2(_point2), control1(_control1), control2(_control2) {}

		BasicType point1;
		BasicType point2;
		BasicType control1;
		BasicType control2;
	};

	template<typename BasicType>
	BasicType InterpolateBezier(const BezierBase<BasicType>& curve, float percentage) {
		ECS_ASSERT(percentage >= 0.0f && percentage <= 1.0f);

		// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
		// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)
		float one_minus = 1.0f - percentage;
		return curve.point1 * one_minus * one_minus * one_minus + curve.control1 * (3.0f * one_minus * one_minus * percentage) +
			curve.control2 * (3.0f * one_minus * percentage * percentage) + curve.point2 * percentage * percentage * percentage;
	}

	struct BezierFloat {
		BezierFloat() {}
		BezierFloat(float _point1, float _point2, float _control1, float _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float point1;
		float control1;
		float control2;
		float point2;
	};

	struct BezierFloat2 {
		BezierFloat2() {}
		BezierFloat2(float2 _point1, float2 _point2, float2 _control1, float2 _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float2 point1;
		float2 control1;
		float2 control2;
		float2 point2;
	};

	struct BezierFloat3 {
		BezierFloat3() {}
		BezierFloat3(float3 _point1, float3 _point2, float3 _control1, float3 _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float3 point1;
		float3 control1;
		float3 control2;
		float3 point2;
	};

	struct BezierFloat4 {
		BezierFloat4() {}
		BezierFloat4(float4 _point1, float4 _point2, float4 _control1, float4 _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float4 point1;
		float4 control1;
		float4 control2;
		float4 point2;
	};

	ECS_INLINE float ECS_VECTORCALL InterpolateBezier(BezierFloat curve, float percentage) {
		ECS_ASSERT(0.0f <= percentage && percentage <= 1.0f);
		// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
		// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)
		Vector4 curve_vector(&curve);
		Vector4 one = VectorGlobals::ONE_4;
		Vector4 percentages(percentage);

		Vector4 one_minus = one - percentages;
		Vector4 shuffle_1 = blend4<4, 5, 6, 3>(percentages, one_minus);
		Vector4 shuffle_2 = blend4<4, 5, 2, 3>(percentages, one_minus);
		Vector4 shuffle_3 = blend4<4, 1, 2, 3>(percentages, one_minus);

		Vector4 scalar_factors(1.0f, 3.0f, 3.0f, 1.0f);
		Vector4 factors = shuffle_1 * shuffle_2 * shuffle_3 * scalar_factors;
		factors *= curve_vector;
		return horizontal_add(factors);
	}

	ECS_INLINE float2 ECS_VECTORCALL InterpolateBezier(BezierFloat2 curve, float percentage) {
		ECS_ASSERT(0.0f <= percentage && percentage <= 1.0f);
		// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
		// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)

		Vector8 curve_vector(&curve);
		Vector8 one = VectorGlobals::ONE_8;
		Vector8 percentages(percentage);

		Vector8 one_minus = one - percentages;
		Vector8 shuffle_1 = blend8<8, 9, 10, 11, 12, 13, 6, 7>(percentages, one_minus);
		Vector8 shuffle_2 = blend8<8, 9, 10, 11, 4, 5, 6, 7>(percentages, one_minus);
		Vector8 shuffle_3 = blend8<8, 9, 2, 3, 4, 5, 6, 7>(percentages, one_minus);
		
		Vector8 scalar_factors(1.0f, 1.0f, 3.0f, 3.0f, 3.0f, 3.0f, 1.0f, 1.0f);
		Vector8 factors = shuffle_1 * shuffle_2 * shuffle_3 * scalar_factors;
		factors *= curve_vector;
		
		alignas(curve_vector.value.size() * 4) float values[curve_vector.value.size()];
		factors.StoreAligned(values);
		return { values[0] + values[2] + values[4] + values[6], values[1] + values[3] + values[5] + values[7] };
	}


	ECS_INLINE float3 ECS_VECTORCALL InterpolateBezier(const BezierFloat2& curve, float percentage) {
		ECS_ASSERT(0.0f <= percentage && percentage <= 1.0f);
		// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
		// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)

		// Only the first 6 values will be used, the other two will be hoisted into
		// the other SIMD register
		Vector8 curve_vector1(&curve);
		Vector8 curve_vector2(function::OffsetPointer(&curve, sizeof(float) * 6));

		Vector8 one = VectorGlobals::ONE_8;
		Vector8 percentages(percentage);
		Vector8 one_minus = one - percentages;

		Vector8 shuffle_low_1 = one_minus;
		Vector8 shuffle_low_2 = one_minus;
		Vector8 shuffle_low_3 = blend8<8, 9, 10, 3, 4, 5, V_DC, V_DC>(percentages, one_minus);

		Vector8 shuffle_high_1 = blend8<8, 9, 10, 3, 4, 5, V_DC, V_DC>(percentages, one_minus);
		Vector8 shuffle_high_2 = percentages;
		Vector8 shuffle_high_3 = percentages;
		
		Vector8 scalar_factors(1.0f, 1.0f, 1.0f, 3.0f, 3.0f, 3.0f, 1.0f, 1.0f);
		Vector8 scalar_factors_low = scalar_factors;
		Vector8 scalar_factors_high = permute8<3, 3, 3, 0, 0, 0, V_DC, V_DC>(scalar_factors);

		Vector8 factors_low = shuffle_low_1 * shuffle_low_2 * shuffle_low_3 * scalar_factors_low * curve_vector1;
		Vector8 factors_high = shuffle_high_1 * shuffle_high_2 * shuffle_high_3 * scalar_factors_high * curve_vector2;

		Vector8 factors_semi_added = factors_low + factors_high;
		Vector8 permutation = permute8<3, 4, 5, V_DC, V_DC, V_DC, V_DC, V_DC>(factors_semi_added);
		Vector8 values = factors_semi_added + permutation;

		alignas(one.value.size() * sizeof(float)) float scalar_values[one.value.size()];
		values.StoreAligned(scalar_values);

		return { scalar_values };
	}

	ECS_INLINE float4 ECS_VECTORCALL InterpolateBezier(const BezierFloat4& curve, float percentage) {
		ECS_ASSERT(0.0f <= percentage && percentage <= 1.0f);
		// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
		// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)

		float4 result;

		Vector8 curve_low(&curve);
		Vector8 curve_high(function::OffsetPointer(&curve, sizeof(float) * 8));

		Vector8 one = VectorGlobals::ONE_8;
		Vector8 percentages(percentage);
		Vector8 one_minus = one - percentages;

		Vector8 shuffle_low_1 = one_minus;
		Vector8 shuffle_low_2 = one_minus;
		Vector8 shuffle_low_3 = blend8<8, 9, 10, 11, 4, 5, 6, 7>(percentages, one_minus);

		Vector8 shuffle_high_1 = blend8<8, 9, 10, 11, 4, 5, 6, 7>(percentages, one_minus);
		Vector8 shuffle_high_2 = percentages;
		Vector8 shuffle_high_3 = percentages;

		Vector8 scalar_factors(1.0f, 1.0f, 1.0f, 1.0f, 3.0f, 3.0f, 3.0f, 3.0f);
		Vector8 scalar_factors_low = scalar_factors;
		Vector8 scalar_factors_high = permute8<4, 5, 6, 7, 0, 1, 2, 3>(scalar_factors);

		Vector8 factors_low = shuffle_low_1 * shuffle_low_2 * shuffle_low_3 * scalar_factors_low * curve_low;
		Vector8 factors_high = shuffle_high_1 * shuffle_high_2 * shuffle_high_3 * scalar_factors_high * curve_high;

		Vector8 factors_semi_added = factors_low + factors_high;
		Vector8 factors_permuted = permute8<4, 5, 6, 7, V_DC, V_DC, V_DC, V_DC>(factors_semi_added);
		factors_semi_added += factors_permuted;

		factors_semi_added.Low().Store(&result);
		return result;
	}
}