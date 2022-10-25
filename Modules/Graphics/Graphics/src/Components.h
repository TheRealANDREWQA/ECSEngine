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

	ECSEngine::CoallescedMesh mesh; ECS_GIVE_SIZE_REFLECTION(static_assert(sizeof(ECSEngine::CoallescedMesh) == 152))
	unsigned int count;
	ECSEngine::Color color;
};

struct ECS_REFLECT_LINK_COMPONENT(RenderMesh) RenderMesh_Link {
	unsigned int mesh_handle; ECS_MESH_HANDLE
	unsigned int count;
	ECSEngine::Color color;
};

struct ECS_REFLECT_SHARED_COMPONENT RenderEverything {
	ECS_EVALUATE_FUNCTION_REFLECT static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 6;
	}

	ECSEngine::ResourceView texture; ECS_GIVE_SIZE_REFLECTION(static_assert(sizeof(ECSEngine::ResourceView) == 8))
	ECSEngine::SamplerState sampler_state; ECS_GIVE_SIZE_REFLECTION(static_assert(sizeof(ECSEngine::SamplerState) == 8))
	ECSEngine::VertexShader vertex_shader; ECS_GIVE_SIZE_REFLECTION(static_assert(sizeof(ECSEngine::VertexShader) == 8))
	ECSEngine::PixelShader pixel_shader; ECS_GIVE_SIZE_REFLECTION(static_assert(sizeof(ECSEngine::PixelShader) == 8))
	ECSEngine::Material material; ECS_GIVE_SIZE_REFLECTION(static_assert(sizeof(ECSEngine::Material) == 328))
	ECSEngine::Stream<void> misc_data; ECS_GIVE_SIZE_REFLECTION(static_assert(sizeof(ECSEngine::Stream<void>) == 16))
};

struct ECS_REFLECT_LINK_COMPONENT(RenderEverything) RenderEverything_Link {
	unsigned int texture_handle; ECS_TEXTURE_HANDLE
	unsigned int sampler_handle; ECS_GPU_SAMPLER_HANDLE
	unsigned int vertex_handle; ECS_SHADER_HANDLE
	unsigned int pixel_handle; ECS_SHADER_HANDLE
	unsigned int material_handle; ECS_MATERIAL_HANDLE
	unsigned int misc_handle; ECS_MISC_HANDLE
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

struct ECS_REFLECT_SETTINGS Settings {
	float factor;
	float inverse;
	unsigned int count;
};