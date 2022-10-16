// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"
#include "ECSEngineAssetMacros.h"
#include "ECSEngineResources.h"

#define GRAPHICS_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

#define GRAPHICS_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

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

struct ECS_REFLECT_SHARED_COMPONENT RenderMesh {
	ECS_EVALUATE_FUNCTION_REFLECT static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 5;
	}

	ECSEngine::Mesh mesh; ECS_SKIP_REFLECTION(static_assert(sizeof(ECSEngine::Mesh) == 136))
};

struct ECS_REFLECT_LINK_COMPONENT(RenderMesh) RenderMesh_Link {
	unsigned int mesh_handle; ECS_MESH_HANDLE
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

struct ECS_REFLECT_COMPONENT GraphicsName {
	ECS_EVALUATE_FUNCTION_REFLECT static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 2;
	}

	ECS_EVALUATE_FUNCTION_REFLECT static inline size_t AllocatorSize() {
		return ECS_KB_R * 2;
	}

	ECSEngine::Stream<char> name;
};

struct ECS_REFLECT_COMPONENT GTranslation {
	ECS_EVALUATE_FUNCTION_REFLECT static inline short ID() {
		return GRAPHICS_COMPONENT_BASE + 3;
	}

	ECSEngine::float3 translation = { 0.5f, 10.0f, 0.0f };
};