#include "ecspch.h"
#include "MathHelpers.h"
#include "../Utilities/Utilities.h"

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

}