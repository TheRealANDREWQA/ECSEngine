#pragma once
#include "ReflectionTypes.h"

namespace ECSEngine {

	namespace Reflection {

		// Returns true if the given field is an overall type allocator, else false
		ECSENGINE_API bool IsReflectionTypeOverallAllocatorByTag(const ReflectionType* type, size_t field_index);

		// Returns true if the given field is an allocator, else false
		ECSENGINE_API bool IsReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index);

		// Returns true if the given field index is an allocator, else false. It deduces this information
		// By checking the misc entries, not by checking for the actual definition
		ECSENGINE_API bool IsReflectionTypeFieldAllocatorFromMisc(const ReflectionType* type, size_t field_index);

		// Returns true if the given field should be used as an allocator reference, instead of a fully allocated instance
		ECSENGINE_API bool IsReflectionTypeFieldAllocatorAsReference(const ReflectionType* type, size_t field_index);

		// If the given field has a field allocator specified in its tag, it will return the allocator field name, else an empty name
		ECSENGINE_API Stream<char> GetReflectionTypeFieldAllocatorFromTag(const ReflectionType* type, size_t field_index);

		// For a given type, it returns the index of the misc entry that corresponds to the overall allocator, if there is one, else -1
		ECSENGINE_API size_t GetReflectionTypeAllocatorMiscIndex(const ReflectionType* type);

		ECS_INLINE bool HasReflectionTypeOverallAllocator(const ReflectionType* type) {
			return GetReflectionTypeAllocatorMiscIndex(type) != -1;
		}

		// For a field that is an allocator, it converts that instance to a polymorphic entry
		ECSENGINE_API AllocatorPolymorphic ConvertReflectionTypeFieldToAllocator(const ReflectionType* type, size_t field_index, const void* instance);

		// If the given type has an overall allocator, it will return the pointer to that field, else it will return the fallback allocator
		ECSENGINE_API AllocatorPolymorphic GetReflectionTypeOverallAllocator(const ReflectionType* type, const void* instance, AllocatorPolymorphic fallback_allocator);

		// If the given type has a field allocator specified for the field index, then it will return the allocator polymorphic that corresponds
		// To the specified allocator entry, else it will return the fallback allocator
		ECSENGINE_API AllocatorPolymorphic GetReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index, const void* instance, AllocatorPolymorphic fallback_allocator);

	}

}