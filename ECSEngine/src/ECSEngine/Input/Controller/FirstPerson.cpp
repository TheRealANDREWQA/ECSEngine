#include "ecspch.h"
#include "FirstPerson.h"
#include "WASDController.h"

namespace ECSEngine {

	QuaternionScalar FirstPersonControllerQuaternion(float2 input_delta, float rotation_factor) {
		return QuaternionAngleFromAxis(GetUpVector(), input_delta.x * rotation_factor) * QuaternionAngleFromAxis(GetRightVector(), input_delta.y * rotation_factor);
	}

	void FirstPersonWASDController(
		bool w,
		bool a,
		bool s,
		bool d,
		float2 input_delta,
		float movement_factor,
		float rotation_factor,
		float3& translation,
		float3& rotation
	) {
		QuaternionScalar quaternion_rotation = QuaternionFromEuler(rotation);
		float3 forward_direction = RotateVector(GetForwardVector(), quaternion_rotation);
		float3 right_direction = RotateVector(GetRightVector(), quaternion_rotation);
		translation += WASDController(w, a, s, d, movement_factor, forward_direction, right_direction);
		rotation += FirstPersonController(input_delta, rotation_factor);
	}

	void FirstPersonWASDControllerQuaternion(
		bool w,
		bool a,
		bool s,
		bool d,
		float2 input_delta,
		float movement_factor,
		float rotation_factor,
		float3& translation,
		QuaternionScalar& rotation
	) {
		float3 forward_direction = RotateVector(GetForwardVector(), rotation);
		float3 right_direction = RotateVector(GetRightVector(), rotation);
		translation += WASDController(w, a, s, d, movement_factor, forward_direction, right_direction);
		rotation = RotateQuaternion(rotation, FirstPersonControllerQuaternion(input_delta, rotation_factor));
	}

	void FirstPersonWASDController(
		const InputMapping* input_mapping,
		const Mouse* mouse,
		unsigned int w,
		unsigned int a,
		unsigned int s,
		unsigned int d,
		float movement_factor,
		float rotation_factor,
		float3& translation,
		float3& rotation
	) {
		QuaternionScalar quaternion_rotation = QuaternionFromEuler(rotation);
		float3 forward_direction = RotateVector(GetForwardVector(), quaternion_rotation);
		float3 right_direction = RotateVector(GetRightVector(), quaternion_rotation);
		translation += WASDController(input_mapping, w, a, s, d, movement_factor, forward_direction, right_direction);
		rotation += FirstPersonController(mouse, rotation_factor);
	}

	void FirstPersonWASDControllerQuaternion(
		const InputMapping* input_mapping,
		const Mouse* mouse,
		unsigned int w,
		unsigned int a,
		unsigned int s,
		unsigned int d,
		float movement_factor,
		float rotation_factor,
		float3& translation,
		QuaternionScalar& rotation
	) {
		float3 forward_direction = RotateVector(GetForwardVector(), rotation);
		float3 right_direction = RotateVector(GetRightVector(), rotation);
		translation += WASDController(input_mapping, w, a, s, d, movement_factor, forward_direction, right_direction);
		rotation = RotateQuaternion(rotation, FirstPersonControllerQuaternion(mouse, rotation_factor));
	}

}