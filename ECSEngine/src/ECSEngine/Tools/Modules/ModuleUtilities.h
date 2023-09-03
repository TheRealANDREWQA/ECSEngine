#pragma once
#include "ModuleDefinition.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
	}

	struct AssetDatabase;

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the linked components
	ECSENGINE_API void ModuleGatherSerializeOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<SerializeEntityManagerComponentInfo>& infos);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the linked components
	ECSENGINE_API void ModuleGatherSerializeSharedOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<SerializeEntityManagerSharedComponentInfo>& infos);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the linked components
	ECSENGINE_API void ModuleGatherDeserializeOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<DeserializeEntityManagerComponentInfo>& infos);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the linked components
	ECSENGINE_API void ModuleGatherDeserializeSharedOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos);

	// ------------------------------------------------------------------------------------------------------------

	// Returns an empty target if there is no match
	ECSENGINE_API ModuleLinkComponentTarget GetModuleLinkComponentTarget(const AppliedModule* applied_module, Stream<char> name);

	// ------------------------------------------------------------------------------------------------------------

	// Returns an empty target if there is no match
	ECSENGINE_API ModuleLinkComponentTarget GetModuleLinkComponentTarget(
		Stream<const AppliedModule*> applied_module, 
		Stream<char> name, 
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------
	
	// Returns false if there is a link component that needs DLL support but there is no function found.
	// Can optionally give some extra convert targets without being in the modules
	ECSENGINE_API bool ModuleGatherLinkSerializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	// Can optionally give some extra convert targets without being in the modules
	ECSENGINE_API bool ModuleGatherLinkSerializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	// Can optionally give some extra convert targets without being in the modules
	ECSENGINE_API bool ModuleGatherLinkSerializeUniqueAndSharedOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& shared_infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool ModuleGatherLinkDeserializeUniqueAndSharedOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& shared_infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);
	 
	// ------------------------------------------------------------------------------------------------------------
}