// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"
#include "ECSEngineAssetMacros.h"
#include "ECSEngineResources.h"

#define GRAPHICS_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

#define GRAPHICS_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

struct ECS_REFLECT_COMPONENT GraphicsTranslation {
	constexpr static inline short ID() {
		return GRAPHICS_COMPONENT_BASE + 25;
	}

	constexpr static inline bool IsShared() {
		return false;
	}

	ECSEngine::float3 translation;
};

struct ECS_REFLECT_SHARED_COMPONENT GraphicsMesh {
	constexpr static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 0;
	}

	constexpr static inline size_t AllocatorSize() {
		return 120;
	}

	constexpr static inline bool IsShared() {
		return false;
	}

	ECSEngine::Stream<char> name;
};

struct ECS_REFLECT_SHARED_COMPONENT RenderMesh {
	constexpr static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 5;
	}

	constexpr static inline bool IsShared() {
		return true;
	}

	ECSEngine::CoallescedMesh* mesh;
	unsigned int count;
	unsigned int new_count;
	ECSEngine::Color color;
	ECSEngine::Color new_color;
};

struct ECS_REFLECT_SHARED_COMPONENT RenderEverything {
	constexpr static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 6;
	}

	constexpr static inline bool IsShared() {
		return true;
	}

	ECSEngine::ResourceView texture;
	ECSEngine::SamplerState sampler_state;
	ECSEngine::VertexShader vertex_shader;
	ECSEngine::PixelShader pixel_shader;
	ECSEngine::Material* material;
	ECSEngine::Stream<void> misc_data;
};

struct ECS_REFLECT_SHARED_COMPONENT GraphicsTexture {
	constexpr static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 1;
	}

	constexpr static inline size_t AllocatorSize() {
		return ECS_KB_R * 2;
	}

	constexpr static inline bool IsShared() {
		return false;
	}

	ECSEngine::Stream<char> name;
	ECSEngine::float3 value;
};

struct ECS_REFLECT_COMPONENT GraphicsName {
	constexpr static inline short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 2;
	}

	constexpr static inline size_t AllocatorSize() {
		return ECS_KB_R * 2;
	}

	constexpr static inline bool IsShared() {
		return false;
	}

	ECSEngine::Stream<char> name;
};

struct ECS_REFLECT_COMPONENT GTranslation {
	constexpr static inline short ID() {
		return GRAPHICS_COMPONENT_BASE + 3;
	}

	constexpr static inline bool IsShared() {
		return true;
	}

	ECSEngine::float3 translation = { 0.5f, 10.0f, 0.0f };
};

struct ECS_REFLECT_SETTINGS Settings {
	float factor;
	float inverse;
	unsigned int count;
};