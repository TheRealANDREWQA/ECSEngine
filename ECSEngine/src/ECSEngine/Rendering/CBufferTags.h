#pragma once
#include "Shaders/CBufferTags.hlsli"
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionType;
	}

#define ECS_SHADER_REFLECTION_CONSTANT_BUFFER_TAG_DELIMITER "___"

	// This will retrieve the fields from the reflection type tag
	ECSENGINE_API void GetConstantBufferInjectTagFieldsFromTypeTag(const Reflection::ReflectionType* type, CapacityStream<unsigned int>* fields);

}