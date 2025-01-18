// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"
#include "ECSEngineAssetMacros.h"
#include "ECSEngineResourceTypes.h"

#define GRAPHICS_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

#define GRAPHICS_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

#define GRAPHICS_GLOBAL_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

struct ECS_REFLECT_COMPONENT RenderMesh {
	constexpr ECS_INLINE static short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 0;
	}

	constexpr ECS_INLINE static bool IsShared() {
		return true;
	}

	ECS_INLINE bool Validate() const {
		return IsAssetPointerValid(mesh) && IsAssetPointerValid(material);
	}

	ECSEngine::CoalescedMesh* mesh;
	ECSEngine::Material* material;
};

struct ECS_REFLECT_COMPONENT RenderEverything {
	constexpr ECS_INLINE static short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 0;
	}

	constexpr ECS_INLINE static bool IsShared() {
		return true;
	}

	ECSEngine::ResourceView texture;
	ECSEngine::SamplerState sampler_state;
	ECSEngine::VertexShader vertex_shader;
	ECSEngine::PixelShader pixel_shader;
	ECSEngine::Material* material;
	ECSEngine::MiscAssetData misc_data;
};