#pragma once
#include "../../Core.h"
#include "../../Utilities/BasicTypes.h"
#include "../../Math/Quaternion.h"
#include "../Mouse.h"

namespace ECSEngine {

	struct InputMapping;

	// Returns euler angles that for a delta rotation that need to
	// Be applied to the rotating element/s
	ECS_INLINE float3 FirstPersonController(float2 input_delta, float rotation_factor) {
		return { input_delta.y * rotation_factor, input_delta.x * rotation_factor, 0.0f };
	}

	// It returns a quaternion rotation that needs to be applied to
	// The rotating element/s
	ECSENGINE_API QuaternionScalar FirstPersonControllerQuaternion(float2 input_delta, float rotation_factor);

	// Returns euler angles that for a delta rotation that need to
	// Be applied to the rotating element/s
	ECS_INLINE float3 FirstPersonController(const Mouse* mouse, float rotation_factor) {
		return FirstPersonController(mouse->GetPositionDelta(), rotation_factor);
	}

	// It returns a quaternion rotation that needs to be applied to
	// The rotating element/s
	ECS_INLINE QuaternionScalar FirstPersonControllerQuaternion(
		const Mouse* mouse,
		float rotation_factor
	) {
		return FirstPersonControllerQuaternion(mouse->GetPositionDelta(), rotation_factor);
	}

	// It will apply the modifications to the given translation and rotation
	ECSENGINE_API void FirstPersonWASDController(
		bool w,
		bool a,
		bool s,
		bool d,
		float2 input_delta,
		float movement_factor,
		float rotation_factor,
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
		float3& translation,
		QuaternionScalar& rotation
	);

}