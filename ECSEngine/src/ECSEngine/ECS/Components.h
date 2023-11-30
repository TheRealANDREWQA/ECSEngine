// ECS_REFLECT
#pragma once

// This file contains all the engine components

#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/Reflection/ReflectionConstants.h"
#include "../Tools/UI/UIReflectionMacros.h"
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
		ECS_INLINE constexpr static short ID() {
			return ECS_COMPONENT_BASE + 0;
		}

		ECS_INLINE constexpr static bool IsShared() {
			return false;
		}

		float3 value = { 0.0f, 0.0f, 0.0f };
	};

	struct ECS_REFLECT_COMPONENT Rotation {
		ECS_INLINE constexpr static short ID() {
			return ECS_COMPONENT_BASE + 1;
		}

		ECS_INLINE constexpr static bool IsShared() {
			return false;
		}

		float4 value = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	struct ECS_REFLECT_COMPONENT Scale {
		ECS_INLINE constexpr static short ID() {
			return ECS_COMPONENT_BASE + 2;
		}

		ECS_INLINE constexpr static bool IsShared() {
			return false;
		}

		float3 value = { 1.0f, 1.0f, 1.0f };
	};

	struct ECS_REFLECT_COMPONENT Name {
		ECS_INLINE constexpr static short ID() {
			return ECS_COMPONENT_BASE + 3;
		}

		ECS_INLINE constexpr static size_t AllocatorSize() {
			return ECS_KB_R * 256;
		}

		ECS_INLINE constexpr static bool IsShared() {
			return false;
		}

		Stream<char> name;
	};

	struct ECS_REFLECT_GLOBAL_COMPONENT CameraComponent {
		ECS_INLINE constexpr static short ID() {
			return ECS_GLOBAL_COMPONENT_BASE + 0;
		}

		CameraCached value;
	};

	struct ECS_REFLECT_COMPONENT PrefabComponent {
		ECS_INLINE constexpr static short ID() {
			return ECS_COMPONENT_BASE + 4;
		}

		ECS_INLINE constexpr static bool IsShared() {
			return false;
		}

		// This is a unique identifier that is to be implemented
		// Inside the editor to keep track of each instance
		unsigned int id; ECS_UI_OMIT_FIELD_REFLECT
		// This value indicates whether or not the entity is detached
		// From its prefab (the entity is no longer updated when the prefab is)
		bool detached; ECS_UI_OMIT_FIELD_REFLECT
	};

	// ------------------------------------ Link Components -----------------------------------------------------------

	struct ECS_REFLECT_LINK_COMPONENT(Rotation) RotationLink {
		float3 value;
	};

	struct ECS_REFLECT_LINK_COMPONENT(CameraComponent) CameraComponentLink {
		CameraParametersFOV value;
	};

	struct ModuleRegisterLinkComponentFunctionData;
	struct ModuleRegisterExtraInformationFunctionData;
	struct ModuleRegisterDebugDrawFunctionData;

	ECSENGINE_API void RegisterECSLinkComponents(ModuleRegisterLinkComponentFunctionData* register_data);

	ECSENGINE_API void RegisterECSModuleExtraInformation(ModuleRegisterExtraInformationFunctionData* register_data);

	ECSENGINE_API void RegisterECSDebugDrawElements(ModuleRegisterDebugDrawFunctionData* register_data);

}