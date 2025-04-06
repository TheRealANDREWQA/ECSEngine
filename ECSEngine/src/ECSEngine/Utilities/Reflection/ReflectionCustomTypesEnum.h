#pragma once

namespace ECSEngine {

	namespace Reflection {

		enum ECS_REFLECTION_CUSTOM_TYPE_INDEX : unsigned char {
			ECS_REFLECTION_CUSTOM_TYPE_STREAM,
			ECS_REFLECTION_CUSTOM_TYPE_REFERENCE_COUNTED,
			ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET,
			ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET,
			ECS_REFLECTION_CUSTOM_TYPE_DATA_POINTER,
			// Using a single entry for the allocator in order to reduce the interface bloat that is required,
			// This type will deal with all types of allocators
			ECS_REFLECTION_CUSTOM_TYPE_ALLOCATOR,
			ECS_REFLECTION_CUSTOM_TYPE_HASH_TABLE,
			ECS_REFLECTION_CUSTOM_TYPE_DECK,
			ECS_REFLECTION_CUSTOM_TYPE_COUNT
		};

	}

}