// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"

#define GRAPHICS_COMPONENT_BASE ECS_CONSTANT_REFLECT(0)

#define GRAPHICS_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(0)

struct ECS_REFLECT_COMPONENT GraphicsTranslation {
	ECS_EVALUATE_FUNCTION_REFLECT static inline short ID() {
		return GRAPHICS_COMPONENT_BASE + 25;
	}

	ECSEngine::float3 translation;
};

struct ECS_REFLECT_SHARED_COMPONENT GraphicsMesh {
	ECS_EVALUATE_FUNCTION_REFLECT static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 0;
	}

	ECS_EVALUATE_FUNCTION_REFLECT static inline size_t AllocatorSize() {
		return 120;
	}

	ECSEngine::Stream<char> name;
};

struct ECS_REFLECT_SHARED_COMPONENT GraphicsTexture {
	ECS_EVALUATE_FUNCTION_REFLECT static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 1;
	}

	ECS_EVALUATE_FUNCTION_REFLECT static inline size_t AllocatorSize() {
		return ECS_KB_R * 2;
	}

	ECSEngine::Stream<char> name;
	ECSEngine::float3 value;
};