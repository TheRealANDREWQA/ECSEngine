#include "ecspch.h"
#include "ModuleUtilities.h"
#include "../../ECS/LinkComponents.h"
#include "../../ECS/ComponentHelpers.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../ECS/EntityManagerSerialize.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<SerializeEntityManagerComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<SerializeEntityManagerComponentInfo> current_infos = applied_modules[index]->serialize_streams.serialize_components;
			infos.AddStreamAssert(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeSharedOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<SerializeEntityManagerSharedComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<SerializeEntityManagerSharedComponentInfo> current_infos = applied_modules[index]->serialize_streams.serialize_shared_components;
			infos.AddStreamAssert(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeGlobalOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<SerializeEntityManagerGlobalComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<SerializeEntityManagerGlobalComponentInfo> current_infos = applied_modules[index]->serialize_streams.serialize_global_components;
			infos.AddStreamAssert(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeAllOverrides(
		Stream<const AppliedModule*> applied_modules, 
		CapacityStream<SerializeEntityManagerComponentInfo>& unique_infos, 
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& shared_infos, 
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>& global_infos
	)
	{
		ModuleGatherSerializeOverrides(applied_modules, unique_infos);
		ModuleGatherSerializeSharedOverrides(applied_modules, shared_infos);
		ModuleGatherSerializeGlobalOverrides(applied_modules, global_infos);
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<DeserializeEntityManagerComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_components;
			infos.AddStreamAssert(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeSharedOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerSharedComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_shared_components;
			infos.AddStreamAssert(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeGlobalOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerGlobalComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_global_components;
			infos.AddStreamAssert(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules, 
		CapacityStream<DeserializeEntityManagerComponentInfo>& unique_infos, 
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& shared_infos, 
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& global_infos
	)
	{
		ModuleGatherDeserializeOverrides(applied_modules, unique_infos);
		ModuleGatherDeserializeSharedOverrides(applied_modules, shared_infos);
		ModuleGatherDeserializeGlobalOverrides(applied_modules, global_infos);
	}

	// ------------------------------------------------------------------------------------------------------------

	ModuleLinkComponentTarget GetModuleLinkComponentTarget(const AppliedModule* applied_module, Stream<char> name)
	{
		for (size_t index = 0; index < applied_module->link_components.size; index++) {
			if (function::CompareStrings(applied_module->link_components[index].component_name, name)) {
				return applied_module->link_components[index];
			}
		}
		return { nullptr, nullptr };
	}

	// ------------------------------------------------------------------------------------------------------------

	ModuleLinkComponentTarget GetModuleLinkComponentTarget(
		Stream<const AppliedModule*> applied_module, 
		Stream<char> name, 
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		for (size_t index = 0; index < applied_module.size; index++) {
			ModuleLinkComponentTarget target = GetModuleLinkComponentTarget(applied_module[index], name);
			if (target.build_function != nullptr) {
				return target;
			}
		}
		for (size_t index = 0; index < extra_targets.size; index++) {
			ModuleLinkComponentTarget current_target = extra_targets[index];
			if (current_target.component_name == name) {
				return current_target;
			}
		}
		return { nullptr, nullptr };
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename LinkFunctor, typename ConvertFunctor>
	static bool ModuleGatherLinkOverridesImpl(
		Stream<const AppliedModule*> applied_modules,
		Stream<ModuleLinkComponentTarget> extra_targets,
		LinkFunctor&& link_functor,
		ConvertFunctor&& convert_functor
	) {
		ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, unique_link_types, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, shared_link_types, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, global_link_types, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(ModuleLinkComponentTarget, unique_link_type_targets, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(ModuleLinkComponentTarget, shared_link_type_targets, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(ModuleLinkComponentTarget, global_link_type_targets, ECS_KB);

		link_functor(unique_link_types, shared_link_types, global_link_types);

		auto iterate = [applied_modules, extra_targets](Stream<const Reflection::ReflectionType*> link_types, CapacityStream<ModuleLinkComponentTarget>& link_type_targets) {
			for (size_t index = 0; index < link_types.size; index++) {
				bool needs_dll = GetReflectionTypeLinkComponentNeedsDLL(link_types[index]);
				if (needs_dll) {
					ModuleLinkComponentTarget target = GetModuleLinkComponentTarget(applied_modules, link_types[index]->name, extra_targets);
					if (target.build_function == nullptr || target.reverse_function == nullptr) {
						return false;
					}
					link_type_targets.AddAssert(target);
				}
				else {
					link_type_targets.AddAssert({ nullptr, nullptr });
				}
			}
			return true;
		};

		if (!iterate(unique_link_types, unique_link_type_targets)) {
			return false;
		}

		if (!iterate(shared_link_types, shared_link_type_targets)) {
			return false;
		}

		if (!iterate(global_link_types, global_link_type_targets)) {
			return false;
		}

		convert_functor(unique_link_types, shared_link_types, global_link_types, unique_link_type_targets, shared_link_type_targets, global_link_type_targets);
		return true;
	}

	bool ModuleGatherLinkSerializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetUniqueLinkComponents(reflection_manager, unique_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(infos.size + unique_link_types.size <= infos.capacity);
				ConvertLinkTypesToSerializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, infos.buffer + infos.size);
				infos.size += unique_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkSerializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetSharedLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(infos.size + shared_link_types.size <= infos.capacity);
				ConvertLinkTypesToSerializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, infos.buffer + infos.size);
				infos.size += shared_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkSerializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database, 
		AllocatorPolymorphic temp_allocator, 
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>& infos, 
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetGlobalLinkComponents(reflection_manager, global_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(infos.size + global_link_types.size <= infos.capacity);
				ConvertLinkTypesToSerializeEntityManagerGlobal(reflection_manager, database, temp_allocator, global_link_types, global_link_type_targets, infos.buffer + infos.size);
				infos.size += global_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkSerializeAllOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerComponentInfo>& unique_infos, 
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& shared_infos,
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>& global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetAllLinkComponents(reflection_manager, unique_link_types, shared_link_types, global_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(unique_infos.size + unique_link_types.size <= unique_infos.capacity);
				ECS_ASSERT(shared_infos.size + shared_link_types.size <= shared_infos.capacity);
				ECS_ASSERT(global_infos.size + global_link_types.size <= global_infos.capacity);

				ConvertLinkTypesToSerializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, unique_infos.buffer + unique_infos.size);
				ConvertLinkTypesToSerializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, shared_infos.buffer + shared_infos.size);
				ConvertLinkTypesToSerializeEntityManagerGlobal(reflection_manager, database, temp_allocator, global_link_types, global_link_type_targets, global_infos.buffer + global_infos.size);
				unique_infos.size += unique_link_types.size;
				shared_infos.size += shared_link_types.size;
				global_infos.size += global_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetUniqueLinkComponents(reflection_manager, unique_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(infos.size + unique_link_types.size <= infos.capacity);
				ConvertLinkTypesToDeserializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, infos.buffer + infos.size);
				infos.size += unique_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetSharedLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(infos.size + shared_link_types.size <= infos.capacity);
				ConvertLinkTypesToDeserializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, infos.buffer + infos.size);
				infos.size += shared_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database, 
		AllocatorPolymorphic temp_allocator, 
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& infos, 
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetGlobalLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(infos.size + global_link_types.size <= infos.capacity);
				ConvertLinkTypesToDeserializeEntityManagerGlobal(reflection_manager, database, temp_allocator, global_link_types, global_link_type_targets, infos.buffer + infos.size);
				infos.size += global_link_types.size;
			}
			);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& shared_infos,
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetAllLinkComponents(reflection_manager, unique_link_types, shared_link_types, global_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ECS_ASSERT(unique_infos.size + unique_link_types.size <= unique_infos.capacity);
				ECS_ASSERT(shared_infos.size + shared_link_types.size <= shared_infos.capacity);
				ECS_ASSERT(global_infos.size + global_link_types.size <= global_infos.capacity);

				ConvertLinkTypesToDeserializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, unique_infos.buffer + unique_infos.size);
				ConvertLinkTypesToDeserializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, shared_infos.buffer + shared_infos.size);
				ConvertLinkTypesToDeserializeEntityManagerGlobal(reflection_manager, database, temp_allocator, global_link_types, global_link_type_targets, global_infos.buffer + global_infos.size);
				unique_infos.size += unique_link_types.size;
				shared_infos.size += shared_link_types.size;
				global_infos.size += global_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleValidateDebugDrawComponentsExist(
		Stream<ModuleDebugDrawElement> debug_draw_elements, 
		const Reflection::ReflectionManager* reflection_manager, 
		CapacityStream<char>* error_message
	)
	{
		const size_t MAX_ELEMENTS_CAPACITY = ECS_KB;
		ECS_ASSERT(debug_draw_elements.size <= MAX_ELEMENTS_CAPACITY);
		// For each entry in the elements stream, keep a boolean to see if has a matching type or not
		ECS_STACK_CAPACITY_STREAM(bool, has_type, MAX_ELEMENTS_CAPACITY);
		// We can use memset to initialize to -1 everything
		memset(has_type.buffer, false, debug_draw_elements.size * sizeof(bool));
		has_type.size = debug_draw_elements.size;

		ECS_STACK_CAPACITY_STREAM(unsigned int, unique_components, ECS_KB * 8);
		ECS_STACK_CAPACITY_STREAM(unsigned int, shared_components, ECS_KB * 8);
		ECS_STACK_CAPACITY_STREAM(unsigned int, global_components, ECS_KB);

		GetHierarchyComponentTypes(reflection_manager, -1, &unique_components, &shared_components, &global_components);

		auto set_indices_for_type = [&](Stream<unsigned int> indices, ECS_COMPONENT_TYPE component_type) {
			for (unsigned int index = 0; index < indices.size; index++) {
				const Reflection::ReflectionType* type = reflection_manager->GetType(indices[index]);
				Component component = GetReflectionTypeComponent(type);
				size_t debug_index = debug_draw_elements.Find(component, [=](ModuleDebugDrawElement element) {
					if (element.component_type == component_type) {
						return element.component;
					}
					return Component(-1);
				});
				if (debug_index != -1) {
					has_type[debug_index] = true;
				}
			}
		};

		set_indices_for_type(unique_components, ECS_COMPONENT_UNIQUE);
		set_indices_for_type(shared_components, ECS_COMPONENT_SHARED);
		set_indices_for_type(global_components, ECS_COMPONENT_GLOBAL);

		// Go through each type now and check to see if it has a type
		if (error_message == nullptr) {
			return function::SearchBytes(has_type.buffer, has_type.size, false, sizeof(bool)) != -1;
		}
		else {
			bool success = true;

			size_t offset = 0;
			size_t found_index = function::SearchBytes(has_type.buffer + offset, has_type.size - offset, false, sizeof(bool));
			while (found_index != -1) {
				success = false;
				ECS_FORMAT_STRING(*error_message, "You have assigned a draw function for component {#}, type {#} which doesn't exist\n", 
					debug_draw_elements[found_index + offset].component.value, ComponentTypeToString(debug_draw_elements[found_index + offset].component_type));
				offset += found_index;
				found_index = function::SearchBytes(has_type.buffer + offset, has_type.size - offset, false, sizeof(bool));
			}

			return success;
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleMatchDebugDrawElements(
		Stream<ComponentWithType> components,
		Stream<ModuleDebugDrawElement> match_elements,
		ModuleDebugDrawElement* output_elements
	) {
		for (size_t index = 0; index < components.size; index++) {
			size_t found_index = match_elements.Find(components[index], [](const ModuleDebugDrawElement& element) {
				return ComponentWithType{ element.component, element.component_type };
				});
			if (found_index != -1) {
				output_elements[index] = match_elements[found_index];
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

}