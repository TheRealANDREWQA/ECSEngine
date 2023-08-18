#pragma once
#include "Vector.h"

namespace ECSEngine {

	struct PlaneScalar {
		ECS_INLINE PlaneScalar() {}
		ECS_INLINE PlaneScalar(float3 _normal, float _dot) : normal(_normal), dot(_dot) {}

		float3 normal;
		float dot;
	};

	struct Plane {
		typedef Vector8 VectorType;

		ECS_INLINE Plane() {}
		ECS_INLINE Plane(float3 axis_direction, float3 point) {
			Vector8 axis_direction_vector = axis_direction;
			Vector8 point_vector = point;
			axis_direction_vector = NormalizeIfNot(axis_direction_vector);
			Vector8 dot = ECSEngine::Dot(axis_direction_vector, point_vector);
			normal_dot = PerLaneBlend<0, 1, 2, 7>(axis_direction_vector, dot);
		}
		ECS_INLINE Plane(float3 axis_direction0, float3 point0, float3 axis_direction1, float3 point1) {
			Vector8 axis_direction_vector = { axis_direction0, axis_direction1 };
			Vector8 point_vector = { point0, point1 };
			axis_direction_vector = NormalizeIfNot(axis_direction_vector);
			Vector8 dot = ECSEngine::Dot(axis_direction_vector, point_vector);
			normal_dot = PerLaneBlend<0, 1, 2, 7>(axis_direction_vector, dot);
		}
		ECS_INLINE Plane(Vector8 _value) : normal_dot(_value) {}

		ECS_INLINE Vec4f ECS_VECTORCALL Low() const {
			return normal_dot.Low();
		}

		ECS_INLINE Vec4f ECS_VECTORCALL High() const {
			return normal_dot.High();
		}

		ECS_INLINE void Normalize() {
			normal_dot = ECSEngine::Normalize(normal_dot);
		}

		// There must be 2 entries in the given pointer
		ECS_INLINE void Normal(float3* values) const {
			normal_dot.StoreFloat3(values);
		}

		ECS_INLINE void Normal(float3* low, float3* high) const {
			normal_dot.StoreFloat3(low, high);
		}

		ECS_INLINE float2 Dot() const {
			return normal_dot.GetLasts();
		}

		Vector8 normal_dot;
	};

	// Given 3 non-collinear points A, B and C (ordered counter clockwise), 
	// calculates the plane determined by them
	ECS_INLINE Plane ECS_VECTORCALL ComputePlane(Vector8 a, Vector8 b, Vector8 c) {
		Vector8 normal = Normalize(Cross(b - a, c - a));
		Vector8 dot = Dot(normal, a);
		return PerLaneBlend<0, 1, 2, 7>(normal, dot);
	}

	ECS_INLINE Plane ECS_VECTORCALL PlaneXY(float z_offset = 0.0f, bool invert_normal = false) {
		Vector8 normal = ForwardVector();
		normal = Select(Vector8(invert_normal ? INT_MAX : 0), normal, -normal);
		Vector8 offset = z_offset;
		return PerLaneBlend<0, 1, 2, 7>(normal, offset);
	}

	ECS_INLINE Plane ECS_VECTORCALL PlaneXZ(float y_offset = 0.0f, bool invert_normal = false) {
		Vector8 normal = UpVector();
		normal = Select(Vector8(invert_normal ? INT_MAX : 0), normal, -normal);
		Vector8 offset = y_offset;
		return PerLaneBlend<0, 1, 2, 7>(normal, offset);
	}

	ECS_INLINE Plane ECS_VECTORCALL PlaneYZ(float x_offset = 0.0f, bool invert_normal = false) {
		Vector8 normal = RightVector();
		normal = Select(Vector8(invert_normal ? INT_MAX : 0), normal, -normal);
		Vector8 offset = x_offset;
		return PerLaneBlend<0, 1, 2, 7>(normal, offset);
	}

	// Returns true if the given direction is parallel to the plane
	// Both the direction and the plane normal must be normalized beforehand
	ECS_INLINE Vector8 ECS_VECTORCALL PlaneIsParallelMask(Plane plane, Vector8 direction_normalized, Vector8 epsilon = VectorGlobals::EPSILON) {
		return IsPerpendicularMask(plane.normal_dot, direction_normalized, epsilon);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(
		PlaneIsParallel,
		FORWARD(Plane plane, Vector8 direction_normalized, Vector8 epsilon = VectorGlobals::EPSILON),
		FORWARD(plane, direction_normalized, epsilon)
	);

	ECS_INLINE Vector8 ECS_VECTORCALL PlaneIsParallelAngleMask(Plane plane, Vector8 direction_normalized, Vector8 radians) {
		return IsPerpendicularAngleMask(plane.normal_dot, direction_normalized, radians);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(
		PlaneIsParallelAngle,
		FORWARD(Plane plane, Vector8 direction_normalized, Vector8 radians),
		FORWARD(plane, direction_normalized, radians)
	);

	// Returns true if the point is located on the same side as the normal of the plane
	// The value is splatted across all register values per lane
	ECS_INLINE Vector8 ECS_VECTORCALL IsAbovePlaneMask(Plane plane, Vector8 point) {
		// Get a point on the plane, calculate the direction to the original point
		// If the cosine of the normal and the direction is positive, then the point
		// is above. If the value is 0, then it's on the plane and negative means bellow the plane
		Vector8 plane_point = plane.normal_dot * PerLaneBroadcast<3>(plane.normal_dot);
		Vector8 direction = point - plane_point;
		Vector8 normalized_direction = Normalize(direction);
		Vector8 cosine = Dot(plane.normal_dot, normalized_direction);
		Vector8 mask = cosine > ZeroVector();
		mask = PerLaneBroadcast<0>(mask);
		return mask;
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK(
		IsAbovePlane,
		FORWARD(Plane plane, Vector8 point),
		FORWARD(plane, point)
	);


}