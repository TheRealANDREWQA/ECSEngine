// ECS_REFLECT
#pragma once

// This file contains all the engine components

#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/Reflection/ReflectionConstants.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "../Rendering/Camera.h"

// The base of the ECSEngine unique components
#define ECS_COMPONENT_BASE ECS_CONSTANT_REFLECT(0)
// The base of the ECSEngine shared components
#define ECS_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(0)
// The base of the ECSEngine global components
#define ECS_GLOBAL_COMPONENT_BASE ECS_CONSTANT_REFLECT(0)

namespace ECSEngine {

	struct CoalescedMesh;
	struct Material;

	// The name of the unique and shared components from the engine
	inline Stream<char> ECS_COMPONENTS[] = {
		STRING(Translation),
		STRING(Rotation),
		STRING(Scale),
		STRING(Name),
		STRING(CameraComponent)
	};

	struct ECS_REFLECT_COMPONENT Translation {
		constexpr static ECS_INLINE short ID() {
			return ECS_COMPONENT_BASE + 0;
		}

		constexpr static ECS_INLINE bool IsShared() {
			return false;
		}

		float3 value = { 0.0f, 0.0f, 0.0f };
	};

	struct ECS_REFLECT_COMPONENT Rotation {
		constexpr static ECS_INLINE short ID() {
			return ECS_COMPONENT_BASE + 1;
		}

		constexpr static ECS_INLINE bool IsShared() {
			return false;
		}

		float4 value = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	struct ECS_REFLECT_COMPONENT Scale {
		constexpr static ECS_INLINE short ID() {
			return ECS_COMPONENT_BASE + 2;
		}

		constexpr static ECS_INLINE bool IsShared() {
			return false;
		}

		float3 value = { 1.0f, 1.0f, 1.0f };
	};

	struct ECS_REFLECT_COMPONENT Name {
		constexpr static ECS_INLINE short ID() {
			return ECS_COMPONENT_BASE + 4;
		}

		constexpr static ECS_INLINE size_t AllocatorSize() {
			return ECS_KB_R * 256;
		}

		constexpr static ECS_INLINE bool IsShared() {
			return false;
		}

		Stream<char> name;
	};

	struct ECS_REFLECT_GLOBAL_COMPONENT CameraComponent {
		constexpr static ECS_INLINE short ID() {
			return ECS_GLOBAL_COMPONENT_BASE + 0;
		}

		CameraCached value;
	};

	// ------------------------------------ Link Components -----------------------------------------------------------

	struct ECS_REFLECT_LINK_COMPONENT(Rotation) RotationLink {
		float3 value;
	};

	struct ECS_REFLECT_LINK_COMPONENT(CameraComponent) CameraComponentLink {
		CameraParametersFOV value;
	};

	struct ModuleRegisterLinkComponentFunctionData;

	ECSENGINE_API void RegisterECSLinkComponents(ModuleRegisterLinkComponentFunctionData* register_data);

}