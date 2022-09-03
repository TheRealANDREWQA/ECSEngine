#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "Reflection/ReflectionTypes.h"
#include "Serialization/SerializationHelpers.h"

namespace ECSEngine {

	ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(ReferenceCounted);
	
	ECS_SERIALIZE_CUSTOM_TYPE_FUNCTION_HEADER(ReferenceCounted);

#define ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_VERSION (0)

}