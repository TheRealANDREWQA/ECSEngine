#pragma once
#include "Vector.h"
#include "Quaternion.h"

namespace ECSEngine {

	struct PlaneScalar {
		ECS_INLINE PlaneScalar() {}
		ECS_INLINE PlaneScalar(float3 _normal, float _dot) : normal(_normal), dot(_dot) {}

		float3 normal;
		float dot;
	};

	struct ECSENGINE_API Plane {
		typedef Vector8 VectorType;

		ECS_INLINE Plane() {}
		Plane(Vector8 axis_direction, Vector8 point);

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
	ECSENGINE_API Plane ECS_VECTORCALL ComputePlane(Vector8 a, Vector8 b, Vector8 c);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneFromNormalizedAxis(Vector8 axis_direction_normalized, Vector8 offset, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXY(float z_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXZ(float y_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneYZ(float x_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXYRotated(Quaternion rotation, Vector8 plane_offset, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXZRotated(Quaternion rotation, Vector8 plane_offset, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneYZRotated(Quaternion rotation, Vector8 plane_offset, bool invert_normal = false);

	// Returns true if the given direction is parallel to the plane
	// Both the direction and the plane normal must be normalized beforehand
	ECS_INLINE Vector8 ECS_VECTORCALL PlaneIsParallelMask(Plane plane, Vector8 direction_normalized, Vector8 epsilon = VectorGlobals::EPSILON) {
		return IsPerpendicularMask(plane.normal_dot, direction_normalized, epsilon);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		PlaneIsParallel,
		3,
		FORWARD(Plane plane, Vector8 direction_normalized, Vector8 epsilon = VectorGlobals::EPSILON),
		FORWARD(plane, direction_normalized, epsilon)
	);

	ECS_INLINE Vector8 ECS_VECTORCALL PlaneIsParallelAngleMask(Plane plane, Vector8 direction_normalized, Vector8 radians) {
		return IsPerpendicularAngleMask(plane.normal_dot, direction_normalized, radians);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		PlaneIsParallelAngle,
		3,
		FORWARD(Plane plane, Vector8 direction_normalized, Vector8 radians),
		FORWARD(plane, direction_normalized, radians)
	);

	// Returns true if the point is located on the same side as the normal of the plane
	// The value is splatted across all register values per lane
	ECSENGINE_API Vector8 ECS_VECTORCALL IsAbovePlaneMask(Plane plane, Vector8 point);

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		IsAbovePlane,
		4,
		FORWARD(Plane plane, Vector8 point),
		FORWARD(plane, point)
	);


}