#pragma once
#include "../Resources/AssetMetadata.h"
#include "../Tools/Modules/ModuleDefinition.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct AssetDatabase;

	// Returns true if the link component needs to be built using DLL functions
	ECSENGINE_API bool GetReflectionTypeLinkComponentNeedsDLL(const Reflection::ReflectionType* type);

	// Returns { nullptr, 0 } if there is no target specified
	ECSENGINE_API Stream<char> GetReflectionTypeLinkComponentTarget(const Reflection::ReflectionType* type);

	// If it ends in Link, it will return the name without that suffix
	ECSENGINE_API Stream<char> GetReflectionTypeLinkNameBase(Stream<char> name);

	// Returns the name for the link component (e.g. Translation -> TranslationLink)
	ECSENGINE_API Stream<char> GetReflectionTypeLinkComponentName(Stream<char> name, CapacityStream<char>& link_name);

	// When searching for asset fields in a target type and the type is a shader, the shader_type can narrow down the
	// shader type that it can accept (it can also be ECS_SHADER_TYPE_COUNT when it can accept all)
	struct LinkComponentAssetField {
		unsigned int field_index;
		AssetTypeEx type;
	};

	// ------------------------------------------------------------------------------------------------------------

	// The allocation will be made from the given allocator. Can specify whether or not the allocation should be coallesced or not.
	// When the allocation is coallesced in order to deallocate just deallocate the name buffer of the type
	// The type will not be inserted into the reflection manager. The reflection manager is used to determine the alignments of different fields
	ECSENGINE_API Reflection::ReflectionType CreateLinkTypeForComponent(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type, 
		AllocatorPolymorphic allocator, 
		bool coalesced_allocation = false
	);

	// Returns true if the link_type is default generated - mirros the target type with unsigned int handles
	// instead of pointers
	ECSENGINE_API bool IsLinkTypeDefaultGeneratedForComponent(
		const Reflection::ReflectionType* target_type,
		const Reflection::ReflectionType* link_type
	);

	// ------------------------------------------------------------------------------------------------------------
	
	// Can optionally give an exception list in order to avoid creating a link type for certain types
	// If the allocator is left unspecified, it will use the allocator from the reflection manager
	// If the link_type_names is specified it will fill in the names of newly generated link types
	struct CreateLinkTypesForComponentsOptions {
		Stream<Stream<char>> exceptions = { nullptr, 0 };
		CapacityStream<Stream<char>>* link_type_names = nullptr;
		AllocatorPolymorphic allocator = { nullptr };
	};

	// It will create the link types for all components (unique and shared) that require such a type and insert them
	// into the reflection manager. The link reflection type is made with a coalesced allocation
	ECSENGINE_API void CreateLinkTypesForComponents(
		Reflection::ReflectionManager* reflection_manager, 
		unsigned int folder_index, 
		CreateLinkTypesForComponentsOptions* options = nullptr
	);

	// ------------------------------------------------------------------------------------------------------------

	// Returns the name of the type if there is a link type that targets the given type (using the ECS_REFLECT_LINK_COMPONENT()). 
	// If suffixed_names is given then it will also search for types that have a certain suffix for the given target.
	// If there is none it will return { nullptr, 0 }
	ECSENGINE_API Stream<char> GetLinkComponentForTarget(
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<char> target, 
		Stream<char> suffixed_name = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// This version is the same as the single variant the difference being that it will fill in the link names
	// in the link_names pointer. There must be targets.size spots allocated in the link_names
	ECSENGINE_API void GetLinkComponentForTargets(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<Stream<char>> targets,
		Stream<char>* link_names,
		Stream<char> suffixed_name = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in the asset fields that cannot be found in the new_type from the previous_type
	ECSENGINE_API void GetLinkComponentMissingAssetFields(
		const Reflection::ReflectionType* previous_type,
		const Reflection::ReflectionType* new_type,
		CapacityStream<LinkComponentAssetField>* missing_fields
	);

	// Fills in the asset fields that cannot be found in the new_type from the previous_type
	ECSENGINE_API void GetLinkComponentTargetMissingAssetFields(
		const Reflection::ReflectionType* previous_type,
		const Reflection::ReflectionType* new_type,
		CapacityStream<LinkComponentAssetField>* missing_fields
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in the field indices of the fields that contain asset handles
	ECSENGINE_API void GetAssetFieldsFromLinkComponent(const Reflection::ReflectionType* type, CapacityStream<LinkComponentAssetField>& typed_handles);

	// Returns true if it has conformant types. (for example we don't support basic arrays of ResourceView for textures)
	ECSENGINE_API bool GetAssetFieldsFromLinkComponentTarget(const Reflection::ReflectionType* type, CapacityStream<LinkComponentAssetField>& typed_handles);

	// When type is SHADER the shader_type can be used to narrow down the selection of shaders that it can accept
	struct AssetTargetFieldFromReflection {
		bool success;
		AssetTypeEx type;
		Stream<void> asset;
	};
	
	// Returns true if the link component has any asset fields, else false
	ECSENGINE_API bool HasAssetFieldsLinkComponent(const Reflection::ReflectionType* type);

	// Returns true if the target component has any asset fields, else false
	ECSENGINE_API bool HasAssetFieldsTargetComponent(const Reflection::ReflectionType* type);

	// ------------------------------------------------------------------------------------------------------------

	// Extracts from a target type the asset fields. If the data is nullptr then no pointer will be retrieved.
	// It returns ECS_ASSET_TYPE_COUNT if there is not an asset field type. It will set the boolean flag to false if the field 
	// has been identified to be an asset field but incorrectly specified (like an array of meshes or pointer indirection to 2).
	// The asset pointer that will be filled in for the GPU resources (textures, samplers and shaders) will be the interface pointer, 
	// not the pointer to the ResourceView or SamplerState. In this way the result can be passed to ExtractLinkComponentFunctionAsset
	// class of functions to get the correct type
	ECSENGINE_API AssetTargetFieldFromReflection GetAssetTargetFieldFromReflection(
		const Reflection::ReflectionType* type,
		unsigned int field,
		const void* data
	);

	// Only accesses the field and returns the interface/pointer to the structure
	// Returns { nullptr, -1 } if the asset is could not be retrieved
	ECSENGINE_API Stream<void> GetAssetTargetFieldFromReflection(
		const Reflection::ReflectionType* type,
		unsigned int field,
		const void* data,
		ECS_ASSET_TYPE asset_type
	);

	ECSENGINE_API void GetLinkComponentTargetHandles(
		const Reflection::ReflectionType* type,
		const AssetDatabase* asset_database,
		const void* data,
		Stream<LinkComponentAssetField> asset_fields,
		unsigned int* handles
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
		const Reflection::ReflectionType* type,
		unsigned int field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type
	);

	// It will try to set the field according to the given field only if the metadata matches the given
	// comparator, else it will leave it the same
	ECSENGINE_API ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflectionIfMatches(
		const Reflection::ReflectionType* type,
		unsigned int field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type,
		Stream<void> comparator
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void GetLinkComponentHandles(
		const Reflection::ReflectionType* type, 
		const void* link_component,
		Stream<unsigned int> field_indices,
		CapacityStream<unsigned int>& handles
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void GetLinkComponentHandles(
		const Reflection::ReflectionType* type,
		const void* link_component, 
		Stream<LinkComponentAssetField> field_indices,
		CapacityStream<unsigned int>& handles
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void GetLinkComponentHandlePtrs(
		const Reflection::ReflectionType* type,
		const void* link_component,
		Stream<unsigned int> field_indices,
		CapacityStream<unsigned int*>& pointers
	);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void GetLinkComponentHandlePtrs(
		const Reflection::ReflectionType* type,
		const void* link_component,
		Stream<LinkComponentAssetField> field_indices,
		CapacityStream<unsigned int*>& pointers
	);

	// ------------------------------------------------------------------------------------------------------------

	// There must be asset_fields.size slots in the field_data array
	ECSENGINE_API void GetLinkComponentAssetData(
		const Reflection::ReflectionType* type,
		const void* link_component,
		const AssetDatabase* database,
		Stream<LinkComponentAssetField> asset_fields,
		CapacityStream<Stream<void>>* field_data
	);

	// ------------------------------------------------------------------------------------------------------------

	// There must be asset_fields.size slots in the field_data array
	ECSENGINE_API void GetLinkComponentAssetDataForTarget(
		const Reflection::ReflectionType* type,
		const void* target,
		Stream<LinkComponentAssetField> asset_fields,
		CapacityStream<Stream<void>>* field_data
	);

	// ------------------------------------------------------------------------------------------------------------

	// This version searches for a specific asset type from asset fields that can reference other assets
	// like materials can reference textures, samplers and shaders
	ECSENGINE_API void GetLinkComponentAssetDataForTargetDeep(
		const Reflection::ReflectionType* type,
		const void* target,
		Stream<LinkComponentAssetField> asset_fields,
		const AssetDatabase* database,
		ECS_ASSET_TYPE asset_type,
		CapacityStream<Stream<void>>* field_data
	);
	
	// ------------------------------------------------------------------------------------------------------------

	// If the link component is a default linked component, it verifies that the target
	// can be obtained from the linked component. For user supplied linked components, it assumes
	// that it is always the case.
	ECSENGINE_API bool ValidateLinkComponent(
		const Reflection::ReflectionType* link_type,
		const Reflection::ReflectionType* target_type
	);

	// ------------------------------------------------------------------------------------------------------------

	// If the link component has modifier fields but there is no such function, it will return false
	ECSENGINE_API bool ValidateLinkComponentTarget(
		const Reflection::ReflectionType* link_type,
		ModuleLinkComponentTarget target
	);

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE CoalescedMesh* ExtractLinkComponentFunctionMesh(void* buffer) {
		return (CoalescedMesh*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE ResourceView ExtractLinkComponentFunctionTexture(void* buffer) {
		return (ID3D11ShaderResourceView*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE SamplerState ExtractLinkComponentFunctionGPUSampler(void* buffer) {
		return (ID3D11SamplerState*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename ShaderType>
	ECS_INLINE ShaderType ExtractLinkComponentFunctionShader(void* buffer) {
		return ShaderType::FromInterface(buffer);
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE Material* ExtractLinkComponentFunctionMaterial(void* buffer) {
		return (Material*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void ResetLinkComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* link_component
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all the link components which target a unique component
	ECSENGINE_API void GetUniqueLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all the link components which target a shared component
	ECSENGINE_API void GetSharedLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all the link components which target a global component
	ECSENGINE_API void GetGlobalLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all link components which target a shared component or unique component
	ECSENGINE_API void GetAllLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& unique_link_types,
		CapacityStream<const Reflection::ReflectionType*>& shared_link_types,
		CapacityStream<const Reflection::ReflectionType*>& global_link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	struct ConvertToAndFromLinkBaseData {
		// ----------------- Mandatory ---------------------------
		ModuleLinkComponentTarget module_link;
		const Reflection::ReflectionManager* reflection_manager;
		const Reflection::ReflectionType* target_type;
		const Reflection::ReflectionType* link_type;
		const AssetDatabase* asset_database;


		// ------------------ Optional ---------------------------
		AllocatorPolymorphic allocator = { nullptr };
		Stream<LinkComponentAssetField> asset_fields = { nullptr, 0 };	
	};

	// Returns true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	// If the allocator is not provided, then it will only reference the fields (it will not make a deep copy)
	ECSENGINE_API bool ConvertFromTargetToLinkComponent(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* target_data,
		void* link_data,
		const void* previous_target_data,
		const void* previous_link_data
	);

	// ------------------------------------------------------------------------------------------------------------

	// Return true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	// If the allocator is not provided, then it will only reference the streams (it will not make a deep copy)
	ECSENGINE_API bool ConvertLinkComponentToTarget(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* link_data,
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data,
		bool apply_modifier_function
	);

	// ------------------------------------------------------------------------------------------------------------

	// It will ignore non asset fields. Return true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	ECSENGINE_API bool ConvertLinkComponentToTargetAssetsOnly(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* link_data,
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data
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
	ECSENGINE_API void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		Stream<unsigned int>* asset_fields_reference_count,
		GetAssetReferenceCountsFromEntitiesOptions options = {}
	);

	// ------------------------------------------------------------------------------------------------------------
	
	// Returns true if the link type has any modifier fields
	ECSENGINE_API bool HasModifierFieldsLinkComponent(const Reflection::ReflectionType* link_type);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void CopyModifierFieldsLinkComponent(const Reflection::ReflectionType* link_type, void* destination, const void* source);

	// ------------------------------------------------------------------------------------------------------------
	
	ECSENGINE_API void ResetModifierFieldsLinkComponent(const Reflection::ReflectionType* link_type, void* link_component);

	// ------------------------------------------------------------------------------------------------------------

}