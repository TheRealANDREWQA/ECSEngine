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

	// Uses SIMD operations
	ECSENGINE_API float3 CalculateFloat3Midpoint(Stream<float3> values);

	// Uses SIMD operations
	ECSENGINE_API void ApplyFloat3Addition(Stream<float3> values, float3 add_value);

	// Uses SIMD operations
	ECSENGINE_API void ApplyFloat3Subtraction(Stream<float3> values, float3 subtract_value);

	// Determines the extreme points in each direction - they lie on the AABB
	// There must be 6 entries in the values pointer.
	// They are filled in the X min, Y min, Z min, X max, Y max, Z max
	ECSENGINE_API void DetermineExtremePoints(Stream<float3> points, float3* values);

	// It will merge vertices which are close enough and return the new count.
	// You can specify an epsilon to detect close enough vertices (should not
	// be very large)
	ECSENGINE_API size_t WeldVertices(Stream<float3>& points, float3 epsilon = float3::Splat(0.0f));

	// Returns the normal calculated as the cross product between AB and AC
	// The normal is not normalized. You must do that manually
	ECSENGINE_API float3 TriangleNormal(float3 a, float3 b, float3 c);

	// Returns the normal of the triangle facing the given point
	// The normal is not normalized. You must do that manually
	ECSENGINE_API float3 TriangleNormal(float3 a, float3 b, float3 c, float3 look_point);

	// Returns true if the triangle ABC is facing d using the TriangleNormal function
	ECSENGINE_API bool IsTriangleFacing(float3 a, float3 b, float3 c, float3 d);

	ECSENGINE_API float TriangleArea(float3 point_a, float3 point_b, float3 point_c);

	ECSENGINE_API float TetrahedronVolume(float3 point_a, float3 point_b, float3 point_c, float3 point_d);

	ECSENGINE_API float GetFloat3MinX(Stream<float3> values, size_t* index = nullptr);

	ECSENGINE_API float GetFloat3MinY(Stream<float3> values, size_t* index = nullptr);

	ECSENGINE_API float GetFloat3MinZ(Stream<float3> values, size_t* index = nullptr);

	ECSENGINE_API float GetFloat3MaxX(Stream<float3> values, size_t* index = nullptr);

	ECSENGINE_API float GetFloat3MaxY(Stream<float3> values, size_t* index = nullptr);

	ECSENGINE_API float GetFloat3MaxZ(Stream<float3> values, size_t* index = nullptr);

	// The min is the x, the max is the y
	ECSENGINE_API float2 GetFloat3MinMaxX(Stream<float3> values, ulong2* indices = nullptr);

	// The min is the x, the max is the y
	ECSENGINE_API float2 GetFloat3MinMaxY(Stream<float3> values, ulong2* indices = nullptr);

	// The min is the x, the max is the y
	ECSENGINE_API float2 GetFloat3MinMaxZ(Stream<float3> values, ulong2* indices = nullptr);

	// Finds the corresponding min for each x, y and z separately and it can give 
	// you back the indices of these elements
	ECSENGINE_API float3 GetFloat3Min(Stream<float3> values, ulong3* indices = nullptr);

	// Finds the corresponding max for each x, y and z separately and it can give
	// you back the indices of these elements
	ECSENGINE_API float3 GetFloat3Max(Stream<float3> values, ulong3* indices = nullptr);

	// Finds the corresponding min and max for each x, y and z separately
	ECSENGINE_API void GetFloat3MinMax(Stream<float3> values, float3* min, float3* max, ulong3* min_indices = nullptr, ulong3* max_indices = nullptr);

	// Returns the allocated counts. There are bucket_count + 1 entries to satisfy the
	// Count sort requirement. The values are sorted ascending
	ECSENGINE_API unsigned int* CountSortFloat3X(Stream<float3> values, float3* output_values, size_t bucket_count, AllocatorPolymorphic allocator);

	// Returns the allocated counts. There are bucket_count + 1 entries to satisfy the
	// Count sort requirement. The values are sorted ascending
	ECSENGINE_API unsigned int* CountSortFloat3Y(Stream<float3> values, float3* output_values, size_t bucket_count, AllocatorPolymorphic allocator);

	// Returns the allocated counts. There are bucket_count + 1 entries to satisfy the
	// Count sort requirement. The values are sorted ascending
	ECSENGINE_API unsigned int* CountSortFloat3Z(Stream<float3> values, float3* output_values, size_t bucket_count, AllocatorPolymorphic allocator);

}