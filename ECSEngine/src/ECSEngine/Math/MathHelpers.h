#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"

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

	ECSENGINE_API FullRectangle3D GetRectangle3D(float3 center, float3 half_width, float3 half_height);

	// Uses SIMD operations
	ECSENGINE_API float3 CalculateFloat3Midpoint(Stream<float3> values);

	// Uses SIMD operations
	ECSENGINE_API float3 CalculateFloat3Midpoint(const float* values_x, const float* values_y, const float* values_z, size_t count);

	// Uses SIMD operations
	ECSENGINE_API void ApplyFloat3Addition(Stream<float3> values, float3 add_value);

	// Uses SIMD operations
	ECSENGINE_API void ApplyFloat3Subtraction(Stream<float3> values, float3 subtract_value);

	// Determines the extreme points in each direction - they lie on the AABB
	// There must be 6 entries in the values pointer.
	// They are filled in the X min, Y min, Z min, X max, Y max, Z max
	ECSENGINE_API void DetermineExtremePoints(Stream<float3> points, float3* values);

	enum ECS_WELD_VERTICES_TYPE : unsigned char {
		ECS_WELD_VERTICES_SNAP_FIRST,
		ECS_WELD_VERTICES_AVERAGE
	};

	// It will merge vertices which are close enough and return the new count.
	// You can specify an epsilon to detect close enough vertices (should not
	// be very large). If the relative epsilon is activated, the given epsilon
	// Is multiplied by a factor based on the span for the given axis
	template<ECS_WELD_VERTICES_TYPE type>
	ECSENGINE_API size_t WeldVertices(Stream<float3>& points, float3 epsilon = float3::Splat(0.0f), bool relative_epsilon = false);

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

	// Equivalent to a 1D simplex voronoi region detection
	ECSENGINE_API float3 ClosestPointToSegment(float3 segment_a, float3 segment_b, float3 point);

	// Equivalent to a 2D simplex voronoi region detection
	ECSENGINE_API float3 ClosestPointToTriangle(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point);

	// Equivalent to a 3D simplex voronoi region detection
	ECSENGINE_API float3 ClosestPointToTetrahedron(float3 A, float3 B, float3 C, float3 D, float3 point);

	enum ECS_SIMPLEX_VORONOI_REGION_TYPE : unsigned char {
		ECS_SIMPLEX_VORONOI_VERTEX,
		ECS_SIMPLEX_VORONOI_EDGE,
		ECS_SIMPLEX_VORONOI_FACE,
		ECS_SIMPLEX_VORONOI_TETRAHEDRON
	};

	struct Simplex1DVoronoiRegion {
		ECS_SIMPLEX_VORONOI_REGION_TYPE type;
		// This is the point projection on the simplex
		float3 projection;
		// For the vertex case, it will return the point to which it belongs
		// The other point is the second entry
		float3 points[2];
	};

	struct Simplex2DVoronoiRegion {
		ECS_SIMPLEX_VORONOI_REGION_TYPE type;
		// This is the point projection on the simplex
		float3 projection;
		// For the vertex case, it will fill in the point to which it belongs
		// For the edge case, the points that form the edge
		// The remaining points are in the other entries
		float3 points[3];
	};

	struct Simplex3DVoronoiRegion {
		ECS_SIMPLEX_VORONOI_REGION_TYPE type;
		// This is the point projection on the simplex
		float3 projection;
		// For the vertex case, it will fill in the point to which it belongs
		// For the edge case, the points that form the edge
		// For the triangle case, the points of the triangle
		// The remaining points are in the other entries
		float3 points[4];
	};

	ECSENGINE_API Simplex1DVoronoiRegion CalculateSimplex1DVoronoiRegion(float3 segment_a, float3 segment_b, float3 point);

	// It can compute the projection, if it is in the edge region faster than the other variant
	ECSENGINE_API Simplex1DVoronoiRegion CalculateSimplex1DVoronoiRegionWithDirection(
		float3 segment_a,
		float3 segment_b,
		float3 segment_normalized_direction,
		float3 point
	);

	ECSENGINE_API Simplex2DVoronoiRegion CalculateSimplex2DVoronoiRegion(float3 triangle_a, float3 triangle_b, float3 triangle_c, float3 point);

	ECSENGINE_API Simplex3DVoronoiRegion CalculateSimplex3DVoronoiRegion(float3 A, float3 B, float3 C, float3 D, float3 point);

	// This version does not use SIMD to compute the support point
	// Returns the index of the point
	ECSENGINE_API size_t ComputeSupportPointScalar(Stream<float3> points, float3 direction);

	// This version does not use SIMD to compute the farthest point
	// Returns the index of the point
	ECSENGINE_API size_t ComputeFarthestPointFromScalar(Stream<float3> points, float3 point);

	ECSENGINE_API float2 ClampPointToRectangle(float2 point, const Rectangle2D& rectangle);

	ECSENGINE_API float3 ClampPointToRectangle(float3 point, const Rectangle3D& rectangle);

	// Clips the first parameter such that it doesn't fall outside the clipper rectangle
	ECSENGINE_API Rectangle2D ClipRectangle(const Rectangle2D& rectangle, const Rectangle2D& clipper);

	// Clips the first parameter such that it doesn't fall outside the clipper rectangle while also
	// Outputting in the last parameter the uvs of the rectangle such they reflect correctly the clipping
	// The uvs parameter must contain the current UV coordinates of the rectangle
	ECSENGINE_API Rectangle2D ClipRectangleWithUVs(const Rectangle2D& rectangle, const Rectangle2D& clipper, Rectangle2D& uvs);

}