#include "ecspch.h"
#include "ModuleUtilities.h"
#include "../../ECS/LinkComponents.h"
#include "../../ECS/ComponentHelpers.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../ECS/EntityManagerSerialize.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeOverrides(Stream<const AppliedModule*> applied_modules, AdditionStream<SerializeEntityManagerComponentInfo> infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<SerializeEntityManagerComponentInfo> current_infos = applied_modules[index]->serialize_streams.serialize_components;
			infos.AddStream(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeSharedOverrides(Stream<const AppliedModule*> applied_modules, AdditionStream<SerializeEntityManagerSharedComponentInfo> infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<SerializeEntityManagerSharedComponentInfo> current_infos = applied_modules[index]->serialize_streams.serialize_shared_components;
			infos.AddStream(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeGlobalOverrides(Stream<const AppliedModule*> applied_modules, AdditionStream<SerializeEntityManagerGlobalComponentInfo> infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<SerializeEntityManagerGlobalComponentInfo> current_infos = applied_modules[index]->serialize_streams.serialize_global_components;
			infos.AddStream(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeAllOverrides(
		Stream<const AppliedModule*> applied_modules, 
		AdditionStream<SerializeEntityManagerComponentInfo> unique_infos, 
		AdditionStream<SerializeEntityManagerSharedComponentInfo> shared_infos, 
		AdditionStream<SerializeEntityManagerGlobalComponentInfo> global_infos
	)
	{
		ModuleGatherSerializeOverrides(applied_modules, unique_infos);
		ModuleGatherSerializeSharedOverrides(applied_modules, shared_infos);
		ModuleGatherSerializeGlobalOverrides(applied_modules, global_infos);
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeOverrides(Stream<const AppliedModule*> applied_modules, AdditionStream<DeserializeEntityManagerComponentInfo> infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_components;
			infos.AddStream(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeSharedOverrides(Stream<const AppliedModule*> applied_modules, AdditionStream<DeserializeEntityManagerSharedComponentInfo> infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerSharedComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_shared_components;
			infos.AddStream(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeGlobalOverrides(Stream<const AppliedModule*> applied_modules, AdditionStream<DeserializeEntityManagerGlobalComponentInfo> infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerGlobalComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_global_components;
			infos.AddStream(current_infos);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules, 
		AdditionStream<DeserializeEntityManagerComponentInfo> unique_infos, 
		AdditionStream<DeserializeEntityManagerSharedComponentInfo> shared_infos, 
		AdditionStream<DeserializeEntityManagerGlobalComponentInfo> global_infos
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
			if (applied_module->link_components[index].component_name == name) {
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
				ModuleLinkComponentTarget target = GetModuleLinkComponentTarget(applied_modules, link_types[index]->name, extra_targets);
				if (target.build_function == nullptr || target.reverse_function == nullptr) {
					return false;
				}
				link_type_targets.AddAssert(target);
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
		AdditionStream<SerializeEntityManagerComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetUniqueLinkComponents(reflection_manager, unique_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToSerializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, infos.Reserve(unique_link_types.size));
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkSerializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<SerializeEntityManagerSharedComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetSharedLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToSerializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, infos.Reserve(shared_link_types.size));
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkSerializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database, 
		AllocatorPolymorphic temp_allocator, 
		AdditionStream<SerializeEntityManagerGlobalComponentInfo> infos, 
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetGlobalLinkComponents(reflection_manager, global_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToSerializeEntityManagerGlobal(reflection_manager, database, temp_allocator, global_link_types, global_link_type_targets, infos.Reserve(global_link_types.size));
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkSerializeAllOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<SerializeEntityManagerComponentInfo> unique_infos, 
		AdditionStream<SerializeEntityManagerSharedComponentInfo> shared_infos,
		AdditionStream<SerializeEntityManagerGlobalComponentInfo> global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetAllLinkComponents(reflection_manager, unique_link_types, shared_link_types, global_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToSerializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, unique_infos.Reserve(unique_link_types.size));
				ConvertLinkTypesToSerializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, shared_infos.Reserve(shared_link_types.size));
				ConvertLinkTypesToSerializeEntityManagerGlobal(reflection_manager, database, temp_allocator, global_link_types, global_link_type_targets, global_infos.Reserve(global_link_types.size));
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<DeserializeEntityManagerComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets,
		Stream<ModuleComponentFunctions> component_functions
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetUniqueLinkComponents(reflection_manager, unique_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToDeserializeEntityManagerUnique(
					reflection_manager, 
					database, 
					temp_allocator, 
					unique_link_types, 
					unique_link_type_targets,
					component_functions,
					infos.Reserve(unique_link_types.size)
				);
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<DeserializeEntityManagerSharedComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets,
		Stream<ModuleComponentFunctions> component_functions
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetSharedLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToDeserializeEntityManagerShared(
					reflection_manager,
					database,
					temp_allocator,
					shared_link_types,
					shared_link_type_targets,
					component_functions,
					infos.Reserve(shared_link_types.size)
				);
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database, 
		AllocatorPolymorphic temp_allocator, 
		AdditionStream<DeserializeEntityManagerGlobalComponentInfo> infos, 
		Stream<ModuleLinkComponentTarget> extra_targets
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetGlobalLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToDeserializeEntityManagerGlobal(
					reflection_manager, 
					database, 
					temp_allocator, 
					global_link_types, 
					global_link_type_targets, 
					infos.Reserve(global_link_types.size)
				);
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<DeserializeEntityManagerComponentInfo> unique_infos,
		AdditionStream<DeserializeEntityManagerSharedComponentInfo> shared_infos,
		AdditionStream<DeserializeEntityManagerGlobalComponentInfo> global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets,
		Stream<ModuleComponentFunctions> component_functions
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules, extra_targets,
			[=](auto& unique_link_types, auto& shared_link_types, auto& global_link_types) {
				GetAllLinkComponents(reflection_manager, unique_link_types, shared_link_types, global_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto global_link_types, auto unique_link_type_targets, auto shared_link_type_targets, auto global_link_type_targets) {
				ConvertLinkTypesToDeserializeEntityManagerUnique(
					reflection_manager, 
					database, 
					temp_allocator, 
					unique_link_types, 
					unique_link_type_targets, 
					component_functions,
					unique_infos.Reserve(unique_link_types.size)
				);
				ConvertLinkTypesToDeserializeEntityManagerShared(
					reflection_manager, 
					database, 
					temp_allocator, 
					shared_link_types, 
					shared_link_type_targets, 
					component_functions,
					shared_infos.Reserve(shared_link_types.size)
				);
				ConvertLinkTypesToDeserializeEntityManagerGlobal(
					reflection_manager, 
					database, 
					temp_allocator, 
					global_link_types, 
					global_link_type_targets, 
					global_infos.Reserve(global_link_types.size)
				);
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<ModuleComponentFunctions> ModuleAggregateComponentFunctions(Stream<const AppliedModule*> applied_modules, AllocatorPolymorphic allocator)
	{
		Stream<ModuleComponentFunctions> result;

		size_t total_size = 0;
		for (size_t index = 0; index < applied_modules.size; index++) {
			total_size += applied_modules[index]->component_functions.size;
		}

		result.Initialize(allocator, total_size);
		result.size = 0;
		for (size_t index = 0; index < applied_modules.size; index++) {
			result.AddStream(applied_modules[index]->component_functions);
		}
		return result;
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleRetrieveComponentBuildDependentEntries(
		Stream<const AppliedModule*> applied_modules,
		Stream<char> component_name,
		CapacityStream<Stream<char>>* dependent_components
	) {
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<ModuleComponentFunctions> component_functions = applied_modules[index]->component_functions;
			for (size_t subindex = 0; subindex < component_functions.size; subindex++) {
				if (component_functions[subindex].build_entry.component_dependencies.Find(component_name) != -1) {
					dependent_components->AddAssert(component_functions[subindex].component_name);
				}
			}
		}
	}

	Stream<Stream<char>> ModuleRetrieveComponentBuildDependentEntries(
		Stream<const AppliedModule*> applied_modules,
		Stream<char> component_name,
		AllocatorPolymorphic allocator
	) {
		ECS_STACK_CAPACITY_STREAM(Stream<char>, dependent_components, 1024);
		ModuleRetrieveComponentBuildDependentEntries(applied_modules, component_name, &dependent_components);
		return dependent_components.Copy(allocator);
	}

	// ------------------------------------------------------------------------------------------------------------

}