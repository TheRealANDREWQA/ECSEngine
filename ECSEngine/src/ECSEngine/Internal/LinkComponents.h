#pragma once
#include "Resources/AssetMetadata.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionType;
	}

	struct LinkComponentAssetField {
		unsigned int field_index;
		ECS_ASSET_TYPE type;
	};

	// Fills in the field indices of the fields that contain asset handles
	ECSENGINE_API void GetReflectionAssetFieldsFromLinkComponent(const Reflection::ReflectionType* type, CapacityStream<LinkComponentAssetField>& field_indices);

	struct AssetTargetFieldFromReflection {
		ECS_ASSET_TYPE type;
		bool success;
		Stream<void> asset;
	};

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

}