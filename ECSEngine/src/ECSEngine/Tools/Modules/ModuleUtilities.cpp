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
			for (size_t subindex = 0; subindex < current_infos.size; subindex++) {
				infos.AddAssert(current_infos[index]);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherSerializeSharedOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<SerializeEntityManagerSharedComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<SerializeEntityManagerSharedComponentInfo> current_infos = applied_modules[index]->serialize_streams.serialize_shared_components;
			for (size_t subindex = 0; subindex < current_infos.size; subindex++) {
				infos.AddAssert(current_infos[index]);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<DeserializeEntityManagerComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_components;
			for (size_t subindex = 0; subindex < current_infos.size; subindex++) {
				infos.AddAssert(current_infos[index]);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ModuleGatherDeserializeSharedOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos)
	{
		for (size_t index = 0; index < applied_modules.size; index++) {
			Stream<DeserializeEntityManagerSharedComponentInfo> current_infos = applied_modules[index]->serialize_streams.deserialize_shared_components;
			for (size_t subindex = 0; subindex < current_infos.size; subindex++) {
				infos.AddAssert(current_infos[index]);
			}
		}
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

	ModuleLinkComponentTarget GetModuleLinkComponentTarget(Stream<const AppliedModule*> applied_module, Stream<char> name)
	{
		for (size_t index = 0; index < applied_module.size; index++) {
			ModuleLinkComponentTarget target = GetModuleLinkComponentTarget(applied_module[index], name);
			if (target.build_function != nullptr) {
				return target;
			}
		}
		return { nullptr, nullptr };
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename LinkFunctor, typename ConvertFunctor>
	bool ModuleGatherLinkOverridesImpl(
		Stream<const AppliedModule*> applied_modules,
		LinkFunctor&& link_functor,
		ConvertFunctor&& convert_functor
	) {
		ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, unique_link_types, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, shared_link_types, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(ModuleLinkComponentTarget, unique_link_type_targets, ECS_KB);
		ECS_STACK_CAPACITY_STREAM(ModuleLinkComponentTarget, shared_link_type_targets, ECS_KB);

		link_functor(unique_link_types, shared_link_types);

		auto iterate = [applied_modules](Stream<const Reflection::ReflectionType*> link_types, CapacityStream<ModuleLinkComponentTarget>& link_type_targets) {
			for (size_t index = 0; index < link_types.size; index++) {
				bool needs_dll = GetReflectionTypeLinkComponentNeedsDLL(link_types[index]);
				if (needs_dll) {
					ModuleLinkComponentTarget target = GetModuleLinkComponentTarget(applied_modules, link_types[index]->name);
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

		convert_functor(unique_link_types, shared_link_types, unique_link_type_targets, shared_link_type_targets);

		return true;
	}

	bool ModuleGatherLinkSerializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerComponentInfo>& infos
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules,
			[=](auto& unique_link_types, auto& shared_link_types) {
				GetUniqueLinkComponents(reflection_manager, unique_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto unique_link_type_targets, auto shared_link_type_targets) {
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
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& infos
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules,
			[=](auto& unique_link_types, auto& shared_link_types) {
				GetSharedLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto unique_link_type_targets, auto shared_link_type_targets) {
				ECS_ASSERT(infos.size + shared_link_types.size <= infos.capacity);
				ConvertLinkTypesToSerializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, infos.buffer + infos.size);
				infos.size += shared_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkSerializeUniqueAndSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerComponentInfo>& unique_infos, 
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& shared_infos
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules,
			[=](auto& unique_link_types, auto& shared_link_types) {
				GetUniqueAndSharedLinkComponents(reflection_manager, unique_link_types, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto unique_link_type_targets, auto shared_link_type_targets) {
				ECS_ASSERT(unique_infos.size + unique_link_types.size <= unique_infos.capacity);
				ECS_ASSERT(shared_infos.size + shared_link_types.size <= shared_infos.capacity);

				ConvertLinkTypesToSerializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, unique_infos.buffer + unique_infos.size);
				ConvertLinkTypesToSerializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, shared_infos.buffer + shared_infos.size);
				unique_infos.size += unique_link_types.size;
				shared_infos.size += shared_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& infos
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules,
			[=](auto& unique_link_types, auto& shared_link_types) {
				GetUniqueLinkComponents(reflection_manager, unique_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto unique_link_type_targets, auto shared_link_type_targets) {
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
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules,
			[=](auto& unique_link_types, auto& shared_link_types) {
				GetSharedLinkComponents(reflection_manager, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto unique_link_type_targets, auto shared_link_type_targets) {
				ECS_ASSERT(infos.size + shared_link_types.size <= infos.capacity);
				ConvertLinkTypesToDeserializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, infos.buffer + infos.size);
				infos.size += shared_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ModuleGatherLinkDeserializeUniqueAndSharedOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& shared_infos
	)
	{
		return ModuleGatherLinkOverridesImpl(applied_modules,
			[=](auto& unique_link_types, auto& shared_link_types) {
				GetUniqueAndSharedLinkComponents(reflection_manager, unique_link_types, shared_link_types);
			},
			[&](auto unique_link_types, auto shared_link_types, auto unique_link_type_targets, auto shared_link_type_targets) {
				ECS_ASSERT(unique_infos.size + unique_link_types.size <= unique_infos.capacity);
				ECS_ASSERT(shared_infos.size + shared_link_types.size <= shared_infos.capacity);

				ConvertLinkTypesToDeserializeEntityManagerUnique(reflection_manager, database, temp_allocator, unique_link_types, unique_link_type_targets, unique_infos.buffer + unique_infos.size);
				ConvertLinkTypesToDeserializeEntityManagerShared(reflection_manager, database, temp_allocator, shared_link_types, shared_link_type_targets, shared_infos.buffer + shared_infos.size);
				unique_infos.size += unique_link_types.size;
				shared_infos.size += shared_link_types.size;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

}