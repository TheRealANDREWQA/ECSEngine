#pragma once
#include "ModuleDefinition.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
	}

	struct AssetDatabase;

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeOverrides(Stream<const AppliedModule*> applied_modules, AdditionStream<SerializeEntityManagerComponentInfo> infos);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		AdditionStream<SerializeEntityManagerSharedComponentInfo> infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules,
		AdditionStream<SerializeEntityManagerGlobalComponentInfo> infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		AdditionStream<SerializeEntityManagerComponentInfo> unique_infos,
		AdditionStream<SerializeEntityManagerSharedComponentInfo> shared_infos,
		AdditionStream<SerializeEntityManagerGlobalComponentInfo> global_infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		AdditionStream<DeserializeEntityManagerComponentInfo> infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		AdditionStream<DeserializeEntityManagerSharedComponentInfo> infos
	);
	
	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules,
		AdditionStream<DeserializeEntityManagerGlobalComponentInfo> infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		AdditionStream<DeserializeEntityManagerComponentInfo> unique_infos,
		AdditionStream<DeserializeEntityManagerSharedComponentInfo> shared_infos,
		AdditionStream<DeserializeEntityManagerGlobalComponentInfo> global_infos
	);

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
		AdditionStream<SerializeEntityManagerComponentInfo> infos,
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
		AdditionStream<SerializeEntityManagerSharedComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	// Can optionally give some extra convert targets without being in the modules
	ECSENGINE_API bool ModuleGatherLinkSerializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<SerializeEntityManagerGlobalComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	// Can optionally give some extra convert targets without being in the modules
	ECSENGINE_API bool ModuleGatherLinkSerializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<SerializeEntityManagerComponentInfo> unique_infos,
		AdditionStream<SerializeEntityManagerSharedComponentInfo> shared_infos,
		AdditionStream<SerializeEntityManagerGlobalComponentInfo> global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {}
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<DeserializeEntityManagerComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {},
		Stream<ModuleComponentFunctions> component_functions = {}
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<DeserializeEntityManagerSharedComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {},
		Stream<ModuleComponentFunctions> component_functions = {}
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<DeserializeEntityManagerGlobalComponentInfo> infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {}
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		AdditionStream<DeserializeEntityManagerComponentInfo> unique_infos,
		AdditionStream<DeserializeEntityManagerSharedComponentInfo> shared_infos,
		AdditionStream<DeserializeEntityManagerGlobalComponentInfo> global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {},
		Stream<ModuleComponentFunctions> component_functions = {}
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Stream<ModuleComponentFunctions> ModuleAggregateComponentFunctions(
		Stream<const AppliedModule*> applied_modules,
		AllocatorPolymorphic allocator
	);

	// ------------------------------------------------------------------------------------------------------------

	// For a given component, it will fill in the component names that have a build function
	// Which targets this given component as a dependency
	ECSENGINE_API void ModuleRetrieveComponentBuildDependentEntries(
		Stream<const AppliedModule*> applied_modules, 
		Stream<char> component_name,
		CapacityStream<Stream<char>>* dependent_components
	);

	// For a given component, it will fill in the component names that have a build function
	// Which targets this given component as a dependency
	// To deallocate, you can deallocate the returned buffer
	ECSENGINE_API Stream<Stream<char>> ModuleRetrieveComponentBuildDependentEntries(
		Stream<const AppliedModule*> applied_modules,
		Stream<char> component_name,
		AllocatorPolymorphic allocator
	);

	// ------------------------------------------------------------------------------------------------------------

}