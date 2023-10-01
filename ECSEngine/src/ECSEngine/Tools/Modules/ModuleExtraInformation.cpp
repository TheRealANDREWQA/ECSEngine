#include "ecspch.h"
#include "ModuleExtraInformation.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../ECS/EntityManager.h"
#include "../../ECS/ComponentHelpers.h"

#define RENDER_MESH_BOUNDS "RenderMeshBounds"
#define GLOBAL_COMPONENT_TRANSFORM_CONTROLLER "GlobalComponentTransformController"

namespace ECSEngine {


	// ------------------------------------------------------------------------------------------------------------

	GraphicsModuleRenderMeshBounds GetGraphicsModuleRenderMeshBounds(ModuleExtraInformation extra_information)
	{
		GraphicsModuleRenderMeshBounds bounds;

		Stream<Stream<void>> value_void = extra_information.Find(RENDER_MESH_BOUNDS);
		if (value_void.size == 2) {
			bounds.component = value_void[0].As<char>();
			bounds.field = value_void[1].As<char>();
		}

		return bounds;
	}

	void SetGraphicsModuleRenderMeshBounds(ModuleRegisterExtraInformationFunctionData* register_data, Stream<char> component, Stream<char> field)
	{
		Stream<void>* values = (Stream<void>*)Allocate(register_data->allocator, sizeof(Stream<void>) * 2);
		values[0] = component;
		values[1] = field;
		register_data->extra_information.Add({ RENDER_MESH_BOUNDS, { values, 2 } });
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetGlobalComponentTransformGizmos(ModuleExtraInformation extra_information, CapacityStream<GlobalComponentTransformGizmos>* gizmos)
	{
		Stream<Stream<void>> value_void = extra_information.Find(GLOBAL_COMPONENT_TRANSFORM_CONTROLLER);
		if (value_void.size > 0 && (value_void.size % 4 == 0)) {
			size_t add_count = value_void.size / 4;
			ECS_ASSERT(gizmos->size + add_count <= gizmos->capacity);
			for (size_t index = 0; index < add_count; index++) {
				size_t stream_index = index * 4;

				GlobalComponentTransformGizmos gizmo;
				gizmo.component = value_void[stream_index].As<char>();
				gizmo.translation_field = value_void[stream_index + 1].As<char>();
				gizmo.rotation_field = value_void[stream_index + 2].As<char>();
				gizmo.scale_field = value_void[stream_index + 3].As<char>();

				gizmos->Add(&gizmo);
			}
		}
	}

	void SetGlobalComponentTransformGizmos(
		ModuleRegisterExtraInformationFunctionData* register_data, 
		Stream<GlobalComponentTransformGizmos> gizmos
	)
	{
		// Each gizmo has 4 fields
		Stream<void>* void_streams = (Stream<void>*)Allocate(register_data->allocator, gizmos.size * 4 * sizeof(Stream<void>));
		for (size_t index = 0; index < gizmos.size; index++) {
			size_t current_index = index * 4;
			void_streams[current_index] = gizmos[index].component;
			void_streams[current_index + 1] = gizmos[index].translation_field;
			void_streams[current_index + 2] = gizmos[index].rotation_field;
			void_streams[current_index + 3] = gizmos[index].scale_field;
		}
		register_data->extra_information.Add({ GLOBAL_COMPONENT_TRANSFORM_CONTROLLER, { void_streams, gizmos.size * 4 } });
	}

	void GetGlobalComponentTransformGizmosData(
		EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<GlobalComponentTransformGizmos> gizmos,
		CapacityStream<void*> data
	) {
		for (size_t index = 0; index < gizmos.size; index++) {
			Reflection::ReflectionType reflection_type;
			if (reflection_manager->TryGetType(gizmos[index].component, reflection_type)) {
				data[index] = entity_manager->TryGetGlobalComponent(GetReflectionTypeComponent(&reflection_type));
			}
			else {
				data[index] = nullptr;
			}
		}
	}

	template<typename Functor>
	static void ForEachGlobalComponentTransformGizmoPointers(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<GlobalComponentTransformGizmos> gizmos,
		Stream<void*> data,
		bool skip_empty_data,
		Functor&& functor
	) {
		for (size_t index = 0; index < gizmos.size; index++) {
			if (!skip_empty_data || data[index] != nullptr) {
				const Reflection::ReflectionType* reflection_type = reflection_manager->GetType(gizmos[index].component);

				TransformGizmoPointers gizmo;
				gizmo.position = nullptr;
				gizmo.euler_rotation = nullptr;
				gizmo.is_euler_rotation = true;
				gizmo.scale = nullptr;

				if (data[index] != nullptr) {
					if (gizmos[index].translation_field.size > 0) {
						Reflection::ReflectionTypeFieldDeep deep_field = Reflection::FindReflectionTypeFieldDeep(
							reflection_manager,
							reflection_type,
							gizmos[index].translation_field
						);
						if (deep_field.IsValid()) {
							if (deep_field.type->fields[deep_field.field_index].info.basic_type == Reflection::ReflectionBasicFieldType::Float3
								&& !Reflection::IsStream(deep_field.type->fields[deep_field.field_index].info.stream_type)) {
								float3* translation = (float3*)deep_field.GetFieldData(data[index]);
								gizmo.position = translation;
							}
						}
					}

					if (gizmos[index].rotation_field.size > 0) {
						Reflection::ReflectionTypeFieldDeep deep_field = Reflection::FindReflectionTypeFieldDeep(
							reflection_manager,
							reflection_type,
							gizmos[index].rotation_field
						);
						if (deep_field.IsValid()) {
							if (!Reflection::IsStream(deep_field.type->fields[deep_field.field_index].info.stream_type)) {
								if (deep_field.type->fields[deep_field.field_index].info.basic_type == Reflection::ReflectionBasicFieldType::Float3) {
									gizmo.is_euler_rotation = true;
									float3* euler = (float3*)deep_field.GetFieldData(data[index]);
									gizmo.euler_rotation = euler;
								}
								else if (deep_field.type->fields[deep_field.field_index].info.basic_type == Reflection::ReflectionBasicFieldType::Float4) {
									gizmo.is_euler_rotation = false;
									float4* quaternion = (float4*)deep_field.GetFieldData(data[index]);
									gizmo.quaternion_rotation = quaternion;
								}
							}
						}
					}

					if (gizmos[index].scale_field.size > 0) {
						Reflection::ReflectionTypeFieldDeep deep_field = Reflection::FindReflectionTypeFieldDeep(
							reflection_manager,
							reflection_type,
							gizmos[index].scale_field
						);
						if (deep_field.IsValid()) {
							if (!Reflection::IsStream(deep_field.type->fields[deep_field.field_index].info.stream_type)) {
								if (deep_field.type->fields[deep_field.field_index].info.basic_type == Reflection::ReflectionBasicFieldType::Float3) {
									gizmo.scale = (float3*)deep_field.GetFieldData(data[index]);
								}
							}
						}
					}
				}

				functor(gizmo);
			}
		}
	}

	// It will add the transform gizmo values only for data pointers that are not nullptr
	void GetGlobalComponentTransformGizmoLocation(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<GlobalComponentTransformGizmos> gizmos,
		Stream<void*> data,
		CapacityStream<TransformGizmo>* transform_gizmo,
		bool skip_empty_data
	) {
		ForEachGlobalComponentTransformGizmoPointers(reflection_manager, gizmos, data, skip_empty_data, [&](TransformGizmoPointers pointers) {
			if (pointers.position != nullptr || pointers.euler_rotation != nullptr) {
				TransformGizmo gizmo;
				gizmo.position = pointers.position != nullptr ? *pointers.position : float3::Splat(0.0f);
				if (pointers.euler_rotation != nullptr) {
					if (pointers.is_euler_rotation) {
						gizmo.rotation = QuaternionFromEuler(*pointers.euler_rotation).StorageLow();
					}
					else {
						gizmo.rotation = *pointers.quaternion_rotation;
					}
				}

				transform_gizmo->AddAssert(gizmo);
			}
		});
	}

	void GetGlobalComponentTransformGizmoPointers(
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<GlobalComponentTransformGizmos> gizmos, 
		Stream<void*> data, 
		CapacityStream<TransformGizmoPointers>* gizmo_pointers,
		bool skip_empty_data
	)
	{
		ECS_ASSERT(gizmo_pointers->size + gizmos.size <= gizmo_pointers->capacity);
		ForEachGlobalComponentTransformGizmoPointers(reflection_manager, gizmos, data, skip_empty_data, [&](TransformGizmoPointers pointers) {
			gizmo_pointers->Add(pointers);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	TransformGizmo TransformGizmoPointers::ToValue() const
	{
		TransformGizmo value;

		if (position != nullptr) {
			value.position = *position;
		}
		else {
			value.position = float3::Splat(0.0f);
		}

		if (euler_rotation != nullptr) {
			if (is_euler_rotation) {
				value.rotation = QuaternionFromEuler(*euler_rotation).StorageLow();
			}
			else {
				value.rotation = *quaternion_rotation;
			}
		}

		return value;
	}

	// ------------------------------------------------------------------------------------------------------------

}