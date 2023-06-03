#pragma once
#include "Shaders/CBufferTags.hlsli"
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionType;
	}

	// This will retrieve the fields from the reflection type field tags
	ECSENGINE_API void GetConstantBufferInjectTagFieldsFromType(const Reflection::ReflectionType* type, CapacityStream<unsigned int>* fields);

}