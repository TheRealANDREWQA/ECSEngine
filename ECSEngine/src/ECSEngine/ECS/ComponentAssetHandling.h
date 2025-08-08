#pragma once
#include "../Core.h"
#include "../Resources/AssetMetadata.h"

namespace ECSEngine {

	struct AssetDatabase;
	struct AssetDatabaseAssetRemap;

	// When searching for asset fields in a target type and the type is a shader, the shader_type can narrow down the
	// shader type that it can accept (it can also be ECS_SHADER_TYPE_COUNT when it can accept all)
	struct ComponentAssetField {
		unsigned int field_index;
		AssetTypeEx type;
	};

	// ------------------------------------------------------------------------------------------------------------

	// Fills in the asset fields that cannot be found in the new_type from the previous_type
	ECSENGINE_API void GetComponentMissingAssetFields(
		const Reflection::ReflectionType* previous_type,
		const Reflection::ReflectionType* new_type,
		CapacityStream<ComponentAssetField>* missing_fields
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns true if it has conformant types. (for example we don't support basic arrays of ResourceView for textures)
	ECSENGINE_API bool GetAssetFieldsFromComponent(const Reflection::ReflectionType* type, CapacityStream<ComponentAssetField>& typed_handles);

	// When type is SHADER the shader_type can be used to narrow down the selection of shaders that it can accept
	struct AssetTargetFieldFromReflection {
		bool success;
		AssetTypeEx type;
		Stream<void> asset;
	};

	// Returns true if the target component has any asset fields, else false
	ECSENGINE_API bool HasAssetFieldsComponent(const Reflection::ReflectionType* type);

	// ------------------------------------------------------------------------------------------------------------
	
	// Extracts from a target type the asset fields. If the data is nullptr then no pointer will be retrieved.
	// It returns ECS_ASSET_TYPE_COUNT if there is not an asset field type. It will set the boolean flag to false if the field 
	// has been identified to be an asset field but incorrectly specified (like an array of meshes or pointer indirection to 2).
	// The asset pointer that will be filled in for the GPU resources (textures, samplers and shaders) will be the interface pointer, 
	// not the pointer to the ResourceView or SamplerState. In this way the result can be passed to ExtractLinkComponentFunctionAsset
	// class of functions to get the correct type
	ECSENGINE_API AssetTargetFieldFromReflection GetAssetTargetFieldFromReflection(const Reflection::ReflectionField& field, const void* data);

	// Only accesses the field and returns the interface/pointer to the structure
	// Returns { nullptr, -1 } if the asset is could not be retrieved
	ECSENGINE_API Stream<void> GetAssetTargetFieldFromReflection(
		const Reflection::ReflectionField& field,
		const void* data,
		ECS_ASSET_TYPE asset_type
	);

	// ------------------------------------------------------------------------------------------------------------

	enum ECS_SET_ASSET_TARGET_FIELD_RESULT : unsigned char {
		ECS_SET_ASSET_TARGET_FIELD_NONE,
		ECS_SET_ASSET_TARGET_FIELD_OK,
		ECS_SET_ASSET_TARGET_FIELD_FAILED,
		ECS_SET_ASSET_TARGET_FIELD_MATCHED
	};

	// It will try to set the field according to the given field. It will fail if the field
	// has a different type,
	ECSENGINE_API ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflection(
		const Reflection::ReflectionField& field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type
	);

	// It will try to set the field according to the given field only if the metadata matches the given
	// comparator, else it will leave it the same
	ECSENGINE_API ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflectionIfMatches(
		const Reflection::ReflectionField& field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type,
		Stream<void> comparator
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void GetComponentHandles(
		const Reflection::ReflectionType* type,
		const void* component,
		const AssetDatabase* asset_database,
		Stream<unsigned int> field_indices,
		CapacityStream<unsigned int>& handles
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void GetComponentHandles(
		const Reflection::ReflectionType* type,
		const void* component,
		const AssetDatabase* asset_database,
		Stream<ComponentAssetField> field_indices,
		CapacityStream<unsigned int>& handles
	);

	// ------------------------------------------------------------------------------------------------------------

	// There must be asset_fields.size slots in the field_data array
	ECSENGINE_API void GetComponentAssetData(
		const Reflection::ReflectionType* type,
		const void* component,
		Stream<ComponentAssetField> asset_fields,
		CapacityStream<Stream<void>>* field_data
	);

	// ------------------------------------------------------------------------------------------------------------

	// This version searches for a specific asset type from asset fields that can reference other assets
	// like materials can reference textures, samplers and shaders
	ECSENGINE_API void GetComponentAssetDataDeep(
		const Reflection::ReflectionType* type,
		const void* component,
		Stream<ComponentAssetField> asset_fields,
		const AssetDatabase* database,
		ECS_ASSET_TYPE asset_type,
		CapacityStream<Stream<void>>* field_data
	);

	// ------------------------------------------------------------------------------------------------------------

	struct EntityManager;

	struct GetAssetReferenceCountsFromEntitiesOptions {
		// If add_reference_counts_from_dependencies is set to true, then assets which have dependencies will increase
		// The reference count for the asset in the streams given
		bool add_reference_counts_to_dependencies = false;
		// If this is set to true, then shared assets will have a reference count of one instead of the per entity
		// That references it
		bool shared_instance_only_once = false;
	};

	// Determines the reference counts of the assets from the data stored in the entity manager's components
	// using the reflection types from the reflection manager. It does not convert the target data into link components
	// using DLL functions or the implicit conversion (TODO: Determine if there is a use case for this)
	// It will change the reference count for the assets accordingly in the asset database (it will also remove them if they reach 0)
	ECSENGINE_API void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		AssetDatabase* asset_database
	);

	// Determines the reference counts of the assets from the data stored in the entity manager's components
	// using the reflection types from the reflection manager. It does not convert the target data into link components
	// using DLL functions or the implicit conversion (TODO: Determine if there is a use case for this)
	// There must be a Stream<unsigned int> for each asset type (ECS_ASSET_TYPE_COUNT in total)
	// and each entry symbolizes the reference count for the asset at the given index
	// If you remove assets from the database, the reference counts are no longer conforming
	ECSENGINE_API void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		Stream<Stream<unsigned int>> asset_fields_reference_count,
		GetAssetReferenceCountsFromEntitiesOptions options = {}
	);

	// Determines the reference counts of the assets from the data stored in the entity manager's components
	// using the reflection types from the reflection manager. It does not convert the target data into link components
	// using DLL functions or the implicit conversion (TODO: Determine if there is a use case for this)
	// There must be a Stream<uint2> for each asset type (ECS_ASSET_TYPE_COUNT in total)
	// In the x component the handle for that asset is set and in the y the reference count
	ECSENGINE_API void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_databae,
		Stream<Stream<uint2>> asset_fields_reference_count,
		GetAssetReferenceCountsFromEntitiesOptions options = {}
	);

	// Prepares the Stream<Stream<unsigned int>> from a given allocator. There must be ECS_ASSET_TYPE_COUNT
	// Entries in the pointer
	ECSENGINE_API void GetAssetReferenceCountsFromEntitiesPrepare(
		Stream<Stream<unsigned int>> asset_fields_reference_counts,
		AllocatorPolymorphic allocator,
		const AssetDatabase* asset_database
	);

	// Prepares the Stream<Stream<uint2>> from a given allocator. There must be ECS_ASSET_TYPE_COUNT
	// Entries in the pointer
	ECSENGINE_API void GetAssetReferenceCountsFromEntitiesPrepare(
		Stream<Stream<uint2>> asset_fields_reference_counts,
		AllocatorPolymorphic allocator,
		const AssetDatabase* asset_database
	);

	ECSENGINE_API void DeallocateAssetReferenceCountsFromEntities(Stream<Stream<unsigned int>> reference_counts, AllocatorPolymorphic allocator);

	ECSENGINE_API void DeallocateAssetReferenceCountsFromEntities(Stream<Stream<uint2>> reference_counts, AllocatorPolymorphic allocator);

	// ------------------------------------------------------------------------------------------------------------

	typedef void (*ForEachAssetReferenceDifferenceFunctor)(ECS_ASSET_TYPE type, unsigned int handle, int reference_count_change, void* user_data);

	ECSENGINE_API void ForEachAssetReferenceDifference(
		const AssetDatabase* databae,
		Stream<Stream<unsigned int>> previous_counts,
		Stream<Stream<unsigned int>> current_counts,
		ForEachAssetReferenceDifferenceFunctor functor,
		void* functor_data
	);

	// The functor receives as arguments (ECS_ASSET_TYPE type, unsigned int handle, int reference_count_change)
	template<typename Functor>
	void ForEachAssetReferenceDifference(
		const AssetDatabase* database,
		Stream<Stream<unsigned int>> previous_counts,
		Stream<Stream<unsigned int>> current_counts,
		Functor functor
	) {
		auto wrapper = [](ECS_ASSET_TYPE type, unsigned int handle, int reference_count_change, void* user_data) {
			Functor* functor = (Functor*)user_data;
			(*functor)(type, handle, reference_count_change);
			};
		ForEachAssetReferenceDifference(database, previous_counts, current_counts, wrapper, &functor);
	}

	// ------------------------------------------------------------------------------------------------------------

	struct ComponentWithAssetFields {
		const Reflection::ReflectionType* type;
		Stream<ComponentAssetField> asset_fields;
	};

	ECSENGINE_API ComponentWithAssetFields GetComponentWithAssetFieldForComponent(
		const Reflection::ReflectionType* reflection_type,
		AllocatorPolymorphic allocator,
		Stream<ECS_ASSET_TYPE> asset_types,
		bool deep_search
	);

	// ------------------------------------------------------------------------------------------------------------

	struct UpdateAssetToComponentElement {
		ECS_INLINE bool IsAssetDifferent() const {
			return old_asset.buffer != new_asset.buffer || old_asset.size != new_asset.size;
		}

		Stream<void> old_asset;
		Stream<void> new_asset;
		ECS_ASSET_TYPE type;
	};

	// Finds all unique and shared components that reference these assets and updates their values
	ECSENGINE_API void UpdateAssetsToComponents(
		const Reflection::ReflectionManager* reflection_manager,
		EntityManager* entity_manager,
		Stream<UpdateAssetToComponentElement> elements
	);

	// Updates the link components to the new remapping from here
	ECSENGINE_API void UpdateAssetRemappings(
		const Reflection::ReflectionManager* reflection_manager,
		EntityManager* entity_manager,
		const AssetDatabaseAssetRemap& asset_remapping
	);

	// ------------------------------------------------------------------------------------------------------------
}