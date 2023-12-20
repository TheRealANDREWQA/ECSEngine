#pragma once
#include "Vector.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	template<typename BasicType>
	struct HermiteBase {
		ECS_INLINE HermiteBase() {}
		ECS_INLINE HermiteBase(BasicType _point1, BasicType _slope1, BasicType _point2, BasicType _slope2) : point1(_point1),
			point2(_point2), slope1(_slope1), slope2(_slope2) {}

		BasicType point1;
		BasicType slope1;
		BasicType point2;
		BasicType slope2;
	};

	struct HermiteFloat {
		ECS_INLINE HermiteFloat() {}
		ECS_INLINE HermiteFloat(float _point1, float _slope1, float _point2, float _slope2) : point1(_point1),
			point2(_point2), slope1(_slope1), slope2(_slope2) {}

		float point1;
		float slope1;
		float point2;
		float slope2;
	};

	struct HermiteFloat2 {
		ECS_INLINE HermiteFloat2() {}
		ECS_INLINE HermiteFloat2(float2 _point1, float2 _slope1, float2 _point2, float2 _slope2) : point1(_point1),
			point2(_point2), slope1(_slope1), slope2(_slope2) {}

		float2 point1;
		float2 slope1;
		float2 point2;
		float2 slope2;
	};

	struct HermiteFloat3 {
		ECS_INLINE HermiteFloat3() {}
		ECS_INLINE HermiteFloat3(float3 _point1, float3 _slope1, float3 _point2, float3 _slope2) : point1(_point1),
			point2(_point2), slope1(_slope1), slope2(_slope2) {}

		float3 point1;
		float3 slope1;
		float3 point2;
		float3 slope2;
	};

	struct HermiteFloat4 {
		ECS_INLINE HermiteFloat4() {}
		ECS_INLINE HermiteFloat4(float4 _point1, float4 _slope1, float4 _point2, float4 _slope2) : point1(_point1),
			point2(_point2), slope1(_slope1), slope2(_slope2) {}

		float4 point1;
		float4 slope1;
		float4 point2;
		float4 slope2;
	};

	// This is a general function - use the specialized versions for floats
	template<typename BasicType>
	BasicType InterpolateHermite(const HermiteBase<BasicType>& curve, float percentage) {
		ECS_ASSERT(percentage >= 0.0f && percentage <= 1.0f);

		// Point1 ((1 + 2 * percentage)(1-percentage)^2) + slope1 ((1-percentage) ^ 2 * percentage) +
		// point2 ((3 - 2 * percentage)percentage ^ 2) + slope2 * (percentage ^ 2 (percentage - 1))
		return curve.point1 * (1.0f + 2.0f * percentage)(1.0f - percentage)(1.0f - percentage) + curve.slope1 * ((1.0f - percentage) * (1.0f - percentage) * percentage)
			+ curve.point2 * (3.0f - 2 * percentage)(percentage * percentage) + curve.slope2 * (percentage * percentage * (percentage - 1.0f));
	}

	ECSENGINE_API float ECS_VECTORCALL InterpolateHermite(HermiteFloat curve, Vector8 percentage);

	ECSENGINE_API float2 ECS_VECTORCALL InterpolateHermite(HermiteFloat curve0, HermiteFloat curve1, Vector8 percentage);

	ECSENGINE_API float2 ECS_VECTORCALL InterpolateHermite(HermiteFloat2 curve, Vector8 percentage);

	ECSENGINE_API float3 ECS_VECTORCALL InterpolateHermite(const HermiteFloat3& curve, Vector8 percentage);

	ECSENGINE_API float4 ECS_VECTORCALL InterpolateHermite(const HermiteFloat4& curve, Vector8 percentage);

}