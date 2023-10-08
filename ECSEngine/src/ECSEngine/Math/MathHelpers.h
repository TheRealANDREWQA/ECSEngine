#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	template<bool is_delta = false, typename Value>
	ECS_INLINE Value Lerp(Value a, Value b, float percentage) {
		if constexpr (!is_delta) {
			return (b - a) * percentage + a;
		}
		else {
			return b * percentage + a;
		}
	}

	template<bool is_delta = false, typename Value>
	ECS_INLINE Value LerpDouble(Value a, Value b, double percentage) {
		if constexpr (!is_delta) {
			return (b - a) * percentage + a;
		}
		else {
			return b * percentage + a;
		}
	}

	template<typename Value>
	ECS_INLINE float InverseLerp(Value a, Value b, Value c) {
		return (c - a) / (b - a);
	}

	template<typename Value>
	Value PlanarLerp(Value a, Value b, Value c, Value d, float x_percentage, float y_percentage) {
		// Interpolation formula
		// a ----- b
		// |       |
		// |       |
		// |       |
		// c ----- d

		// result = a * (1 - x)(1 - y) + b * x (1 - y) + c * (1 - x) y + d * x y;

		return a * ((1.0f - x_percentage) * (1.0f - y_percentage)) + b * (x_percentage * (1.0f - y_percentage)) +
			c * ((1.0f - x_percentage) * y_percentage) + d * (x_percentage * y_percentage);
	}

	template<typename Function>
	ECS_INLINE float2 SampleFunction(float value, Function&& function) {
		return { value, function(value) };
	}

	template<typename Value>
	ECS_INLINE Value Clamp(Value value, Value min, Value max) {
		return ClampMax(ClampMin(value, min), max);
	}

	// The value will not be less than min
	template<typename Value>
	ECS_INLINE Value ClampMin(Value value, Value min) {
		return value < min ? min : value;
	}

	// The value will not be greater than max
	template<typename Value>
	ECS_INLINE Value ClampMax(Value value, Value max) {
		return value > max ? max : value;
	}

	// Returns a + b
// If it overflows, it will set it to the maximum value for that integer
	template<typename Integer>
	ECS_INLINE Integer SaturateAdd(Integer a, Integer b) {
		Integer new_value = a + b;
		Integer min, max;
		IntegerRange(min, max);
		return new_value < a ? max : new_value;
	}

	// Stores the result in the given pointer and returns the previous value at that pointer
	// If it overflows, it will set it to the maximum value for that integer
	template<typename Integer>
	ECS_INLINE Integer SaturateAdd(Integer* a, Integer b) {
		Integer old_value = *a;
		*a = SaturateAdd(*a, b);
		return old_value;
	}

	// Returns a - b
	// If it overflows, it will set it to the min value for that integer
	template<typename Integer>
	ECS_INLINE Integer SaturateSub(Integer a, Integer b) {
		Integer new_value = a - b;
		Integer min, max;
		IntegerRange(min, max);
		return new_value > a ? min : new_value;
	}

	// Stores the result in the given pointer and returns the previous value at that pointer
	// If it overflows, it will set it to the min value for that integer
	template<typename Integer>
	ECS_INLINE Integer SaturateSub(Integer* a, Integer b) {
		Integer old_value = *a;
		*a = SaturateSub(*a, b);
		return old_value;
	}

	// Returns the overlapping line as a pair of start, end (end is not included)
	// If the lines don't overlap, it will return { -1, -1 }
	ECSENGINE_API uint2 LineOverlap(
		unsigned int first_start,
		unsigned int first_end,
		unsigned int second_start,
		unsigned int second_end
	);

	// Returns the rectangle stored as xy - top left, zw - bottom right (the end is not included)
	// that is overlapping between these 2 rectangles. If zero_if_not_valid
	// is set to true then if one of the dimensions is 0, it will make the other
	// one 0 as well (useful for iteration for example, since it will result in
	// less iterations). If the rectangles don't overlap, it will return
	// { -1, -1, -1, -1 }
	ECSENGINE_API uint4 RectangleOverlap(
		uint2 first_top_left,
		uint2 first_bottom_right,
		uint2 second_top_left,
		uint2 second_bottom_right,
		bool zero_if_not_valid = true
	);

	// Positions will be filled with the 4 corners of the rectangle
	ECSENGINE_API void ObliqueRectangle(float2* positions, float2 a, float2 b, float thickness);

	ECSENGINE_API Rectangle3D GetRectangle3D(float3 center, float3 half_width, float3 half_height);

}