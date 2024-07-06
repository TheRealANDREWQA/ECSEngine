// ECS_REFLECT
#pragma once
#include "PhysicsComponents.h"

struct ECS_REFLECT_GLOBAL_COMPONENT PhysicsSettings {
	constexpr inline static short ID() {
		return PHYSICS_GLOBAL_COMPONENT_BASE + 0;
	}

	unsigned int iterations;
	float baumgarte_factor;
};