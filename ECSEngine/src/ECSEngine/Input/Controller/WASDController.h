#pragma once
#include "../../Core.h"
#include "../InputMapping.h"
#include "../../Utilities/BasicTypes.h"

namespace ECSEngine {

	struct Keyboard;

	// Returns a translation delta that needs to be applied
	// The directions need to be normalized
	ECSENGINE_API float3 WASDController(
		bool w, 
		bool a,
		bool s, 
		bool d, 
		float movement_factor, 
		float delta_time,
		float3 forward_direction_normalized, 
		float3 right_direction_normalized
	);

	// It will use the default WASD mappings for the keyboard
	ECSENGINE_API float3 WASDController(
		const Keyboard* keyboard,
		float movement_factor,
		float delta_time,
		float3 forward_direction_normalized,
		float3 right_direction_normalized
	);

	// The w, a, s, d are the button mappings
	ECSENGINE_API float3 WASDController(
		const InputMapping* mapping,
		unsigned int w,
		unsigned int a,
		unsigned int s,
		unsigned int d,
		float movement_factor,
		float delta_time,
		float3 forward_direction_normalized,
		float3 right_direction_normalized
	);

}