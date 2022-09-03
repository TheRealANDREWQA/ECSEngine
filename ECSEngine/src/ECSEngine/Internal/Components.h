// ECS_REFLECT
#pragma once

// This file contains all the engine components

#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"

#define ECS_COMPONENT_BASE ECS_CONSTANT_REFLECT(0)

namespace ECSEngine {

	struct ECS_REFLECT_COMPONENT Translation {
		ECS_EVALUATE_FUNCTION_REFLECT static unsigned short ID() {
			return ECS_COMPONENT_BASE + 1;
		}

		float3 value;
	};

}