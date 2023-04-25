#pragma once
#include "EntityManagerSerializeTypes.h"

namespace ECSEngine {

	struct EntityManager;

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct ModuleLinkComponentTarget;
	struct AssetDatabase;
	struct DeserializeFieldTable;

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeEntityManager(
		const EntityManager* entity_manager,
		Stream<wchar_t> filename,
		const SerializeEntityManagerComponentTable* component_table,
		const SerializeEntityManagerSharedComponentTable* shared_component_table
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It does not close the file handle if it fails. It leave it open
	ECSENGINE_API bool SerializeEntityManager(
		const EntityManager* entity_manager,
		ECS_FILE_HANDLE file_handle,
		const SerializeEntityManagerComponentTable* component_table,
		const SerializeEntityManagerSharedComponentTable* shared_component_table
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		Stream<wchar_t> filename,
		const DeserializeEntityManagerComponentTable* component_table,
		const DeserializeEntityManagerSharedComponentTable* shared_component_table
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It does not close the file handle if it fails. It leaves it open
	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		ECS_FILE_HANDLE file_handle,
		const DeserializeEntityManagerComponentTable* component_table,
		const DeserializeEntityManagerSharedComponentTable* shared_component_table
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		uintptr_t& ptr,
		const DeserializeEntityManagerComponentTable* component_table,
		const DeserializeEntityManagerSharedComponentTable* shared_component_table
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

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToDeserializeEntityManagerUnique(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
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
		Stream<DeserializeEntityManagerComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);

	ECSENGINE_API void AddDeserializeEntityManagerComponentTableOverrides(
		DeserializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<DeserializeEntityManagerComponentInfo> overrides
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// It will fill in the overrides. The module links needs to be in sync with the link types
	ECSENGINE_API void ConvertLinkTypesToDeserializeEntityManagerShared(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
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
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides = { nullptr, 0 },
		Component* override_components = nullptr,
		Stream<unsigned int> hierarchy_indices = { nullptr, 0 }
	);
	
	ECSENGINE_API void AddDeserializeEntityManagerSharedComponentTableOverrides(
		DeserializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides
	);

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