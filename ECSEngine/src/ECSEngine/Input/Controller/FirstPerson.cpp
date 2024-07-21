#include "ecspch.h"
#include "FirstPerson.h"
#include "WASDController.h"

namespace ECSEngine {

	ECS_INLINE static float GetModifierFactor(const Keyboard* keyboard, float factor) {
		float modifier = 1.0f;
		if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			modifier = 0.2f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			modifier = 5.0f;
		}
		return modifier * factor;
	}

	float3 FirstPersonControllerModifiers(const Mouse* mouse, float rotation_factor, float delta_time, const Keyboard* keyboard) {
		return FirstPersonController(mouse, GetModifierFactor(keyboard, rotation_factor), delta_time);
	}

	QuaternionScalar FirstPersonControllerQuaternion(float2 input_delta, float rotation_factor, float delta_time) {
		return QuaternionAngleFromAxis(GetUpVector(), input_delta.x * rotation_factor * delta_time) * QuaternionAngleFromAxis(GetRightVector(), input_delta.y * rotation_factor * delta_time);
	}

	QuaternionScalar FirstPersonControllerQuaternionModifiers(const Mouse* mouse, float rotation_factor, float delta_time, const Keyboard* keyboard) {
		return FirstPersonControllerQuaternion(mouse, GetModifierFactor(keyboard, rotation_factor), delta_time);
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
		rotation += FirstPersonController(input_delta, rotation_factor, delta_time);
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
		rotation = RotateQuaternion(rotation, FirstPersonControllerQuaternion(input_delta, rotation_factor, delta_time));
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
		rotation += FirstPersonController(mouse, rotation_factor, delta_time);
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
		FirstPersonWASDController(mouse, keyboard, GetModifierFactor(keyboard, movement_factor), GetModifierFactor(keyboard, rotation_factor), delta_time, translation, rotation);
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
			GetModifierFactor(keyboard, movement_factor),
			GetModifierFactor(keyboard, rotation_factor),
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
		rotation = RotateQuaternion(rotation, FirstPersonControllerQuaternion(mouse, rotation_factor, delta_time));
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
			GetModifierFactor(keyboard, movement_factor),
			GetModifierFactor(keyboard, rotation_factor),
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
			GetModifierFactor(keyboard, movement_factor), 
			GetModifierFactor(keyboard, rotation_factor), 
			delta_time, 
			translation, 
			rotation
		);
	}

}