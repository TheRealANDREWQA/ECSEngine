#pragma once
#include "ModuleDefinition.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
	}

	struct AssetDatabase;

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeOverrides(Stream<const AppliedModule*> applied_modules, CapacityStream<SerializeEntityManagerComponentInfo>& infos);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules,
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>& infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherSerializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		CapacityStream<SerializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& shared_infos,
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>& global_infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules, 
		CapacityStream<DeserializeEntityManagerComponentInfo>& infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeSharedOverrides(
		Stream<const AppliedModule*> applied_modules, 
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos
	);
	
	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules,
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& infos
	);

	// ------------------------------------------------------------------------------------------------------------

	// Does not include the link components
	ECSENGINE_API void ModuleGatherDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		CapacityStream<DeserializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& shared_infos,
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& global_infos
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
	ECSENGINE_API bool ModuleGatherLinkSerializeGlobalOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>& infos,
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
		CapacityStream<SerializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<SerializeEntityManagerSharedComponentInfo>& shared_infos,
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>& global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {}
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& infos,
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
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& infos,
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
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {}
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns false if there is a link component that needs DLL support but there is no function found.
	ECSENGINE_API bool ModuleGatherLinkDeserializeAllOverrides(
		Stream<const AppliedModule*> applied_modules,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic temp_allocator,
		CapacityStream<DeserializeEntityManagerComponentInfo>& unique_infos,
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>& shared_infos,
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>& global_infos,
		Stream<ModuleLinkComponentTarget> extra_targets = {},
		Stream<ModuleComponentFunctions> component_functions = {}
	);
	 
	// ------------------------------------------------------------------------------------------------------------

	// Determines if the given debug draw elements match the state of the
	// given reflection manager. Returns true if all elements match, else false
	// Can optionally give an error string to be filled in with the description
	// of the error
	ECSENGINE_API bool ModuleValidateDebugDrawComponentsExist(
		Stream<ModuleDebugDrawElement> debug_draw_elements,
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<char>* error_message = nullptr
	);

	// ------------------------------------------------------------------------------------------------------------

	// There should be an entry in the output_elements for each value in components
	// Elements that are not matched with the debug_elements are left the same
	ECSENGINE_API void ModuleMatchDebugDrawElements(
		Stream<ComponentWithType> components,
		Stream<ModuleDebugDrawElement> match_elements,
		ModuleDebugDrawElement* output_elements
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Stream<ModuleComponentFunctions> ModuleAggregateComponentFunctions(
		Stream<const AppliedModule*> applied_modules,
		AllocatorPolymorphic allocator
	);

	// ------------------------------------------------------------------------------------------------------------

}