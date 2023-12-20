#include "ecspch.h"
#include "Plane.h"

namespace ECSEngine {
	
	Plane::Plane(Vector8 axis_direction, Vector8 point) {
		axis_direction = NormalizeIfNot(axis_direction);
		Vector8 dot = ECSEngine::Dot(axis_direction, point);
		normal_dot = PerLaneBlend<0, 1, 2, 7>(axis_direction, dot);
	}

	Plane ECS_VECTORCALL ComputePlane(Vector8 a, Vector8 b, Vector8 c) {
		Vector8 normal = Normalize(Cross(b - a, c - a));
		Vector8 dot = Dot(normal, a);
		return PerLaneBlend<0, 1, 2, 7>(normal, dot);
	}

	Plane ECS_VECTORCALL PlaneFromNormalizedAxis(Vector8 axis_direction_normalized, Vector8 offset, bool invert_normal) {
		Vector8 dot = Dot(axis_direction_normalized, offset);
		Vector8 axis = Select(invert_normal ? TrueMaskVector() : ZeroVector(), -axis_direction_normalized, axis_direction_normalized);
		return PerLaneBlend<0, 1, 2, 7>(axis, dot);
	}

	namespace SIMDHelpers {

		ECS_INLINE Plane ECS_VECTORCALL PlaneFromAxis(Vector8 normal, float offset, bool invert_normal) {
			normal = Select(invert_normal ? TrueMaskVector() : ZeroVector(), -normal, normal);
			Vector8 vector_offset = offset;
			return PerLaneBlend<0, 1, 2, 7>(normal, vector_offset);
		}

		// The normal is already normalized
		ECS_INLINE Plane ECS_VECTORCALL PlaneFromAxisRotated(Vector8 normal_normalized, Quaternion rotation, Vector8 offset, bool invert_normal) {
			normal_normalized = RotateVectorQuaternionSIMD(normal_normalized, rotation);
			return PlaneFromNormalizedAxis(normal_normalized, offset, invert_normal);
		}

	}

	Plane ECS_VECTORCALL PlaneXY(float z_offset, bool invert_normal) {
		Vector8 normal = ForwardVector();
		return SIMDHelpers::PlaneFromAxis(normal, z_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneXZ(float y_offset, bool invert_normal) {
		Vector8 normal = UpVector();
		return SIMDHelpers::PlaneFromAxis(normal, y_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneYZ(float x_offset, bool invert_normal) {
		Vector8 normal = RightVector();
		return SIMDHelpers::PlaneFromAxis(normal, x_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneXYRotated(Quaternion rotation, Vector8 plane_offset, bool invert_normal) {
		Vector8 normal = ForwardVector();
		return SIMDHelpers::PlaneFromAxisRotated(normal, rotation, plane_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneXZRotated(Quaternion rotation, Vector8 plane_offset, bool invert_normal) {
		Vector8 normal = UpVector();
		return SIMDHelpers::PlaneFromAxisRotated(normal, rotation, plane_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneYZRotated(Quaternion rotation, Vector8 plane_offset, bool invert_normal) {
		Vector8 normal = RightVector();
		return SIMDHelpers::PlaneFromAxisRotated(normal, rotation, plane_offset, invert_normal);
	}

	Vector8 ECS_VECTORCALL IsAbovePlaneMask(Plane plane, Vector8 point) {
		// Get a point on the plane, calculate the direction to the original point
		// If the cosine of the normal and the direction is positive, then the point
		// is above. If the value is 0, then it's on the plane and negative means bellow the plane
		Vector8 plane_point = plane.normal_dot * PerLaneBroadcast<3>(plane.normal_dot);
		Vector8 direction = point - plane_point;
		Vector8 cosine = Dot(plane.normal_dot, direction);
		Vector8 mask = cosine > ZeroVector();
		mask = PerLaneBroadcast<0>(mask);
		return mask;
	}

}