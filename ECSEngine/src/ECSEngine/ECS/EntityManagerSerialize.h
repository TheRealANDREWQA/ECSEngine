#pragma once
#include "EntityManagerSerializeTypes.h"

namespace ECSEngine {

	struct EntityManager;

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct ModuleLinkComponentTarget;
	struct ModuleComponentFunctions;
	struct AssetDatabase;
	struct DeserializeFieldTable;

	struct SerializeEntityManagerOptions {
		// All of them are mandatory
		const SerializeEntityManagerComponentTable* component_table;
		const SerializeEntityManagerSharedComponentTable* shared_component_table;
		const SerializeEntityManagerGlobalComponentTable* global_component_table;
	};

	struct DeserializeEntityManagerOptions {
		// ----------------------- Mandatory --------------------------------------
		const DeserializeEntityManagerComponentTable* component_table;
		const DeserializeEntityManagerSharedComponentTable* shared_component_table;
		const DeserializeEntityManagerGlobalComponentTable* global_component_table;
		// ----------------------- Mandatory --------------------------------------

		// ------------------------ Optional --------------------------------------
		// If this is true, and a component fixup is not found, then 
		// It will continue the deserialization by omitting that data
		bool remove_missing_components = false;
		CapacityStream<char>* detailed_error_string = nullptr;
		// ------------------------ Optional --------------------------------------
	};

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeEntityManager(
		const EntityManager* entity_manager,
		Stream<wchar_t> filename,
		SerializeEntityManagerOptions* options
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It does not close the file handle if it fails. It leave it open
	ECSENGINE_API bool SerializeEntityManager(
		const EntityManager* entity_manager,
		ECS_FILE_HANDLE file_handle,
		SerializeEntityManagerOptions* options
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		Stream<wchar_t> filename,
		DeserializeEntityManagerOptions* options
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It does not close the file handle if it fails. It leaves it open
	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		ECS_FILE_HANDLE file_handle,
		DeserializeEntityManagerOptions* options
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		uintptr_t& ptr,
		DeserializeEntityManagerOptions* options
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToSerializeEntityManagerUnique(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		SerializeEntityManagerComponentInfo* overrides
	);

	// Creates and allocates all the necesarry handlers for all the reflected types
	// If the indices are unspecified, it will go through all hierarchies
	// Can specify overrides such that they get ignored when searching. Both overrides and override_components
	// need to be specified in sync if the overrides don't contain the name of the type
	// It doesn't add the overrides!
	ECSENGINE_API void CreateSerializeEntityManagerComponentTable(
		SerializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);

	ECSENGINE_API void AddSerializeEntityManagerComponentTableOverrides(
		SerializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<SerializeEntityManagerComponentInfo> overrides
	);

	ECS_INLINE void CreateSerializeEntityManagerComponentTableAddOverrides(
		SerializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerComponentInfo> overrides,
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	) {
		CreateSerializeEntityManagerComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices);
		if (overrides.size > 0) {
			AddSerializeEntityManagerComponentTableOverrides(table, reflection_manager, overrides);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToSerializeEntityManagerShared(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		SerializeEntityManagerSharedComponentInfo* overrides
	);

	// Creates and allocates all the necesarry handlers for all the reflected types
	// If the indices are unspecified, it will go through all hierarchies
	// Can specify overrides such that they get ignored when searching. Both overrides and override_components
	// need to be specified in sync if the overrides don't contain the name of the type
	// It doesn't add the overrides!
	ECSENGINE_API void CreateSerializeEntityManagerSharedComponentTable(
		SerializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerSharedComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);

	ECSENGINE_API void AddSerializeEntityManagerSharedComponentTableOverrides(
		SerializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<SerializeEntityManagerSharedComponentInfo> overrides
	);

	ECS_INLINE void CreateSerializeEntityManagerSharedComponentTableAddOverrides(
		SerializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerSharedComponentInfo> overrides,
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	) {
		CreateSerializeEntityManagerSharedComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices);
		if (overrides.size > 0) {
			AddSerializeEntityManagerSharedComponentTableOverrides(table, reflection_manager, overrides);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToSerializeEntityManagerGlobal(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		SerializeEntityManagerGlobalComponentInfo* overrides
	);

	// Creates and allocates all the necesarry handlers for all the reflected types
	// If the indices are unspecified, it will go through all hierarchies
	// Can specify overrides such that they get ignored when searching. Both overrides and override_components
	// need to be specified in sync if the overrides don't contain the name of the type
	// It doesn't add the overrides!
	ECSENGINE_API void CreateSerializeEntityManagerGlobalComponentTable(
		SerializeEntityManagerGlobalComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerGlobalComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);

	ECSENGINE_API void AddSerializeEntityManagerGlobalComponentTableOverrides(
		SerializeEntityManagerGlobalComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<SerializeEntityManagerGlobalComponentInfo> overrides
	);

	ECS_INLINE void CreateSerializeEntityManagerGlobalComponentTableAddOverrides(
		SerializeEntityManagerGlobalComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerGlobalComponentInfo> overrides,
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	) {
		CreateSerializeEntityManagerGlobalComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices);
		if (overrides.size > 0) {
			AddSerializeEntityManagerGlobalComponentTableOverrides(table, reflection_manager, overrides);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToDeserializeEntityManagerUnique(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		Stream<ModuleComponentFunctions> module_component_functions,
		DeserializeEntityManagerComponentInfo* overrides
	);

	// Creates and allocates all the necesarry handlers for all the reflected types
	// If the indices are unspecified, it will go through all hierarchies
	// Can specify overrides such that they get ignored when searching. Both overrides and override_components
	// need to be specified in sync if the overrides don't contain the name of the type
	// It doesn't add the overrides!
	ECSENGINE_API void CreateDeserializeEntityManagerComponentTable(
		DeserializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<ModuleComponentFunctions> module_component_functions,
		Stream<DeserializeEntityManagerComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);

	ECSENGINE_API void AddDeserializeEntityManagerComponentTableOverrides(
		DeserializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<DeserializeEntityManagerComponentInfo> overrides
	);

	ECS_INLINE void CreateDeserializeEntityManagerComponentTableAddOverrides(
		DeserializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<ModuleComponentFunctions> module_component_functions,
		Stream<DeserializeEntityManagerComponentInfo> overrides,
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	) {
		CreateDeserializeEntityManagerComponentTable(table, reflection_manager, allocator, module_component_functions, overrides, override_components, hierarchy_indices);
		if (overrides.size > 0) {
			AddDeserializeEntityManagerComponentTableOverrides(table, reflection_manager, overrides);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToDeserializeEntityManagerShared(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		Stream<ModuleComponentFunctions> module_component_functions,
		DeserializeEntityManagerSharedComponentInfo* overrides
	);

	// Creates and allocates all the necesarry handlers for all the reflected types
	// If the indices are unspecified, it will go through all hierarchies
	// Can specify overrides such that they get ignored when searching. Both overrides and override_components
	// need to be specified in sync if the overrides don't contain the name of the type
	// It doesn't add the overrides!
	ECSENGINE_API void CreateDeserializeEntityManagerSharedComponentTable(
		DeserializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<ModuleComponentFunctions> module_component_functions,
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);
	
	ECSENGINE_API void AddDeserializeEntityManagerSharedComponentTableOverrides(
		DeserializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides
	);

	ECS_INLINE void CreateDeserializeEntityManagerSharedComponentTableAddOverrides(
		DeserializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<ModuleComponentFunctions> module_component_functions,
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides,
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	) {
		CreateDeserializeEntityManagerSharedComponentTable(table, reflection_manager, allocator, module_component_functions, overrides, override_components, hierarchy_indices);
		if (overrides.size > 0) {
			AddDeserializeEntityManagerSharedComponentTableOverrides(table, reflection_manager, overrides);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToDeserializeEntityManagerGlobal(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		DeserializeEntityManagerGlobalComponentInfo* overrides
	);

	// Creates and allocates all the necesarry handlers for all the reflected types
	// If the indices are unspecified, it will go through all hierarchies
	// Can specify overrides such that they get ignored when searching. Both overrides and override_components
	// need to be specified in sync if the overrides don't contain the name of the type
	// It doesn't add the overrides!
	ECSENGINE_API void CreateDeserializeEntityManagerGlobalComponentTable(
		DeserializeEntityManagerGlobalComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<DeserializeEntityManagerGlobalComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);

	ECSENGINE_API void AddDeserializeEntityManagerGlobalComponentTableOverrides(
		DeserializeEntityManagerGlobalComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<DeserializeEntityManagerGlobalComponentInfo> overrides
	);

	ECS_INLINE void CreateDeserializeEntityManagerGlobalComponentTableAddOverrides(
		DeserializeEntityManagerGlobalComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<DeserializeEntityManagerGlobalComponentInfo> overrides,
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	) {
		CreateDeserializeEntityManagerGlobalComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices);
		if (overrides.size > 0) {
			AddDeserializeEntityManagerGlobalComponentTableOverrides(table, reflection_manager, overrides);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------

	// For all component types that previously had a link type and now they don't or if they previously lacked
	// a link type and now they do have we will convert the type from the deserialize table into the according type 
	// in order to deserialize all the fields that can be read. The names will reference already existing strings
	ECSENGINE_API void MirrorDeserializeEntityManagerLinkTypes(
		const Reflection::ReflectionManager* current_reflection_manager, 
		DeserializeFieldTable* field_table
	);

	// -------------------------------------------------------------------------------------------------------------------------------------
	
}