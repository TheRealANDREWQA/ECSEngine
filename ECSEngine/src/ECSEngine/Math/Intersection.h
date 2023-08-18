#pragma once
#include "Vector.h"
#include "Plane.h"

namespace ECSEngine {

	namespace SIMDHelpers {

		// Can be used for line tests, ray tests (semi-lines) or segment tests
		// Returns a mask that can be used to perform other operations
		template<typename ResultFunctor>
		ECS_INLINE Vector8 ECS_VECTORCALL IntersectLinePlane(
			Vector8 line_origin,
			Vector8 line_direction,
			Plane plane,
			Vector8& output,
			ResultFunctor&& result_functor
		) {
			// t = (plane.dot - Dot3(plane.normal, segment_a)) / Dot3(plane.normal, segment_direction)
			// The t_factor is stored in the 4th component
			Vector8 t_simd = (plane.normal_dot - Dot(plane.normal_dot, line_origin)) / Dot(plane.normal_dot, line_direction);

			Vector8 mask = result_functor(t_simd);
			// Calculating the final result is not expensive - 5 cycles
			output = Fmadd(PerLaneBroadcast<3>(t_simd), line_direction, line_origin.value);
			output = PerLaneBlend<0, 1, 2, 7>(output, t_simd);
			return mask;
		}

		//// Direction needs to be normalized beforehand. The intersection_point will contain in the first 3 components 
		//// the position of the point and in the 4th component the factor interpolation factor t that retrieves the point
		//// along the ray. Returns true if there is a valid intersection point, else false
		//template<typename ReturnType, typename VectorType, typename ResultFunctor>
		//ECS_INLINE ReturnType ECS_VECTORCALL IntersectRaySphere(
		//	VectorType point, 
		//	VectorType direction, 
		//	VectorType sphere_center, 
		//	float radius,
		//	VectorType intersection_point,
		//	ResultFunctor&& result_functor
		//) {
		//	// We need to solve the quadratic equation
		//	// t^2 + 2bt + c = 0
		//	// Where b = Dot3(displacement, direction)
		//	// and c = Dot3(displacement, displacement) - r^2
		//	// We also need to take into consideration some edge cases

		//	VectorType displacement = point - sphere_center;
		//	float b = Dot3(displacement, displacement).First();
		//}

	}

	// It returns true if the segment intersects the plane, else false.
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from segment_a
	// to the actual point
	ECS_INLINE Vector8 ECS_VECTORCALL IntersectSegmentPlaneMask(Vector8 segment_a, Vector8 segment_b, Plane plane, Vector8& output) {
		return SIMDHelpers::IntersectLinePlane(
			segment_a, segment_b - segment_a, plane, output, [](Vector8 t_simd) {
				return IsInRangeMask<true, true>(t_simd, ZeroVector(), VectorGlobals::ONE);
			}
		);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(
		IntersectSegmentPlane,
		FORWARD(Vector8 segment_a, Vector8 segment_b, Plane plane, Vector8& output),
		FORWARD(segment_a, segment_b, plane, output)
	);

	// It returns true if the ray intersects the plane, else false.
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from ray_origin
	// to the actual point. Don't access a lane that is not intersected - the values are undefined
	ECS_INLINE Vector8 ECS_VECTORCALL IntersectRayPlaneMask(Vector8 ray_origin, Vector8 ray_direction, Plane plane, Vector8& output) {
		return SIMDHelpers::IntersectLinePlane(
			ray_origin, ray_direction, plane, output, [](Vector8 t_simd) {
				return t_simd >= ZeroVector();
			}
		);
	}
	
	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(
		IntersectRayPlane,
		FORWARD(Vector8 ray_origin, Vector8 ray_direction, Plane plane, Vector8& output),
		FORWARD(ray_origin, ray_direction, plane, output)
	);

#define INTERSECT_LINE_PLANE_THRESHOLD 1000000.0f

	// Infinite line - as if testing two rays from the same location but with opposing directions
	// It returns true if the line intersects the plane, else false (they are parallel).
	// In case an intersection was detected, the output will have in the first 3 components
	// the points position and in the 4th component the interpolation factor needed to go from ray_origin
	// to the actual point. Don't access a lane that is not intersected - the values are undefined
	ECS_INLINE Vector8 ECS_VECTORCALL IntersectLinePlaneMask(Vector8 line_direction, Plane plane, Vector8& output) {
		// The origin doesn't matter here (mostly)
		return SIMDHelpers::IntersectLinePlane(ZeroVector(), line_direction, plane, output, [](Vector8 t_simd) {
			// When the line and the plane are parallel, the t value will be very large because of the division by 0
			return abs(t_simd) < Vector8(INTERSECT_LINE_PLANE_THRESHOLD);
		});
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(
		IntersectLinePlane,
		FORWARD(Vector8 line_direction, Plane plane, Vector8& output),
		FORWARD(line_direction, plane, output)
	);

}