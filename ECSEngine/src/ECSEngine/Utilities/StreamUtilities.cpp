#include "ecspch.h"
#include "StreamUtilities.h"
#include "PointerUtilities.h"

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


}