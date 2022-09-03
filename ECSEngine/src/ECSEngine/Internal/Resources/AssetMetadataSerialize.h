#pragma once
#include "../../Utilities/Serialization/SerializationHelpers.h"
#include "../../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	// When deserializing, it will use the backup allocator to allocate its streams
	
	struct AssetDatabase;

	ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(MaterialAsset);
	
	ECS_SERIALIZE_CUSTOM_TYPE_FUNCTION_HEADER(MaterialAsset);

	ECSENGINE_API void SetSerializeCustomMaterialAssetDatabase(const AssetDatabase* database);

	ECSENGINE_API void SetSerializeCustomMaterialAssetDatabase(AssetDatabase* database);

#define ECS_SERIALIZE_CUSTOM_TYPE_MATERIAL_ASSET_VERSION (0)

}