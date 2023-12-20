#pragma once
#include "Vector.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	template<typename BasicType>
	struct BezierBase {
		ECS_INLINE BezierBase() {}
		ECS_INLINE BezierBase(BasicType _point1, BasicType _point2, BasicType _control1, BasicType _control2) : point1(_point1),
			point2(_point2), control1(_control1), control2(_control2) {}

		BasicType point1;
		BasicType point2;
		BasicType control1;
		BasicType control2;
	};

	struct BezierFloat {
		ECS_INLINE BezierFloat() {}
		ECS_INLINE BezierFloat(float _point1, float _point2, float _control1, float _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float point1;
		float control1;
		float control2;
		float point2;
	};

	struct BezierFloat2 {
		ECS_INLINE BezierFloat2() {}
		ECS_INLINE BezierFloat2(float2 _point1, float2 _point2, float2 _control1, float2 _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float2 point1;
		float2 control1;
		float2 control2;
		float2 point2;
	};

	struct BezierFloat3 {
		ECS_INLINE BezierFloat3() {}
		ECS_INLINE BezierFloat3(float3 _point1, float3 _point2, float3 _control1, float3 _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float3 point1;
		float3 control1;
		float3 control2;
		float3 point2;
	};

	struct BezierFloat4 {
		ECS_INLINE BezierFloat4() {}
		ECS_INLINE BezierFloat4(float4 _point1, float4 _point2, float4 _control1, float4 _control2) : point1(_point1), point2(_point2),
			control1(_control1), control2(_control2) {}

		float4 point1;
		float4 control1;
		float4 control2;
		float4 point2;
	};

	// This is generic functions - for more performance use the native float variants
	template<typename BasicType>
	BasicType InterpolateBezier(const BezierBase<BasicType>& curve, float percentage) {
		ECS_ASSERT(percentage >= 0.0f && percentage <= 1.0f);

		// Point1 ((1-percentage)^3) + control1 (3(1-percentage^2)percentage) +
		// control2 (3(1-percentage)percentage ^ 2) + point2 * (percentage ^ 3)
		float one_minus = 1.0f - percentage;
		return curve.point1 * one_minus * one_minus * one_minus + curve.control1 * (3.0f * one_minus * one_minus * percentage) +
			curve.control2 * (3.0f * one_minus * percentage * percentage) + curve.point2 * percentage * percentage * percentage;
	}

	ECSENGINE_API float ECS_VECTORCALL InterpolateBezier(BezierFloat curve, Vector8 percentage);

	ECSENGINE_API float2 ECS_VECTORCALL InterpolateBezier(BezierFloat curve0, BezierFloat curve1, Vector8 percentage);

	ECSENGINE_API float2 ECS_VECTORCALL InterpolateBezier(BezierFloat2 curve, Vector8 percentage);

	ECSENGINE_API float3 ECS_VECTORCALL InterpolateBezier(const BezierFloat3& curve, Vector8 percentages);

	ECSENGINE_API float4 ECS_VECTORCALL InterpolateBezier(const BezierFloat4& curve, Vector8 percentages);

}