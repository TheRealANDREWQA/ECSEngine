#pragma once
#include "../../Core.h"
#include "../../Utilities/BasicTypes.h"
#include "../../Math/Quaternion.h"
#include "../Mouse.h"

namespace ECSEngine {

	struct InputMapping;
	struct Keyboard;

	// Returns euler angles that for a delta rotation that need to
	// Be applied to the rotating element/s
	ECS_INLINE float3 FirstPersonController(float2 input_delta, float rotation_factor, float delta_time) {
		return { input_delta.y * rotation_factor * delta_time, input_delta.x * rotation_factor * delta_time, 0.0f };
	}

	// It returns a quaternion rotation that needs to be applied to
	// The rotating element/s
	ECSENGINE_API QuaternionScalar FirstPersonControllerQuaternion(float2 input_delta, float rotation_factor, float delta_time);

	// Returns euler angles that for a delta rotation that need to
	// Be applied to the rotating element/s
	ECS_INLINE float3 FirstPersonController(const Mouse* mouse, float rotation_factor, float delta_time) {
		return FirstPersonController(mouse->GetPositionDelta(), rotation_factor, delta_time);
	}

	// Returns euler angles that for a delta rotation that need to
	// Be applied to the rotating element/s. It will modify the rotation factor
	// If the ctrl or shift is pressed, to allow modifying the rotation factor 
	// through the keyboard
	ECSENGINE_API float3 FirstPersonControllerModifiers(const Mouse* mouse, float rotation_factor, float delta_time, const Keyboard* keyboard);

	// It returns a quaternion rotation that needs to be applied to
	// The rotating element/s
	ECS_INLINE QuaternionScalar FirstPersonControllerQuaternion(
		const Mouse* mouse,
		float rotation_factor,
		float delta_time
	) {
		return FirstPersonControllerQuaternion(mouse->GetPositionDelta(), rotation_factor, delta_time);
	}

	// It returns a quaternion rotation that needs to be applied to
	// The rotating element/s. It will modify the rotation factor
	// If the ctrl or shift is pressed, to allow modifying the rotation factor 
	// through the keyboard
	ECSENGINE_API QuaternionScalar FirstPersonControllerQuaternionModifiers(
		const Mouse* mouse,
		float rotation_factor,
		float delta_time,
		const Keyboard* keyboard
	);

	// It will apply the modifications to the given translation and rotation
	ECSENGINE_API void FirstPersonWASDController(
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
	);

	// It will apply the modifications to the given translation and rotation
	ECSENGINE_API void FirstPersonWASDControllerQuaternion(
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
	);

	// It will apply the modifications to the given translation and rotation
	ECSENGINE_API void FirstPersonWASDController(
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
	);

	// It will apply the modifications to the given translation and rotation
	// It will modify the rotation factor and movement factor
	// If the ctrl or shift is pressed, to allow modifying the factors
	// through the keyboard
	ECSENGINE_API void FirstPersonWASDControllerModifiers(
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
	);

	// It will apply the modifications to the given translation and rotation
	// It will use the default mappings for WASD
	ECSENGINE_API void FirstPersonWASDController(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		float3& rotation
	);

	// It will use the default mappings for WASD
	// It will modify the rotation factor and movement factor
	// If the ctrl or shift is pressed, to allow modifying the factors
	// through the keyboard
	ECSENGINE_API void FirstPersonWASDControllerModifiers(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		float3& rotation
	);

	// It will apply the modifications to the given translation and rotation
	ECSENGINE_API void FirstPersonWASDControllerQuaternion(
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
	);

	// It will apply the modifications to the given translation and rotation
	// It will modify the rotation factor and movement factor
	// If the ctrl or shift is pressed, to allow modifying the factors
	// through the keyboard
	ECSENGINE_API void FirstPersonWASDControllerQuaternionModifiers(
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
	);

	// It will apply the modifications to the given translation and rotation
	// It will use the default mappings for WASD
	ECSENGINE_API void FirstPersonWASDControllerQuaternion(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		QuaternionScalar& rotation
	);

	// It will apply the modifications to the given translation and rotation
	// It will use the default mappings for WASD.
	// It will modify the rotation factor and movement factor
	// If the ctrl or shift is pressed, to allow modifying the factors
	// through the keyboard
	ECSENGINE_API void FirstPersonWASDControllerQuaternionModifiers(
		const Mouse* mouse,
		const Keyboard* keyboard,
		float movement_factor,
		float rotation_factor,
		float delta_time,
		float3& translation,
		QuaternionScalar& rotation
	);

}