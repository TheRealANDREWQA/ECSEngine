// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"
#include "ECSEngineAssetMacros.h"
#include "ECSEngineResources.h"

#define GRAPHICS_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

#define GRAPHICS_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

#define GRAPHICS_GLOBAL_COMPONENT_BASE ECS_CONSTANT_REFLECT(100)

struct ECS_REFLECT_COMPONENT RenderEverything {
	constexpr static ECS_INLINE short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 6;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return true;
	}

	ECSEngine::ResourceView texture;
	ECSEngine::SamplerState sampler_state;
	ECSEngine::VertexShader vertex_shader;
	ECSEngine::PixelShader pixel_shader;
	ECSEngine::Material* material;
	ECSEngine::Stream<void> misc_data;
};

//struct ECS_REFLECT_COMPONENT Colorssss {
//	constexpr static ECS_INLINE short ID() {
//		return GRAPHICS_SHARED_COMPONENT_BASE + 7;
//	}
//
//	constexpr static ECS_INLINE bool IsShared() {
//		return true;
//	}
//
//	ECSEngine::Color value;
//};

//struct ECS_REFLECT_COMPONENT GraphicsName {
//	constexpr static inline short ID() {
//		return GRAPHICS_SHARED_COMPONENT_BASE + 2;
//	}
//
//	constexpr static inline size_t AllocatorSize() {
//		return ECS_KB_R * 2;
//	}
//
//	constexpr static inline bool IsShared() {
//		return false;
//	}
//
//	ECSEngine::Stream<char> name;
//};

//struct ECS_REFLECT_COMPONENT CheckBOXU {
//	constexpr static ECS_INLINE short ID() {
//		return 2313;
//	}
//
//	constexpr static ECS_INLINE bool IsShared() {
//		return false;
//	}
//
//	bool my_flag;
//};

struct ECS_REFLECT_COMPONENT GTranslation {
	constexpr static ECS_INLINE short ID() {
		return GRAPHICS_COMPONENT_BASE + 3;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return true;
	}

	ECSEngine::float3 translation = { 1.0f, 10.0f, 0.0f };
	ECSEngine::float3 interesting_new_field;
	ECSEngine::CoalescedMesh* POGGERSmesh;
};

struct ECS_REFLECT_COMPONENT RenderMesh {
	constexpr static ECS_INLINE short ID() {
		return GRAPHICS_SHARED_COMPONENT_BASE + 100;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return true;
	}

	ECS_INLINE bool Validate() const {
		return IsAssetPointerValid(mesh) && IsAssetPointerValid(material);
	}

	ECSEngine::CoalescedMesh* mesh;
	ECSEngine::Material* material;
};

struct ECS_REFLECT_SETTINGS Settings {
	float factor;
	float inverse;
	unsigned int count;
};