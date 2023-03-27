#pragma once
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	// When deserializing, it will use the backup allocator to allocate its streams	
	struct AssetDatabase;

	enum ECS_ASSET_MATERIAL_SERIALIZE_SWITCH : unsigned char {
		// If set, then it will not increment its dependencies and in case it doesn't
		// exist, it will simply drop the asset
		ECS_ASSET_MATERIAL_SERIALIZE_DO_NOT_INCREMENT_DEPENDENCIES
	};

	ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(MaterialAsset);
	
	ECS_SERIALIZE_CUSTOM_TYPE_FUNCTION_HEADER(MaterialAsset);

	ECSENGINE_API void SetSerializeCustomMaterialAssetDatabase(const AssetDatabase* database);

	ECSENGINE_API void SetSerializeCustomMaterialAssetDatabase(AssetDatabase* database);

	// Returns the old status
	ECSENGINE_API bool SetSerializeCustomMaterialDoNotIncrementDependencies(bool status);

#define ECS_SERIALIZE_CUSTOM_TYPE_MATERIAL_ASSET_VERSION (0)

}