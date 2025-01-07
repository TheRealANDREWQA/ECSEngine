// ECS_REFLECT
#pragma once
#include "PhysicsComponents.h"

struct ECS_REFLECT_GLOBAL_COMPONENT PhysicsSettings {
	ECS_INLINE constexpr static short ID() {
		return PHYSICS_GLOBAL_COMPONENT_BASE + 0;
	}
	unsigned int iterations = 4;
	float baumgarte_factor = 0.05f;
	bool use_warm_starting = true;
	//bool first_person = true;
};