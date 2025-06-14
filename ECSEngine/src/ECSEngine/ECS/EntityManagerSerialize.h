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
		// If this option is set to true, then it will skip registering the components.
		// Helpful if you want to deserialize into an entity manager that already has the
		// Components registered.
		bool do_not_register_components = false;
		CapacityStream<char>* detailed_error_string = nullptr;
		// ------------------------ Optional --------------------------------------
	};

	struct FileWriteInstrumentTarget;
	struct FileReadInstrumentTarget;

	// -------------------------------------------------------------------------------------------------------------------------------------

	// Writes the header portion of a entity manager serialize. This encompasses what components
	// There are in the manager, their names, their byte sizes and their individual own headers.
	// With the last boolean you can choose to disable including the write of the shared component instances.
	// If the buffering argument is provided, it will use that buffering in order to write the values first and
	// Commit all the data at once in the write instrument, else it will use an internal buffering
	ECSENGINE_API bool SerializeEntityManagerHeaderSection(
		const EntityManager* entity_manager,
		WriteInstrument* write_instrument,
		const SerializeEntityManagerOptions* options,
		CapacityStream<void>* buffering = nullptr
	);

	ECSENGINE_API bool SerializeEntityManager(
		const EntityManager* entity_manager,
		const FileWriteInstrumentTarget& write_target,
		const SerializeEntityManagerOptions* options
	);

	// The same as the other overload, but it serializes only the data itself, without the header
	// Section, in case that data was written already. It must receive a buffering parameter where
	// Some data is accumulated before committing into the write instrument
	ECSENGINE_API bool SerializeEntityManager(
		const EntityManager* entity_manager,
		WriteInstrument* write_instrument,
		CapacityStream<void>& buffering,
		const SerializeEntityManagerOptions* options
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// Opaque structure that is handled internally and must be passed around to the functions that know how to use it
	// It allocates memory from an allocator, but there is currently no deallocation method for it, just provide
	// A temporary allocator for this reason
	namespace internal {
		// We are not storing the allocator size because the type could have changed since the serialization. 
		// Push the responsability onto the serialize function to give us these parameters
		struct SerializedComponentInfo {
			unsigned int header_data_size;
			unsigned short component_byte_size;
			Component component;
			unsigned char serialize_version;
			unsigned char name_size;
		};

		struct SerializedSharedComponentInstanceInfo {
			// The component is stored here as well, since the shared components
			// From the main header might not be the same as in the override
			Component component;
			unsigned short instance_count;
			unsigned short named_instance_count;
		};

		struct SerializedCachedComponentInfo {
			const DeserializeEntityManagerComponentInfo* info;
			Component found_at;
			unsigned char version;
		};

		struct SerializedCachedSharedComponentInfo {
			ECS_INLINE SerializedCachedSharedComponentInfo() {}
			ECS_INLINE SerializedCachedSharedComponentInfo(SerializedCachedComponentInfo simple_info) {
				info = (decltype(info))simple_info.info;
				found_at = simple_info.found_at;
				version = simple_info.version;
			}

			const DeserializeEntityManagerSharedComponentInfo* info;
			Component found_at;
			unsigned char version;
		};

		typedef SerializedCachedComponentInfo SerializedCachedGlobalComponentInfo;

		struct SerializedNamedSharedInstanceHeader {
			SharedInstance instance;
			unsigned short identifier_size;
		};
	}

	// This is the main header of the serialization, that can be used standalone or be shared across multiple serializations.
	// If used across multiple serializations, the archetype count/global_component_count/shared_component_count variable should be ignored, 
	// they are provided by the override header.
	struct SerializeEntityManagerHeader {
		unsigned int version;
		unsigned short component_count;
		unsigned short shared_component_count;
		unsigned short global_component_count;

		// These are additional bytes that can be used in the future to add stuff to the header 
		// without having to invalidate the older version - these bytes need to be set to 0 in versions
		// That don't use them
		unsigned int reserved[8];
	};

	// A header that is used to override fields from the main header when a header write is used across multiple serializations.
	struct SerializeEntityManagerLocalHeader {
		unsigned int version;
		unsigned int archetype_count;
		// We need this count to know to deserialize the current instance counts
		unsigned short shared_component_count;
		// We need this count to know to deserialize the specific current global components
		unsigned short global_component_count;
	};

	// This is data that is serialized using SerializeEntityManagerHeaderSection and deserialized using
	// DeserializeEntityManagerHeaderSection. It can be used across multiple serializations to avoid having
	// To write the same data multiple times.
	struct ECSENGINE_API DeserializeEntityManagerHeaderSectionData {
		ECS_INLINE size_t ComponentCount() const {
			return component_pairs.size;
		}

		ECS_INLINE size_t SharedComponentCount() const {
			return shared_component_pairs.size;
		}

		ECS_INLINE size_t GlobalComponentCount() const {
			return global_component_pairs.size;
		}

		//ECS_INLINE Stream<char> GetComponentNameByIteration(size_t index) const {
		//	return { OffsetPointer(unique_name_characters, unique_name_offsets[index]), component_pairs[index].name_size };
		//}

		// Looks up the name of the component using the cached unique infos. If the component could not be found,
		// Then it will return a string allocated from the provided allocator with the index value of the component stringified
		ECS_INLINE Stream<char> GetComponentNameLookup(Component component, AllocatorPolymorphic allocator) const {
			const internal::SerializedCachedComponentInfo* info = cached_unique_infos.TryGetValuePtr(component);
			return info == nullptr ? info->info->name : component.ToString(allocator);
		}

		//ECS_INLINE Stream<char> GetSharedComponentNameByIteration(size_t index) const {
		//	return { OffsetPointer(shared_name_characters, shared_name_offsets[index]), shared_component_pairs[index].name_size };
		//}

		// Looks up the name of the component using the cached shared infos. If the component could not be found,
		// Then it will return a string allocated from the provided allocator with the index value of the component stringified
		ECS_INLINE Stream<char> GetSharedComponentNameLookup(Component component, AllocatorPolymorphic allocator) const {
			const internal::SerializedCachedSharedComponentInfo* info = cached_shared_infos.TryGetValuePtr(component);
			return info == nullptr ? info->info->name : component.ToString(allocator);
		}

		//ECS_INLINE Stream<char> GetGlobalComponentNameByIteration(size_t index) const {
		//	return { OffsetPointer(global_name_characters, global_name_offsets[index]), global_component_pairs[index].name_size };
		//}

		// Looks up the name of the component using the cached global infos. If the component could not be found,
		// Then it will return a string allocated from the provided allocator with the index value of the component stringified
		ECS_INLINE Stream<char> GetGlobalComponentNameLookup(Component component, AllocatorPolymorphic allocator) const {
			const internal::SerializedCachedGlobalComponentInfo* info = cached_global_infos.TryGetValuePtr(component);
			return info == nullptr ? info->info->name : component.ToString(allocator);
		}

		SerializeEntityManagerHeader header;

		Stream<internal::SerializedComponentInfo> component_pairs;
		Stream<internal::SerializedComponentInfo> shared_component_pairs;
		Stream<internal::SerializedComponentInfo> global_component_pairs;

		HashTable<internal::SerializedCachedComponentInfo, Component, HashFunctionPowerOfTwo> cached_unique_infos;
		HashTable<internal::SerializedCachedSharedComponentInfo, Component, HashFunctionPowerOfTwo> cached_shared_infos;
		HashTable<internal::SerializedCachedGlobalComponentInfo, Component, HashFunctionPowerOfTwo> cached_global_infos;

		// These arrays are parallel to component pairs
		const char* unique_name_characters;
		const char* shared_name_characters;
		const char* global_name_characters;

		// These arrays are parallel to component pairs
		unsigned int* unique_name_offsets;
		unsigned int* shared_name_offsets;
		unsigned int* global_name_offsets;
	};

	ECSENGINE_API Optional<DeserializeEntityManagerHeaderSectionData> DeserializeEntityManagerHeaderSection(
		ReadInstrument* read_instrument,
		AllocatorPolymorphic temporary_allocator,
		const DeserializeEntityManagerOptions* options
	);

	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		const FileReadInstrumentTarget& read_target,
		const DeserializeEntityManagerOptions* options
	);

	// The same as the other overload, but it receives the header as a parameter,
	// In case it was written separately. The temporary allocator should be around MBs
	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		ReadInstrument* read_instrument,
		const DeserializeEntityManagerHeaderSectionData& header_section,
		const DeserializeEntityManagerOptions* options,
		ResizableLinearAllocator* temporary_allocator
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