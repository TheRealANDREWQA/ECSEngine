#include "ecspch.h"
#include "SparseSet.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void SparseSetCopyTypeErased(const void* source, void* destination, size_t element_byte_size, AllocatorPolymorphic allocator) {
		SparseSet<char>* source_set = (SparseSet<char>*)source;
		size_t allocation_size = (element_byte_size + sizeof(uint2)) * source_set->capacity;
		void* allocation = nullptr;
		if (allocation_size > 0) {
			allocation = Allocate(allocator, allocation_size);
			memcpy(allocation, source_set->buffer, allocation_size);
		}

		SparseSet<char>* destination_set = (SparseSet<char>*)destination;
		destination_set->buffer = (char*)allocation;
		destination_set->indirection_buffer = (uint2*)function::OffsetPointer(allocation, element_byte_size * source_set->capacity);
		destination_set->size = source_set->size;
		destination_set->capacity = source_set->capacity;
		destination_set->first_empty_slot = source_set->first_empty_slot;
	}

	// ------------------------------------------------------------------------------------------------------------

	void SparseSetCopyTypeErasedFunction(
		const void* source,
		void* destination,
		size_t element_byte_size,
		AllocatorPolymorphic allocator,
		void (*copy_function)(const void* source_element, void* destination_element, AllocatorPolymorphic allocator, void* extra_data),
		void* extra_data
	) {
		SparseSetCopyTypeErased(source, destination, element_byte_size, allocator);
		// For each element call the functor
		SparseSet<char>* source_set = (SparseSet<char>*)source;
		SparseSet<char>* destination_set = (SparseSet<char>*)destination;

		unsigned int size = source_set->size;
		unsigned int int_byte_size = (unsigned int)element_byte_size;
		for (unsigned int index = 0; index < size; index++) {
			copy_function(function::OffsetPointer(source_set->buffer, index * int_byte_size), function::OffsetPointer(destination_set->buffer, index * int_byte_size), allocator, extra_data);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void SparseSetInitializeUntyped(void* destination, unsigned int capacity, unsigned int element_byte_size, void* _buffer)
	{
		SparseSet<char>* sparse_set = (SparseSet<char>*)destination;
		sparse_set->buffer = (char*)_buffer;
		sparse_set->indirection_buffer = (uint2*)function::OffsetPointer(_buffer, capacity * element_byte_size);
		sparse_set->first_empty_slot = 0;
		sparse_set->size = 0;
		sparse_set->capacity = capacity;

		sparse_set->InitializeIndirectionBuffer(sparse_set->indirection_buffer, capacity);
	}

	// ------------------------------------------------------------------------------------------------------------

}