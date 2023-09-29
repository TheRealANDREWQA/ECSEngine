#pragma once
#include "../../Core.h"
#include "ModuleDefinition.h"
#include "../../Rendering/TransformHelpers.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	struct GraphicsModuleRenderMeshBounds {
		ECS_INLINE bool IsValid() const {
			return component.size > 0 && field.size > 0;
		}

		Stream<char> component;
		Stream<char> field;
	};

	ECSENGINE_API GraphicsModuleRenderMeshBounds GetGraphicsModuleRenderMeshBounds(ModuleExtraInformation extra_information);

	ECSENGINE_API void SetGraphicsModuleRenderMeshBounds(ModuleRegisterExtraInformationFunctionData* register_data, Stream<char> component, Stream<char> field);

	// ------------------------------------------------------------------------------------------------------------

	struct EntityManager;
	namespace Reflection {
		struct ReflectionManager;
	}

	struct GlobalComponentTransformGizmos {
		ECS_INLINE bool IsValid() const {
			return component.size > 0;
		}

		size_t CopySize() const {
			return component.CopySize() + translation_field.CopySize() + rotation_field.CopySize() + scale_field.CopySize();
		}

		GlobalComponentTransformGizmos CopyTo(uintptr_t& ptr) const {
			GlobalComponentTransformGizmos copy;

			copy.component.InitializeAndCopy(ptr, component);
			copy.translation_field.InitializeAndCopy(ptr, translation_field);
			copy.rotation_field.InitializeAndCopy(ptr, rotation_field);
			copy.scale_field.InitializeAndCopy(ptr, scale_field);

			return copy;
		}

		Stream<char> component;
		Stream<char> translation_field;
		Stream<char> rotation_field;
		Stream<char> scale_field;
	};

	struct TransformGizmo {
		float3 position;
		QuaternionStorage rotation;
	};

	struct TransformGizmoPointers {
		bool is_euler_rotation;
		float3* position;
		union {
			float3* euler_rotation;
			float4* quaternion_rotation;
		};
		float3* scale;
	};

	ECSENGINE_API void GetGlobalComponentTransformGizmos(ModuleExtraInformation extra_information, CapacityStream<GlobalComponentTransformGizmos>* gizmos);

	ECSENGINE_API void SetGlobalComponentTransformGizmos(
		ModuleRegisterExtraInformationFunctionData* register_data,
		Stream<GlobalComponentTransformGizmos> gizmos
	);

	// It will fill in nullptr for global components that do not appear in that entity manager
	ECSENGINE_API void GetGlobalComponentTransformGizmosData(
		EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<GlobalComponentTransformGizmos> gizmos,
		CapacityStream<void*> data
	);

	// It will add the transform gizmo values only for data pointers that are not nullptr
	ECSENGINE_API void GetGlobalComponentTransformGizmoLocation(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<GlobalComponentTransformGizmos> gizmos,
		Stream<void*> data,
		CapacityStream<TransformGizmo>* transform_gizmo,
		bool skip_empty_data = true
	);

	// It will add the transform gizmo values only for data pointers that are not nullptr
	ECSENGINE_API void GetGlobalComponentTransformGizmoPointers(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<GlobalComponentTransformGizmos> gizmos,
		Stream<void*> data,
		CapacityStream<TransformGizmoPointers>* gizmo_pointers,
		bool skip_empty_data = true
	);

	// ------------------------------------------------------------------------------------------------------------

}