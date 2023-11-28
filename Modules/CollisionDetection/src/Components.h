// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"

#define COLLISION_DETECTION_COMPONENT_BASE ECS_CONSTANT_REFLECT(200)

#define COLLISION_DETECTION_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(200)

#define COLLISION_DETECTION_GLOBAL_COMPONENT_BASE ECS_CONSTANT_REFLECT(200)

struct ECS_REFLECT_COMPONENT CollisionComponent {
	constexpr static ECS_INLINE short ID() {
		return COLLISION_DETECTION_COMPONENT_BASE + 1;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return false;
	}

	ECSEngine::float3 my_value;
};

struct ECS_REFLECT_COMPONENT Collider {
	constexpr static ECS_INLINE short ID() {
		return COLLISION_DETECTION_COMPONENT_BASE + 0;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return false;
	}

	ECSEngine::float3 value;
};

struct ECS_REFLECT_SETTINGS CollisionSettings {
	float factor;
	float inverse;
	unsigned int count;
};