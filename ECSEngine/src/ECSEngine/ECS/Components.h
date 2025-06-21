// ECS_REFLECT
#pragma once

// This file contains all the engine components

#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/Reflection/ReflectionConstants.h"
#include "../Tools/UI/UIReflectionMacros.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "../Rendering/Camera.h"
#include "InternalStructures.h"

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
		[[ECS_UI_OMIT_FIELD_REFLECT]]
		unsigned int id;
		// This value indicates whether or not the entity is detached
		// From its prefab (the entity is no longer updated when the prefab is)
		[[ECS_UI_OMIT_FIELD_REFLECT]]
		bool detached;
	};

	struct EntityManager;

	// Returns the name of the entity if it has such a component, else it will fill in the name in the storage
	// With the index based description
	ECSENGINE_API Stream<char> GetEntityName(const EntityManager* entity_manager, Entity entity, CapacityStream<char>& storage);

	// Returns the name of the entity if it has such a component, else it will write the index of the entity
	// Into the storage (without the leading Entity)
	ECSENGINE_API Stream<char> GetEntityNameIndexOnly(const EntityManager* entity_manager, Entity entity, CapacityStream<char>& storage);

	// Returns the name of the entity concatenated with the extended entity string if it has such a component, 
	// else it will fill in the name in the storage with the extended description
	ECSENGINE_API Stream<char> GetEntityNameExtended(const EntityManager* entity_manager, Entity entity, CapacityStream<char>& storage);

	// This version has static storage inside it such that you don't have to pass a parameter to it
	// But the drawback is that you can reference at most one name since otherwise it might get overwritten
	// The boolean thread safe can be used for the function to operate in the a thread safe manner. It allows a certain
	// Number of threads at the same time.
	ECSENGINE_API Stream<char> GetEntityNameTempStorage(const EntityManager* entity_manager, Entity entity);

	// This version has static storage inside it such that you don't have to pass a parameter to it
	// But the drawback is that you can reference at most one name since otherwise it might get overwritten
	// The boolean thread safe can be used for the function to operate in the a thread safe manner. It allows a certain
	// Number of threads at the same time.
	ECSENGINE_API Stream<char> GetEntityNameIndexOnlyTempStorage(const EntityManager* entity_manager, Entity entity);

	// This version has static storage inside it such that you don't have to pass a parameter to it
	// But the drawback is that you can reference at most one name since otherwise it might get overwritten
	// The boolean thread safe can be used for the function to operate in the a thread safe manner. It allows a certain
	// Number of threads at the same time.
	ECSENGINE_API Stream<char> GetEntityNameExtendedTempStorage(const EntityManager* entity_manager, Entity entity);

	// Verifies for nullptr and returns 0.0f if it is
	ECS_INLINE float3 GetTranslation(const Translation* translation) {
		return translation != nullptr ? translation->value : float3::Splat(0.0f);
	}

	// Returns an identity rotation if it is nullptr
	ECS_INLINE QuaternionScalar GetRotation(const Rotation* rotation) {
		return rotation != nullptr ? QuaternionScalar(rotation->value) : QuaternionIdentityScalar();
	}

	// Verifies for nullptr and returns 1.0f if it is
	ECS_INLINE float3 GetScale(const Scale* scale) {
		return scale != nullptr ? scale->value : float3::Splat(1.0f);
	}

	// ------------------------------------ Link Components -----------------------------------------------------------

	struct ECS_REFLECT_LINK_COMPONENT(Rotation) RotationLink {
		float3 value;
	};

	struct ECS_REFLECT_LINK_COMPONENT(CameraComponent) CameraComponentLink {
		CameraParametersFOV value;
	};

	struct ModuleRegisterLinkComponentFunctionData;
	struct ModuleRegisterExtraInformationFunctionData;
	struct ModuleRegisterComponentFunctionsData;

	ECSENGINE_API void RegisterECSLinkComponents(ModuleRegisterLinkComponentFunctionData* register_data);

	ECSENGINE_API void RegisterECSModuleExtraInformation(ModuleRegisterExtraInformationFunctionData* register_data);

	ECSENGINE_API void RegisterECSComponentFunctions(ModuleRegisterComponentFunctionsData* register_data);

}