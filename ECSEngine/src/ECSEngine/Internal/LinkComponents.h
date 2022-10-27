#pragma once
#include "Resources/AssetMetadata.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct LinkComponentAssetField {
		unsigned int field_index;
		ECS_ASSET_TYPE type;
	};

	// ------------------------------------------------------------------------------------------------------------

	// Fills in the field indices of the fields that contain asset handles
	ECSENGINE_API void GetAssetFieldsFromLinkComponent(const Reflection::ReflectionType* type, CapacityStream<LinkComponentAssetField>& field_indices);

	struct AssetTargetFieldFromReflection {
		ECS_ASSET_TYPE type;
		bool success;
		Stream<void> asset;
	};

	// ------------------------------------------------------------------------------------------------------------

	// Extracts from a target type the asset fields. If the data is nullptr then no pointer will be retrieved.
	// It returns ECS_ASSET_TYPE_COUNT if there is not an asset field type. It will set the boolean flag to false if the field 
	// has been identified to be an asset field but incorrectly specified (like an array of meshes or pointer indirection to 2).
	// The asset pointer that will be filled in for the GPU resources (textures, samplers and shaders) will be the interface pointer, 
	// not the pointer to the ResourceView or SamplerState. In this way the result can be passed tp ExtractLinkComponentFunctionAsset
	// class of functions to get the correct type
	ECSENGINE_API AssetTargetFieldFromReflection GetAssetTargetFieldFromReflection(
		const Reflection::ReflectionType* type,
		unsigned int field,
		const void* data
	);

	// ------------------------------------------------------------------------------------------------------------

	enum ECS_SET_ASSET_TARGET_FIELD_RESULT : unsigned char {
		ECS_SET_ASSET_TARGET_FIELD_NONE,
		ECS_SET_ASSET_TARGET_FIELD_OK,
		ECS_SET_ASSET_TARGET_FIELD_FAILED
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

	// If the link component is a default linked component, it verifies that the target
	// can be obtained from the linked component. For user supplied linked components it assumes
	// that it is always the case.
	ECSENGINE_API bool ValidateLinkComponent(
		const Reflection::ReflectionType* link_type,
		const Reflection::ReflectionType* target_type
	);

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE CoallescedMesh* ExtractLinkComponentFunctionMesh(void* buffer) {
		return (CoallescedMesh*)buffer;
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

	// Fills in all link components which target a shared component or unique component
	ECSENGINE_API void GetUniqueAndSharedLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& unique_link_types,
		CapacityStream<const Reflection::ReflectionType*>& shared_link_types
	);

	// ------------------------------------------------------------------------------------------------------------

}