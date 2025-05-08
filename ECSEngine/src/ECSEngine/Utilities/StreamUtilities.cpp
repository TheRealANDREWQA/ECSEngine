#include "ecspch.h"
#include "StreamUtilities.h"
#include "PointerUtilities.h"
#include "Utilities.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	void CopyStreamAndMemset(void* destination, size_t destination_capacity, Stream<void> data, int memset_value)
	{
		size_t copy_size = min(destination_capacity, data.size);
		memcpy(destination, data.buffer, copy_size);
		if (copy_size != destination_capacity) {
			memset(OffsetPointer(destination, copy_size), memset_value, destination_capacity - copy_size);
		}
	}

	// --------------------------------------------------------------------------------------------------

	void CopyStreamAndMemset(uintptr_t& ptr, size_t destination_capacity, Stream<void> data, int memset_value)
	{
		CopyStreamAndMemset((void*)ptr, destination_capacity, data, memset_value);
		ptr += destination_capacity;
	}

	// --------------------------------------------------------------------------------------------------

	void* CoalesceStreamWithData(void* allocation, size_t size)
	{
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(Stream<void>);
		Stream<void>* stream = (Stream<void>*)allocation;
		stream->InitializeFromBuffer(buffer, size);
		return allocation;
	}

	// --------------------------------------------------------------------------------------------------

	void* CoalesceCapacityStreamWithData(void* allocation, size_t size, size_t capacity) {
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CapacityStream<void>);
		CapacityStream<void>* stream = (CapacityStream<void>*)allocation;
		stream->InitializeFromBuffer(buffer, size, capacity);
		return allocation;
	}

	// --------------------------------------------------------------------------------------------------

	void* CoalesceStreamWithData(AllocatorPolymorphic allocator, Stream<void> data, size_t element_size)
	{
		size_t allocation_size = sizeof(Stream<void>) + data.size;
		void* allocation = Allocate(allocator, allocation_size);
		if (allocation != nullptr) {
			Stream<void>* stream = (Stream<void>*)allocation;
			stream->InitializeFromBuffer(OffsetPointer(stream, sizeof(*stream)), data.size / element_size);
			memcpy(stream->buffer, data.buffer, data.size);
		}
		return allocation;
	}

	// --------------------------------------------------------------------------------------------------

	template<typename Stream>
	void MakeSequence(Stream stream, size_t offset)
	{
		for (size_t index = 0; index < stream.size; index++) {
			stream[index] = index + offset;
		}
	}

	ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeSequence, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, size_t);
	ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeSequence, CapacityStream<unsigned char>, CapacityStream<unsigned short>, CapacityStream<unsigned int>, CapacityStream<size_t>, size_t);

	// --------------------------------------------------------------------------------------------------

	template<typename Stream>
	void MakeDescendingSequence(Stream stream, size_t offset) {
		offset += stream.size - 1;
		for (size_t index = 0; index < stream.size; index++) {
			stream[index] = offset - index;
		}
	}

	ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeDescendingSequence, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, size_t);
	ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeDescendingSequence, CapacityStream<unsigned char>, CapacityStream<unsigned short>, CapacityStream<unsigned int>, CapacityStream<size_t>, size_t);

	// --------------------------------------------------------------------------------------------------

	template<typename IndexStream>
	void CopyStreamWithMask(void* buffer, Stream<void> data, IndexStream indices)
	{
		for (size_t index = 0; index < indices.size; index++) {
			memcpy(buffer, (void*)((uintptr_t)data.buffer + data.size * indices[index]), data.size);
			uintptr_t ptr = (uintptr_t)buffer;
			ptr += data.size;
			buffer = (void*)ptr;
		}
	}

	ECS_TEMPLATE_FUNCTION_4_AFTER(void, CopyStreamWithMask, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, void*, Stream<void>);

	// --------------------------------------------------------------------------------------------------

	template<typename MovedElementIntegerType>
	void MoveElements(void* elements, size_t element_count, size_t element_byte_size, IteratorInterface<const MovedElementIndex<MovedElementIntegerType>>* moves, AllocatorPolymorphic temporary_allocator) {
		// In order to swap an element, we need some temporary untyped storage
		ECS_STACK_VOID_STREAM(element_storage, ECS_KB * 4);
		ECS_ASSERT(element_byte_size <= element_storage.capacity, "Element byte size is too large for MoveElements!");

		MoveElementsFunctor(element_count, moves, temporary_allocator, [&](MovedElementIntegerType previous_index, MovedElementIntegerType new_index) {
			// Copy the destination to the temporary storage
			void* destination = OffsetPointer(elements, (size_t)previous_index * element_byte_size);
			void* source = OffsetPointer(elements, (size_t)new_index * element_byte_size);
			memcpy(element_storage.buffer, destination, element_byte_size);
			memcpy(destination, source, element_byte_size);
			memcpy(source, element_storage.buffer, element_byte_size);
		});
	}
	
	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<unsigned char>>*, AllocatorPolymorphic);
	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<unsigned short>>*, AllocatorPolymorphic);
	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<unsigned int>>*, AllocatorPolymorphic);
	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<size_t>>*, AllocatorPolymorphic);

	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<const unsigned char>>*, AllocatorPolymorphic);
	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<const unsigned short>>*, AllocatorPolymorphic);
	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<const unsigned int>>*, AllocatorPolymorphic);
	template ECSENGINE_API void MoveElements(void*, size_t, size_t, IteratorInterface<const MovedElementIndex<const size_t>>*, AllocatorPolymorphic);

	// --------------------------------------------------------------------------------------------------

	template<typename IntegerType>
	void RemoveArrayElements(
		IteratorInterface<IntegerType>* indices,
		size_t container_size,
		void (*functor)(void* user_data, IntegerType index),
		void* functor_data,
		AllocatorPolymorphic temporary_allocator
	) {
		static_assert(std::is_unsigned_v<IntegerType>, "RemoveArrayElements accepts only unsigned integers!");

		// If there are no elements, early exit
		if (indices->remaining_count == 0) {
			return;
		}

		if (temporary_allocator.allocator == nullptr) {
			temporary_allocator = ECS_MALLOC_ALLOCATOR;
		}

		// Allocate a temporary array where the integer indices are stored. Those will be modified
		// As we keep performing removals
		Stream<std::remove_const_t<IntegerType>> remapped_indices;
		remapped_indices.Initialize(temporary_allocator, indices->remaining_count);

		__try {
			indices->WriteTo(remapped_indices.buffer);
			for (size_t index = 0; index < remapped_indices.size; index++) {
				IntegerType current_index = remapped_indices[index];
				functor(functor_data, current_index);
				container_size--;

				// Search the current container size in the following entries
				size_t remapped_indices_index = SearchBytes(remapped_indices.buffer + index + 1, remapped_indices.size - index - 1, container_size, sizeof(IntegerType));
				if (remapped_indices_index != -1) {
					remapped_indices[remapped_indices_index] = current_index;
				}
			}
		}
		__finally {
			remapped_indices.Deallocate(temporary_allocator);
		}
	}

	template ECSENGINE_API void RemoveArrayElements(IteratorInterface<unsigned char>*, size_t, void (*)(void*, unsigned char), void*, AllocatorPolymorphic);
	template ECSENGINE_API void RemoveArrayElements(IteratorInterface<unsigned short>*, size_t, void (*)(void*, unsigned short), void*, AllocatorPolymorphic);
	template ECSENGINE_API void RemoveArrayElements(IteratorInterface<unsigned int>*, size_t, void (*)(void*, unsigned int), void*, AllocatorPolymorphic);
	template ECSENGINE_API void RemoveArrayElements(IteratorInterface<size_t>*, size_t, void (*)(void*, size_t), void*, AllocatorPolymorphic);

	template ECSENGINE_API void RemoveArrayElements<const unsigned char>(IteratorInterface<const unsigned char>*, size_t, void (*)(void*, const unsigned char), void*, AllocatorPolymorphic);
	template ECSENGINE_API void RemoveArrayElements<const unsigned short>(IteratorInterface<const unsigned short>*, size_t, void (*)(void*, const unsigned short), void*, AllocatorPolymorphic);
	template ECSENGINE_API void RemoveArrayElements<const unsigned int>(IteratorInterface<const unsigned int>*, size_t, void (*)(void*, const unsigned int), void*, AllocatorPolymorphic);
	template ECSENGINE_API void RemoveArrayElements<const size_t>(IteratorInterface<const size_t>*, size_t, void (*)(void*, const size_t), void*, AllocatorPolymorphic);

	// --------------------------------------------------------------------------------------------------

}