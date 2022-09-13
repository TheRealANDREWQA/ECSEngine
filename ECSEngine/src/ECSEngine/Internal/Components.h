// ECS_REFLECT
#pragma once

// This file contains all the engine components

#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/Reflection/ReflectionConstants.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"

#define ECS_COMPONENT_BASE ECS_CONSTANT_REFLECT(0)

namespace ECSEngine {

	// The name of the unique and shared components from the engine
	inline Stream<char> ECS_COMPONENTS[] = {
		STRING(Translation),
		STRING(Name)
	};

	struct ECS_REFLECT_COMPONENT Translation {
		ECS_EVALUATE_FUNCTION_REFLECT static short ID() {
			return ECS_COMPONENT_BASE + 0;
		}

		float3 value;
	};

	struct ECS_REFLECT_COMPONENT Name {
		ECS_EVALUATE_FUNCTION_REFLECT static short ID() {
			return ECS_COMPONENT_BASE + 1;
		}

		ECS_EVALUATE_FUNCTION_REFLECT static size_t AllocatorSize() {
			return ECS_MB_R;
		}

		Stream<char> name;
	};

}