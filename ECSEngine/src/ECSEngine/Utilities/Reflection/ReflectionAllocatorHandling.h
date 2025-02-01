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

		// If the given field has a field allocator specified in its tag, it will return the allocator's field index, else -1
		ECSENGINE_API size_t GetReflectionTypeFieldAllocatorFromTagAsIndex(const ReflectionType* type, size_t field_index);

		// For a given type, it returns the index of the misc entry that corresponds to the overall allocator, if there is one, else -1
		ECSENGINE_API size_t GetReflectionTypeOverallAllocatorMiscIndex(const ReflectionType* type);

		ECS_INLINE bool HasReflectionTypeOverallAllocator(const ReflectionType* type) {
			return GetReflectionTypeOverallAllocatorMiscIndex(type) != -1;
		}

		// For a field that is an allocator, it converts that instance to a polymorphic entry. The referenced field must be a direct allocator field,
		// Not a nested allocator field like it can happen for the main allocator
		ECSENGINE_API AllocatorPolymorphic ConvertReflectionTypeFieldToAllocator(const ReflectionType* type, size_t field_index, const void* instance);

		// For a field that is an allocator, it converts that instance to a polymorphic entry. The field can be a direct allocator field, like normal
		// Field allocators and main allocators, but also a nested main allocator, it handles that case. This should be the default function
		// To use when you have an arbitrary field index which can be a main index
		ECSENGINE_API AllocatorPolymorphic ConvertReflectionTypeFieldToAllocatorWithMainHandling(const ReflectionType* type, size_t field_index, const void* instance);

		// If the given type has an overall allocator, it will return the pointer to that field, else it will return the fallback allocator
		ECSENGINE_API AllocatorPolymorphic GetReflectionTypeOverallAllocator(const ReflectionType* type, const void* instance, AllocatorPolymorphic fallback_allocator);

		// If the given type has a field allocator specified for the field index, then it will return the allocator polymorphic that corresponds
		// To the specified allocator entry, else it will return the fallback allocator
		ECSENGINE_API AllocatorPolymorphic GetReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index, const void* instance, AllocatorPolymorphic fallback_allocator);

		// If the field allocators are enabled and the given type has a field allocator specified for the field index, then it will return the 
		// Allocator polymorphic that corresponds to the specified allocator entry, else it will return the fallback allocator
		ECS_INLINE AllocatorPolymorphic GetReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index, const void* instance, AllocatorPolymorphic fallback_allocator, bool are_field_allocators_enabled) {
			if (are_field_allocators_enabled) {
				return GetReflectionTypeFieldAllocator(type, field_index, instance, fallback_allocator);
			}
			return fallback_allocator;
		}

		// If the specified SoA has a field allocator, it will return a polymorphic allocator to that instance, else it will return the fallback allocatr
		ECSENGINE_API AllocatorPolymorphic GetReflectionTypeFieldAllocatorForSoa(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, const void* instance, AllocatorPolymorphic fallback_allocator);
		
		// If the specified SoA has a field allocator and those are enabled, it will return a polymorphic allocator to that instance, else it will return the fallback allocatr
		ECS_INLINE AllocatorPolymorphic GetReflectionTypeFieldAllocatorForSoa(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, const void* instance, AllocatorPolymorphic fallback_allocator, bool are_field_allocators_enabled) {
			if (are_field_allocators_enabled) {
				return GetReflectionTypeFieldAllocatorForSoa(type, soa, instance, fallback_allocator);
			}
			return fallback_allocator;
		}

		// Sets the target field with a reference allocator to the passed argument
		ECSENGINE_API void SetReflectionTypeFieldAllocatorReference(const ReflectionType* type, const ReflectionTypeMiscAllocator* allocator_info, void* instance, AllocatorPolymorphic reference_allocator);

		// For a given type, it will fill in the soa indices of the allocator entries in an order in which dependencies
		// Are initialized before they are needed
		ECSENGINE_API void GetReflectionTypeAllocatorInitializeOrderSoaIndices(const ReflectionType* type, CapacityStream<unsigned int>& allocator_soa_indices);

		// For a field index that is an allocator field, which may or may not be a main allocator, it returns the address of the pointer for that instance and the definition
		// Of the allocator, while taking into account nested main allocators
		ECSENGINE_API void GetReflectionTypeAllocatorPointerAndDefinition(const ReflectionType* type, size_t field_index, const void* instance, void*& allocator_pointer, Stream<char>& definition);

	}

}