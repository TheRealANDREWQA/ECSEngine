#pragma once
#include "../../Core.h"
#include "../../Utilities/BasicTypes.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionContainerTypeMatchData;
		struct ReflectionContainerTypeByteSizeData;
		struct ReflectionContainerTypeDependentTypesData;
	}

	ECSENGINE_API bool ReflectionContainerTypeMatch_ReferenceCountedAsset(Reflection::ReflectionContainerTypeMatchData* data);

	ECSENGINE_API ulong2 ReflectionContainerTypeByteSize_ReferenceCountedAsset(Reflection::ReflectionContainerTypeByteSizeData* data);

	ECSENGINE_API void ReflectionContainerTypeDependentTypes_ReferenceCountedAsset(Reflection::ReflectionContainerTypeDependentTypesData* data);

#define ECS_REFLECTION_CONTAINER_TYPE_REFERENCE_COUNTED_ASSET { ReflectionContainerTypeMatch_ReferenceCountedAsset, ReflectionContainerTypeDependentTypes_ReferenceCountedAsset, ReflectionContainerTypeByteSize_ReferenceCountedAsset }

	struct SerializeCustomTypeWriteFunctionData;
	struct SerializeCustomTypeReadFunctionData;

	ECSENGINE_API size_t SerializeCustomTypeWriteFunction_ReferenceCountedAsset(SerializeCustomTypeWriteFunctionData* data);

	ECSENGINE_API size_t SerializeCustomTypeReadFunction_ReferenceCountedAsset(SerializeCustomTypeReadFunctionData* data);

#define ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_ASSET_VERSION (0)

}