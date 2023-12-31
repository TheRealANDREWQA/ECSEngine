#pragma once
#include "Vector.h"
#include "Quaternion.h"

namespace ECSEngine {

	struct ECSENGINE_API PlaneScalar {
		ECS_INLINE PlaneScalar() {}
		ECS_INLINE PlaneScalar(float3 _normal, float _dot) : normal(_normal), dot(_dot) {}
		// It will perform a normalization, use FromNormalized() if you know the direction is normalized
		PlaneScalar(float3 axis_direction, float3 point);
		// This exists to match the SIMD version
		ECS_INLINE PlaneScalar(float3 axis_direction, float3 point, size_t vector_count) : PlaneScalar(axis_direction, point) {}

		static PlaneScalar FromNormalized(float3 axis_direction_normalized, float3 point);

		void Normalize();

		ECS_INLINE operator float4() const {
			return { normal, dot };
		}

		float3 normal;
		float dot;
	};

	struct ECSENGINE_API Plane {
		ECS_INLINE Plane() {}
		ECS_INLINE Plane(Vector3 _normal, Vec8f _dot) {
			normal = _normal;
			dot = _dot;
		}
		// It will perform a normalization, use FromNormalized() if you know the direction is normalized
		Plane(Vector3 axis_direction, Vector3 point, size_t vector_count);

		static Plane FromNormalized(Vector3 axis_direction_normalized, Vector3 point);

		void Normalize();

		void Set(PlaneScalar plane, size_t index);

		ECS_INLINE operator Vector4() const {
			return { normal, dot };
		}

		Vector3 normal;
		Vec8f dot;
	};

	// Given 3 non-collinear points A, B and C (ordered counter clockwise), 
	// calculates the plane determined by them
	ECSENGINE_API PlaneScalar ComputePlane(float3 a, float3 b, float3 c);

	// Given 3 non-collinear points A, B and C (ordered counter clockwise), 
	// calculates the plane determined by them
	ECSENGINE_API Plane ECS_VECTORCALL ComputePlane(Vector3 a, Vector3 b, Vector3 c);

	ECSENGINE_API PlaneScalar PlaneXYScalar(float z_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXY(Vec8f z_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API PlaneScalar PlaneXZScalar(float y_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXZ(Vec8f y_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API PlaneScalar PlaneYZScalar(float x_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneYZ(Vec8f x_offset = 0.0f, bool invert_normal = false);

	ECSENGINE_API PlaneScalar PlaneXYRotated(QuaternionScalar rotation, float3 plane_point, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXYRotated(Quaternion rotation, Vector3 plane_point, bool invert_normal = false);

	ECSENGINE_API PlaneScalar PlaneXZRotated(QuaternionScalar rotation, float3 plane_point, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneXZRotated(Quaternion rotation, Vector3 plane_point, bool invert_normal = false);

	ECSENGINE_API PlaneScalar PlaneYZRotated(QuaternionScalar rotation, float3 plane_point, bool invert_normal = false);

	ECSENGINE_API Plane ECS_VECTORCALL PlaneYZRotated(Quaternion rotation, Vector3 plane_point, bool invert_normal = false);

	// Returns true if the given direction is parallel to the plane
	// Both the direction and the plane normal must be normalized beforehand
	ECS_INLINE bool PlaneIsParallelMask(PlaneScalar plane, float3 direction_normalized, float epsilon = ECS_SIMD_VECTOR_EPSILON_VALUE) {
		return IsPerpendicularMask(plane.normal, direction_normalized, epsilon);
	}

	// Returns true if the given direction is parallel to the plane
	// Both the direction and the plane normal must be normalized beforehand
	ECS_INLINE SIMDVectorMask ECS_VECTORCALL PlaneIsParallelMask(Plane plane, Vector3 direction_normalized, Vec8f epsilon = VectorGlobals::EPSILON) {
		return IsPerpendicularMask(plane.normal, direction_normalized, epsilon);
	}

	// Returns true if the given direction is parallel to the plane
	// Both the direction and the plane normal must be normalized beforehand
	ECS_INLINE bool PlaneIsParallelAngleMask(PlaneScalar plane, float3 direction_normalized, float radians) {
		return IsPerpendicularAngleMask(plane.normal, direction_normalized, radians);
	}

	// Returns true if the given direction is parallel to the plane
	// Both the direction and the plane normal must be normalized beforehand
	ECS_INLINE SIMDVectorMask ECS_VECTORCALL PlaneIsParallelAngleMask(Plane plane, Vector3 direction_normalized, Vec8f radians) {
		return IsPerpendicularAngleMask(plane.normal, direction_normalized, radians);
	}

	// Returns true if the point is located on the same side as the normal of the plane
	ECSENGINE_API bool IsAbovePlaneMask(PlaneScalar plane, float3 point);

	// Returns true if the point is located on the same side as the normal of the plane
	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL IsAbovePlaneMask(Plane plane, Vector3 point);

}