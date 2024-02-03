#include "ecspch.h"
#include "MathHelpers.h"
#include "../Utilities/Utilities.h"
#include "VCLExtensions.h"
#include "Vector.h"
#include "../Utilities/Algorithms.h"
#include "SpatialGrid.h"
#include "../Allocators/ResizableLinearAllocator.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	void ObliqueRectangle(float2* positions, float2 a, float2 b, float thickness)
	{
		// calculating the slope of the perpendicular to AB segment
		// ab_slope = (b.y - a.y) / (b.x - a.x) => p_slope = -1 / ((b.y - a.y) / (b.x - a.x))
		float perpendicular_slope = (a.x - b.x) / (b.y - a.y);

		// expressing the distance from B to the lowest point
		// d is thickness
		// (d/2) ^ 2 = (x_point - b.x) ^ 2 + (y_point - b.y) ^ 2;
		// using the perpendicular slope deduce the y difference
		// y_point - b.y = perpedicular_slope * (x_point - b.x)
		// replace the expression
		// (d/2) ^ 2 = (x_point - b.x) ^ 2 * (perpendicular_slope * 2 + 1);
		// x_point - b.x = +- (d/2) / sqrt(perpendicular_slope * 2 + 1);
		// x_point = +- (d/2) / sqrt(perpendicular_slope * 2 + 1) + b.x;
		// if the ab_slope is positive, than x_point - b.x is positive
		// else negative
		// for the correlated point, the sign is simply reversed
		// the termen is constant for all 4 points, so it is the first calculated
		// from the ecuation of the perpendicular segment: y_point - b.y = perpendicular_slope * (x_point - b.x);
		// can be deduce the y difference;
		float x_difference = thickness / (2.0f * sqrt(perpendicular_slope * perpendicular_slope + 1.0f));
		float y_difference = x_difference * perpendicular_slope;

		// a points; first point is 0, second one is 2 since the right corner is deduced from b
		// if the ab_slope is positive, than the first one is to the left so it is smaller so it must be substracted
		// equivalent perpendicular slope is to be negative
		bool is_negative_slope = perpendicular_slope < 0.0f;
		float2 difference = { is_negative_slope ? x_difference : -x_difference, is_negative_slope ? y_difference : -y_difference };
		positions[0] = { a.x - difference.x, a.y - difference.y };
		// for the correlated point, swap the signs
		positions[2] = { a.x + difference.x, a.y + difference.y };

		// b points
		// if the ab_slope is positive, than the first one is to the left so it is smaller so it must be substracted
		// equivalent perpendicular slope is to be negative
		positions[1] = { b.x - difference.x, b.y - difference.y };
		positions[3] = { b.x + difference.x, b.y + difference.y };
	}

	// --------------------------------------------------------------------------------------------------

	uint2 LineOverlap(unsigned int first_start, unsigned int first_end, unsigned int second_start, unsigned int second_end)
	{
		unsigned int start = -1;
		// We set length to -1 such that if there is no overlap, it will return
		// { -1, -2 } which will return a length of 0 when calculated
		unsigned int length = -1;
		if (IsInRange(first_start, second_start, second_end)) {
			start = first_start;
			length = ClampMax(first_end, second_end) - start;
		}
		else if (first_start < second_start && first_end >= second_start) {
			start = second_start;
			length = ClampMax(first_end - second_start, second_end - second_start);
		}

		return { start, start + length };
	}

	// --------------------------------------------------------------------------------------------------

	uint4 RectangleOverlap(uint2 first_top_left, uint2 first_bottom_right, uint2 second_top_left, uint2 second_bottom_right, bool zero_if_not_valid)
	{
		uint2 overlap_x = LineOverlap(first_top_left.x, first_bottom_right.x, second_top_left.x, second_bottom_right.x);
		uint2 overlap_y = LineOverlap(first_top_left.y, first_bottom_right.y, second_top_left.y, second_bottom_right.y);

		unsigned int width = overlap_x.y - overlap_x.x;
		unsigned int height = overlap_y.y - overlap_y.x;

		if (zero_if_not_valid) {
			if (width == 0) {
				overlap_y.y = overlap_y.x;
			}
			else if (height == 0) {
				overlap_x.y = overlap_x.x;
			}
		}

		return { overlap_x.x, overlap_y.x, overlap_x.y, overlap_y.y };
	}

	// --------------------------------------------------------------------------------------------------

	Rectangle3D GetRectangle3D(float3 center, float3 half_width, float3 half_height)
	{
		Rectangle3D rectangle;

		rectangle.top_left = center - half_width + half_height;
		rectangle.top_right = center + half_width + half_height;
		rectangle.bottom_left = center - half_width - half_height;
		rectangle.bottom_right = center + half_width - half_height;

		return rectangle;
	}

	// --------------------------------------------------------------------------------------------------

	float3 CalculateFloat3Midpoint(Stream<float3> values)
	{
		Vec8f simd_midpoint = 0.0f;
		size_t simd_count = GetSimdCount(values.size, 2);
		for (size_t index = 0; index < simd_count; index += 2) {
			Vec8f float_values = Vec8f().load((const float*)(values.buffer + index));
			simd_midpoint += float_values;
		}

		// Use 3 values to use a simple store
		float3 scalar_midpoint_values[3];
		simd_midpoint.store((float*)scalar_midpoint_values);

		float3 midpoint = scalar_midpoint_values[0] + scalar_midpoint_values[1];

		for (size_t index = simd_count; index < values.size; index++) {
			midpoint += values[index];
		}

		return midpoint * float3::Splat(1.0f / (float)values.size);
	}

	// --------------------------------------------------------------------------------------------------

	void ApplyFloat3Addition(Stream<float3> values, float3 add_value)
	{
		size_t simd_count = GetSimdCount(values.size, 2);
		Vec8f simd_value(add_value.x, add_value.y, add_value.z, add_value.x, add_value.y, add_value.z, 0.0f, 0.0f);
		for (size_t index = 0; index < simd_count; index += 2) {
			float* float_values = (float*)(values.buffer + index);
			Vec8f current_value = Vec8f().load(float_values);
			current_value += simd_value;
			current_value.store(float_values);
		}

		for (size_t index = simd_count; index < values.size; index++) {
			values[index] += add_value;
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ApplyFloat3Subtraction(Stream<float3> values, float3 subtract_value)
	{
		ApplyFloat3Addition(values, -subtract_value);
	}

	// --------------------------------------------------------------------------------------------------

	// TODO: Remove this function? It can be replaced with GetFloat3MinMax
	void DetermineExtremePoints(Stream<float3> points, float3* values)
	{
		ECS_ASSERT(points.size < UINT_MAX);

		size_t simd_count = GetSimdCount(points.size, Vec8f::size());
		Vec8f smallest_values[3];
		Vec8f largest_values[3];
		Vec8ui smallest_values_index[3];
		Vec8ui largest_values_index[3];
		if (simd_count > 0) {
			for (size_t index = 0; index < 3; index++) {
				smallest_values[index] = FLT_MAX;
				largest_values[index] = -FLT_MAX;
			}
			Vec8ui current_indices = { 0, 1, 2, 3, 4, 5, 6, 7 };
			Vec8ui increment = Vec8ui::size();
			for (size_t index = 0; index < simd_count; index += Vec8f::size()) {
				Vec8f values[3];
				// The x values
				values[0] = GatherStride<3, 0>(points.buffer + index);
				// The y values
				values[1] = GatherStride<3, 1>(points.buffer + index);
				// The z values
				values[2] = GatherStride<3, 2>(points.buffer + index);

				for (size_t axis = 0; axis < 3; axis++) {
					Vec8fb are_smaller = values[axis] < smallest_values[axis];
					Vec8fb are_greater = values[axis] > largest_values[axis];
					
					smallest_values[axis] = select(are_smaller, values[axis], smallest_values[axis]);
					smallest_values_index[axis] = select(are_smaller, current_indices, smallest_values_index[axis]);
				
					largest_values[axis] = select(are_greater, values[axis], largest_values[axis]);
					largest_values_index[axis] = select(are_greater, current_indices, largest_values_index[axis]);
				}

				current_indices += increment;
			}
		}
		
		uint3 smallest_value_scalar_index;
		uint3 largest_value_scalar_index;
		float3 smallest_value_scalar = float3::Splat(FLT_MAX);
		float3 largest_value_scalar = float3::Splat(-FLT_MAX);
		for (size_t index = simd_count; index < points.size; index++) {
			for (size_t axis = 0; axis < 3; axis++) {
				if (points[index][axis] < smallest_value_scalar[axis]) {
					smallest_value_scalar[axis] = points[index][axis];
					smallest_value_scalar_index[axis] = index;
				}
				if (points[index][axis] > largest_value_scalar[axis]) {
					largest_value_scalar[axis] = points[index][axis];
					largest_value_scalar_index[axis] = index;
				}
			}
		}

		if (simd_count > 0) {
			// Now we need to get the values out from the SIMD registers into scalar form
			for (size_t axis = 0; axis < 3; axis++) {
				Vec8f current_min = HorizontalMin8(smallest_values[axis]);
				size_t min_index = HorizontalMin8Index(smallest_values[axis], current_min);
				float min_value = smallest_values[axis].extract(min_index);
				if (min_value < smallest_value_scalar[axis]) {
					smallest_value_scalar_index[axis] = smallest_values_index[axis].extract(min_index);
				}

				Vec8f current_max = HorizontalMax8(largest_values[axis]);
				size_t max_index = HorizontalMax8Index(largest_values[axis], current_max);
				float max_value = largest_values[axis].extract(max_index);
				if (max_value > largest_value_scalar[axis]) {
					largest_value_scalar_index[axis] = largest_values_index[axis].extract(max_index);
				}
			}
		}

		for (size_t axis = 0; axis < 3; axis++) {
			values[axis] = points[smallest_value_scalar_index[axis]];
			values[axis + 3] = points[largest_value_scalar_index[axis]];
		}
	}

	// --------------------------------------------------------------------------------------------------

	size_t WeldVertices(Stream<float3>& points, float3 epsilon) {
		// In order to speed this up for a large number of entries, we can use a spatial grid
		// And check values only inside a cell

		// We want to have a good amount of cells in each direction. Choosing 128x128x128
		// Should be a reasonable default, we can take advantage of the fact that it is
		// a sparse grid, and the memory is allocated as needed
		const uint3 DIMENSIONS = { 128, 128, 128 };

		// Determine the min and max for the points in order to have sensible cell sizes
		float3 points_min;
		float3 points_max;
		GetFloat3MinMax(points, &points_min, &points_max);
		float3 points_span = points_max - points_min;
		// We also need to take into account, that if the epsilon is really large
		// With respect to the object span, we need to reduce the cell sizes even
		// More such that the vertices don't spend a lot of time traversing many cells
		points_span /= epsilon;
		// Empirically, it seems to help to make this a little bit larger
		points_span *= float3::Splat(2.0f);

		float3 per_cell_size = points_span / float3(DIMENSIONS);
		uint3 int_per_cell_size = per_cell_size;
		uint3 int_per_cell_power_of_two = BasicTypeAction<uint3>(
			int_per_cell_size,
			[](unsigned int size) {
				return PowerOfTwoGreaterEx(size).y;
			}
		);
		
		// Determine if we need a spatial grid with reduced cell sizes
		bool has_smaller_factor = false;
		float3 smaller_factor = float3::Splat(1.0f);
		if (int_per_cell_size.x == 0) {
			smaller_factor.x = 1.0f / per_cell_size.x;
			has_smaller_factor = true;
		}
		if (int_per_cell_size.y == 0) {
			smaller_factor.y = 1.0f / per_cell_size.y;
			has_smaller_factor = true;
		}
		if (int_per_cell_size.z == 0) {
			smaller_factor.z = 1.0f / per_cell_size.z;
			has_smaller_factor = true;
		}

		const size_t CHUNK_POINT_COUNT = 8;
		typedef float3 ChunkDataEntry;
		struct ChunkData {
			ECS_INLINE void Set(ChunkDataEntry entry, unsigned int count) {
				points[count] = entry;
			}

			float3 points[CHUNK_POINT_COUNT];
		};

		// Use an auto lambda to receive the spatial grid as parameter in order to have
		// The case with smaller factor compile time determined
		auto perform_welding = [&](auto& spatial_grid) {
			for (size_t index = 0; index < points.size; index++) {
				float3 current_point = points[index];
				bool was_welded = spatial_grid.InsertAABBLate(current_point - epsilon, current_point + epsilon, current_point, [&](
					uint3 cell_indices,
					SpatialGridChunk<ChunkData>* initial_chunk
				) {
					bool was_welded = spatial_grid.IterateChunks<true>(initial_chunk, [&](const ChunkData* data, unsigned int count) {
						for (unsigned int index = 0; index < count; index++) {
							float3 absolute_difference = BasicTypeAbsoluteDifference(current_point, data->points[index]);
							bool should_weld = BasicTypeLessEqual(absolute_difference, epsilon);
							if (should_weld) {
								return true;
							}
						}
						return false;
					});
					if (was_welded) {
						return true;
					}
					return false;
				});
				if (was_welded) {
					points.RemoveSwapBack(index);
					index--;
				}
			}
		};

		const size_t DECK_POWER_OF_TWO = 8; // 256
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 64);

		if (has_smaller_factor) {
			SpatialGrid<ChunkData, ChunkDataEntry, CHUNK_POINT_COUNT, SpatialGridDefaultCellIndicesHash, true> spatial_grid;
			// Choose some sensible defaults
			spatial_grid.Initialize(&stack_allocator, DIMENSIONS, int_per_cell_size, DECK_POWER_OF_TWO);
			spatial_grid.SetSmallerCellSizeFactor(smaller_factor);
			perform_welding(spatial_grid);
		}
		else {
			SpatialGrid<ChunkData, ChunkDataEntry, CHUNK_POINT_COUNT> spatial_grid;
			// Choose some sensible defaults
			spatial_grid.Initialize(&stack_allocator, DIMENSIONS, int_per_cell_size, DECK_POWER_OF_TWO);
			perform_welding(spatial_grid);
		}

		return points.size;
	}

	// --------------------------------------------------------------------------------------------------

	float3 TriangleNormal(float3 a, float3 b, float3 c)
	{
		return Cross(b - a, c - a);
	}

	// --------------------------------------------------------------------------------------------------

	float3 TriangleNormal(float3 a, float3 b, float3 c, float3 look_point)
	{
		// Calculate the normal as usual and if the dot product between the normal
		// And the direction vector of the look point with one of the triangle corners
		// Is negative, we need to flip it
		float3 normal = TriangleNormal(a, b, c);
		if (Dot(normal, look_point - c) < 0.0f) {
			return -normal;
		}
		return normal;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsTriangleFacing(float3 a, float3 b, float3 c, float3 d)
	{
		float3 normal = TriangleNormal(a, b, c);
		return Dot(normal, d - c) >= 0.0f;
	}

	// --------------------------------------------------------------------------------------------------

	float TriangleArea(float3 point_a, float3 point_b, float3 point_c)
	{
		// length(a X b) / 2
		return Length(Cross(point_a, point_b)) / 2;
	}

	// --------------------------------------------------------------------------------------------------

	float TetrahedronVolume(float3 point_a, float3 point_b, float3 point_c, float3 point_d) {
		// (a X b * c) / 6 
		// a, b, c are three co-terminal edges - they start from the same point
		// The formula is the cross product between 2 of these vectors, dotted with the third
		// One and divided by 6
		float3 a = point_b - point_a;
		float3 b = point_c - point_a;
		float3 c = point_d - point_a;
		return Dot(Cross(a, b), c) / 6;
	}

	// --------------------------------------------------------------------------------------------------

	template<bool is_min, bool is_max, int offset>
	static float2 GetFloat3MinMaxSingle(Stream<float3> values, ulong2* indices) {
		// We are branching here in case the index is not needed
		// Since we can use the min/max function directly instead
		// Of 2 selects to have the index available
		Vec8f min_values = FLT_MAX;
		Vec8f max_values = -FLT_MAX;
		if (indices == nullptr) {
			ApplySIMDConstexpr(values.size, Vec8f::size(), [&](auto is_full_iteration, size_t index, size_t count) {
				Vec8f current_min_values;
				Vec8f current_max_values;
				if constexpr (is_full_iteration) {
					Vec8f current_values = GatherStride<3, offset>(values.buffer + index);
					current_min_values = current_values;
					current_max_values = current_values;
				}
				else {
					Vec8f current_values = GatherStride<3, offset>(values.buffer + index, count);
					// Here we might need to have min and max at the same time
					// And need to select the last elements accordingly
					Vec8fb gather_mask = CastToFloat(SelectMaskLast<unsigned int>(Vec8f::size() - count));
					if constexpr (is_min) {
						current_min_values = select(gather_mask, FLT_MAX, current_values);
					}
					if constexpr (is_max) {
						current_max_values = select(gather_mask, -FLT_MAX, current_values);
					}
				}

				if constexpr (is_min) {
					min_values = min(current_min_values, min_values);
				}
				if constexpr (is_max) {
					max_values = max(current_max_values, max_values);
				}
			});
			float2 values;
			if constexpr (is_min) {
				values.x = VectorLow(HorizontalMin8(min_values));
			}
			if constexpr (is_max) {
				values.y = VectorLow(HorizontalMax8(min_values));
			}
			return values;
		}
		else {
			Vec8ui current_indices = { 0, 1, 2, 3, 4, 5, 6, 7 };
			Vec8ui increment = Vec8f::size();
			Vec8ui min_indices;
			Vec8ui max_indices;
			ApplySIMDConstexpr(values.size, Vec8f::size(), [&](auto is_full_iteration, size_t index, size_t count) {
				Vec8f current_min_values;
				Vec8f current_max_values;
				if constexpr (is_full_iteration) {
					Vec8f current_values = GatherStride<3, offset>(values.buffer + index);
					current_min_values = current_values;
					current_max_values = current_values;
				}
				else {
					Vec8f current_values = GatherStrideMasked<3, offset>(values.buffer + index, count, is_min ? FLT_MAX : -FLT_MAX);
					Vec8fb mask = CastToFloat(SelectMaskLast<unsigned int>(Vec8f::size() - count));
					if constexpr (is_min) {
						current_min_values = select(mask, FLT_MAX, current_values);
					}
					if constexpr (is_max) {
						current_max_values = select(mask, -FLT_MAX, current_values);
					}
				}

				if constexpr (is_min) {
					Vec8fb min_mask = current_min_values < min_values;
					min_values = select(min_mask, current_min_values, min_values);
					min_indices = select(min_mask, current_indices, min_indices);
				}
				if constexpr (is_max) {
					Vec8fb max_mask = current_max_values > max_values;
					max_values = select(max_mask, current_max_values, max_values);
					max_indices = select(max_mask, current_indices, max_indices);
				}
				current_indices += increment;
			});

			float2 values;
			if constexpr (is_min) {
				Vec8f splatted_min = HorizontalMin8(min_values);
				size_t simd_smallest_index = HorizontalMin8Index(min_values, splatted_min);
				indices->x = min_indices[simd_smallest_index];
				values.x = VectorLow(splatted_min);
			}
			if constexpr (is_max) {
				Vec8f splatted_max = HorizontalMax8(max_values);
				size_t simd_smallest_index = HorizontalMin8Index(max_values, splatted_max);
				indices->y = max_indices[simd_smallest_index];
				values.y = VectorLow(splatted_max);
			}
			return values;
		}
	}

	template<bool is_min, int offset>
	static float GetFloat3MinMaxSingle(Stream<float3> values, size_t* index) {
		ulong2 indices;
		float2 value = GetFloat3MinMaxSingle<is_min ? true : false, is_min ? false : true, offset>(values, index != nullptr ? &indices : nullptr);
		if (index != nullptr) {
			*index = is_min ? indices.x : indices.y;
		}
		return is_min ? value.x : value.y;
	}

	float GetFloat3MinX(Stream<float3> values, size_t* index) {
		return GetFloat3MinMaxSingle<true, 0>(values, index);
	}

	float GetFloat3MinY(Stream<float3> values, size_t* index) {
		return GetFloat3MinMaxSingle<true, 1>(values, index);
	}

	float GetFloat3MinZ(Stream<float3> values, size_t* index) {
		return GetFloat3MinMaxSingle<true, 2>(values, index);
	}

	float GetFloat3MaxX(Stream<float3> values, size_t* index) {
		return GetFloat3MinMaxSingle<false, 0>(values, index);
	}

	float GetFloat3MaxY(Stream<float3> values, size_t* index) {
		return GetFloat3MinMaxSingle<false, 1>(values, index);
	}

	float GetFloat3MaxZ(Stream<float3> values, size_t* index) {
		return GetFloat3MinMaxSingle<false, 2>(values, index);
	}

	// --------------------------------------------------------------------------------------------------

	float2 GetFloat3MinMaxX(Stream<float3> values, ulong2* indices) {
		return GetFloat3MinMaxSingle<true, true, 0>(values, indices);
	}

	float2 GetFloat3MinMaxY(Stream<float3> values, ulong2* indices) {
		return GetFloat3MinMaxSingle<true, true, 1>(values, indices);
	}

	float2 GetFloat3MinMaxZ(Stream<float3> values, ulong2* indices) {
		return GetFloat3MinMaxSingle<true, true, 2>(values, indices);
	}

	// --------------------------------------------------------------------------------------------------

	template<bool is_min, bool is_max>
	static void GetFloat3MinMaxImpl(Stream<float3> values, float3* output_min_values, float3* output_max_values, ulong3* min_indices, ulong3* max_indices) {
		// Read 2 float3's at a time and perform the operation there
		Vec8f min_values = FLT_MAX;
		Vec8f max_values = -FLT_MAX;
		const size_t simd_increment = Vec8f::size() / float3::Count();
		ECS_ASSERT(IsPowerOfTwo(simd_increment));
		size_t simd_count = GetSimdCount(values.size, simd_increment);
		float3 scalar_min = float3::Splat(FLT_MAX);
		float3 scalar_max = float3::Splat(-FLT_MAX);
		if (min_indices == nullptr && max_indices == nullptr) {
			// If we don't need the indices, we can speed up the function by a bit
			// By ommiting that value
			if (simd_count > 0) {
				for (size_t index = 0; index < simd_count; index += simd_increment) {
					Vec8f current_values = Vec8f().load((const float*)(values.buffer + index));
					if constexpr (is_min) {
						min_values = min(current_values, min_values);
					}
					if constexpr (is_max) {
						max_values = max(current_values, max_values);
					}
				}

				float3 simd_min_values[simd_increment + 1];
				float3 simd_max_values[simd_increment + 1];
				if constexpr (is_min) {
					min_values.store((float*)simd_min_values);
					for (size_t index = 0; index < simd_increment; index++) {
						scalar_min = BasicTypeMin(scalar_min, simd_min_values[index]);
					}
				}
				if constexpr (is_max) {
					max_values.store((float*)simd_max_values);
					for (size_t index = 0; index < simd_increment; index++) {
						scalar_max = BasicTypeMax(scalar_max, simd_max_values[index]);
					}
				}
			}

			for (size_t index = simd_count; index < values.size; index++) {
				if constexpr (is_min) {
					scalar_min = BasicTypeMin(scalar_min, values[index]);
				}
				if constexpr (is_max) {
					scalar_max = BasicTypeMax(scalar_max, values[index]);
				}
			}
			if constexpr (is_min) {
				*output_min_values = scalar_min;
			}
			if constexpr (is_max) {
				*output_max_values = scalar_max;
			}
		}
		else {
			ulong3 scalar_min_indices = ulong3::Splat(-1);
			ulong3 scalar_max_indices = ulong3::Splat(-1);
			if (simd_count > 0) {
				Vec8ui simd_vector_increment = simd_increment;
				Vec8ui indices = { 0, 0, 0, 1, 1, 1, (unsigned int)-1, (unsigned int)-1 };
				Vec8ui min_indices = indices;
				Vec8ui max_indices = indices;
				for (size_t index = 0; index < simd_count; index += simd_increment) {
					Vec8f current_values = Vec8f().load((const float*)(values.buffer + index));
					if constexpr (is_min) {
						Vec8fb mask = current_values < min_values;
						min_values = select(mask, current_values, min_values);
						min_indices = select(mask, indices, min_indices);
					}
					if constexpr (is_max) {
						Vec8fb mask = current_values > max_values;
						max_values = select(mask, current_values, max_values);
						max_indices = select(mask, indices, max_indices);
					}
					indices += simd_vector_increment;
				}

				float3 simd_min_values[simd_increment + 1];
				float3 simd_max_values[simd_increment + 1];
				uint3 simd_min_indices[simd_increment + 1];
				uint3 simd_max_indices[simd_increment + 1];
				if constexpr (is_min) {
					min_values.store((float*)simd_min_values);
					min_indices.store(simd_min_indices);
					for (size_t index = 0; index < simd_increment; index++) {
						for (size_t subindex = 0; subindex < float3::Count(); subindex++) {
							bool mask = simd_min_values[index][subindex] < scalar_min[subindex];
							scalar_min[subindex] = mask ? simd_min_values[index][subindex] : scalar_min[subindex];
							scalar_min_indices[subindex] = mask ? simd_min_indices[index][subindex] : scalar_min_indices[subindex];
						}
					}
				}
				if constexpr (is_min) {
					max_values.store((float*)simd_max_values);
					max_indices.store(simd_max_indices);
					for (size_t index = 0; index < simd_increment; index++) {
						for (size_t subindex = 0; subindex < float3::Count(); subindex++) {
							bool mask = simd_max_values[index][subindex] > scalar_max[subindex];
							scalar_max[subindex] = mask ? simd_max_values[index][subindex] : scalar_max[subindex];
							scalar_max_indices[subindex] = mask ? simd_max_indices[index][subindex] : scalar_max_indices[subindex];
						}
					}
				}
			}

			for (size_t index = simd_count; index < values.size; index++) {
				for (size_t subindex = 0; subindex < float3::Count(); subindex++) {
					if constexpr (is_min) {
						bool mask = values[index][subindex] < scalar_min[subindex];
						scalar_min[subindex] = mask ? values[index][subindex] : scalar_min[subindex];
						scalar_min_indices[subindex] = mask ? values[index][subindex] : scalar_min_indices[subindex];
					}
					if constexpr (is_max) {
						bool mask = values[index][subindex] > scalar_max[subindex];
						scalar_max[subindex] = mask ? values[index][subindex] : scalar_max[subindex];
						scalar_max_indices[subindex] = mask ? values[index][subindex] : scalar_max_indices[subindex];
					}
				}
			}

			if constexpr (is_min) {
				*output_min_values = scalar_min;
				if (min_indices != nullptr) {
					*min_indices = scalar_min_indices;
				}
			}
			if constexpr (is_max) {
				*output_max_values = scalar_max;
				if (max_indices != nullptr) {
					*max_indices = scalar_max_indices;
				}
			}
		}
	}

	float3 GetFloat3Min(Stream<float3> values, ulong3* indices) {
		float3 min;
		GetFloat3MinMaxImpl<true, false>(values, &min, nullptr, indices, nullptr);
		return min;
	}

	float3 GetFloat3Max(Stream<float3> values, ulong3* indices) {
		float3 max;
		GetFloat3MinMaxImpl<false, true>(values, nullptr, &max, nullptr, indices);
		return max;
	}

	void GetFloat3MinMax(Stream<float3> values, float3* min, float3* max, ulong3* min_indices, ulong3* max_indices)
	{
		GetFloat3MinMaxImpl<true, true>(values, min, max, min_indices, max_indices);
	}

	// --------------------------------------------------------------------------------------------------

	template<int offset>
	static unsigned int* CountSortFloat3Impl(Stream<float3> values, float3* output_values, size_t bucket_count, AllocatorPolymorphic allocator) {
		float2 min_max_axis = GetFloat3MinMaxSingle<true, true, offset>(values, nullptr);
		float difference = min_max_axis.y - min_max_axis.x;
		float bucket_size = difference / bucket_count;
		float bucket_size_inverse = 1.0f / bucket_size;

		return CountingSortExtended(values.buffer, values.size, output_values, bucket_count, allocator, [bucket_size_inverse, min_max_axis](float3 value) {
			float current_value = value[offset];
			return (size_t)((current_value - min_max_axis.x) * bucket_size_inverse);
		});
	}

	unsigned int* CountSortFloat3X(Stream<float3> values, float3* output_values, size_t bucket_count, AllocatorPolymorphic allocator)
	{
		return CountSortFloat3Impl<0>(values, output_values, bucket_count, allocator);
	}

	unsigned int* CountSortFloat3Y(Stream<float3> values, float3* output_values, size_t bucket_count, AllocatorPolymorphic allocator)
	{
		return CountSortFloat3Impl<1>(values, output_values, bucket_count, allocator);
	}

	unsigned int* CountSortFloat3Z(Stream<float3> values, float3* output_values, size_t bucket_count, AllocatorPolymorphic allocator)
	{
		return CountSortFloat3Impl<2>(values, output_values, bucket_count, allocator);
	}

	// --------------------------------------------------------------------------------------------------

}