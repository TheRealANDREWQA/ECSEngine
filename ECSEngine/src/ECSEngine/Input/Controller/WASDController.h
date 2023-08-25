#pragma once
#include "../../Core.h"
#include "../InputMapping.h"
#include "../../Utilities/BasicTypes.h"

namespace ECSEngine {

	// Returns a translation delta that needs to be applied
	// The directions need to be normalized
	ECSENGINE_API float3 WASDController(
		bool w, 
		bool a, 
		bool s, 
		bool d, 
		float movement_factor, 
		float3 forward_direction_normalized, 
		float3 right_direction_normalized
	);

	ECSENGINE_API float3 WASDController(
		const InputMapping* mapping,
		unsigned int w,
		unsigned int a,
		unsigned int s,
		unsigned int d,
		float movement_factor,
		float3 forward_direction_normalized,
		float3 right_direction_normalized
	);

}