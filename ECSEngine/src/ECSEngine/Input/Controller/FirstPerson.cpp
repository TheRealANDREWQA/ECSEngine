#include "ecspch.h"
#include "FirstPerson.h"
#include "WASDController.h"

namespace ECSEngine {

	float3 FirstPersonControllerModifiers(const Mouse* mouse, float rotation_factor, const Keyboard* keyboard) {
		return FirstPersonController(mouse, GetKeyboardModifierValue(keyboard, rotation_factor));
	}

	QuaternionScalar FirstPersonControllerQuaternion(float2 input_delta, float rotation_factor) {
		return QuaternionAngleFromAxis(GetUpVector(), input_delta.x * rotation_factor) * QuaternionAngleFromAxis(GetRightVector(), input_delta.y * rotation_factor);
	}

	QuaternionScalar FirstPersonControllerQuaternionModifiers(const Mouse* mouse, float rotation_factor, const Keyboard* keyboard) {
		return FirstPersonControllerQuaternion(mouse, GetKeyboardModifierValue(keyboard, rotation_factor));
	}

	void FirstPersonWASDController(
		bool w,
		bool a,
		bool s,
		bool d,
		float2 input_delta,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		float3& rotation
	) {
		QuaternionScalar quaternion_rotation = QuaternionFromEuler(rotation);
		float3 forward_direction = RotateVector(GetForwardVector(), quaternion_rotation);
		float3 right_direction = RotateVector(GetRightVector(), quaternion_rotation);
		translation += WASDController(w, a, s, d, movement_factor, delta_time, forward_direction, right_direction);
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
		float delta_time,
		float3& translation,
		QuaternionScalar& rotation
	) {
		float3 forward_direction = RotateVector(GetForwardVector(), rotation);
		float3 right_direction = RotateVector(GetRightVector(), rotation);
		translation += WASDController(w, a, s, d, movement_factor, delta_time, forward_direction, right_direction);
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
		float delta_time,
		float3& translation,
		float3& rotation
	) {
		QuaternionScalar quaternion_rotation = QuaternionFromEuler(rotation);
		float3 forward_direction = RotateVector(GetForwardVector(), quaternion_rotation);
		float3 right_direction = RotateVector(GetRightVector(), quaternion_rotation);
		translation += WASDController(input_mapping, w, a, s, d, movement_factor, delta_time, forward_direction, right_direction);
		rotation += FirstPersonController(mouse, rotation_factor);
	}

	void FirstPersonWASDController(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		float3& rotation
	) {
		FirstPersonWASDController(
			keyboard->IsDown(ECS_KEY_W),
			keyboard->IsDown(ECS_KEY_A),
			keyboard->IsDown(ECS_KEY_S),
			keyboard->IsDown(ECS_KEY_D),
			mouse->GetPositionDelta(),
			movement_factor,
			rotation_factor,
			delta_time,
			translation,
			rotation
		);
	}

	void FirstPersonWASDControllerModifiers(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		float3& rotation
	) {
		FirstPersonWASDController(mouse, keyboard, GetKeyboardModifierValue(keyboard, movement_factor), GetKeyboardModifierValue(keyboard, rotation_factor), delta_time, translation, rotation);
	}

	void FirstPersonWASDControllerModifiers(
		const InputMapping* input_mapping,
		const Mouse* mouse,
		const Keyboard* keyboard,
		unsigned int w,
		unsigned int a,
		unsigned int s,
		unsigned int d,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		float3& rotation
	) {
		FirstPersonWASDController(
			input_mapping,
			mouse,
			w,
			a,
			s,
			d,
			GetKeyboardModifierValue(keyboard, movement_factor),
			GetKeyboardModifierValue(keyboard, rotation_factor),
			delta_time,
			translation,
			rotation
		);
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
		float delta_time,
		float3& translation,
		QuaternionScalar& rotation
	) {
		float3 forward_direction = RotateVector(GetForwardVector(), rotation);
		float3 right_direction = RotateVector(GetRightVector(), rotation);
		translation += WASDController(input_mapping, w, a, s, d, movement_factor, delta_time, forward_direction, right_direction);
		rotation = RotateQuaternion(rotation, FirstPersonControllerQuaternion(mouse, rotation_factor));
	}

	void FirstPersonWASDControllerQuaternionModifiers(
		const InputMapping* input_mapping,
		const Mouse* mouse,
		const Keyboard* keyboard,
		unsigned int w,
		unsigned int a,
		unsigned int s,
		unsigned int d,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		QuaternionScalar& rotation
	) {
		FirstPersonWASDControllerQuaternion(
			input_mapping,
			mouse,
			w,
			a,
			s,
			d,
			GetKeyboardModifierValue(keyboard, movement_factor),
			GetKeyboardModifierValue(keyboard, rotation_factor),
			delta_time,
			translation,
			rotation
		);
	}

	void FirstPersonWASDControllerQuaternion(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		QuaternionScalar& rotation
	) {
		FirstPersonWASDControllerQuaternion(
			keyboard->IsDown(ECS_KEY_W),
			keyboard->IsDown(ECS_KEY_A),
			keyboard->IsDown(ECS_KEY_S),
			keyboard->IsDown(ECS_KEY_D),
			mouse->GetPositionDelta(),
			movement_factor,
			rotation_factor,
			delta_time,
			translation, rotation
		);
	}

	void FirstPersonWASDControllerQuaternionModifiers(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		QuaternionScalar& rotation
	) {
		FirstPersonWASDControllerQuaternion(
			mouse, 
			keyboard, 
			GetKeyboardModifierValue(keyboard, movement_factor),
			GetKeyboardModifierValue(keyboard, rotation_factor),
			delta_time, 
			translation, 
			rotation
		);
	}

}