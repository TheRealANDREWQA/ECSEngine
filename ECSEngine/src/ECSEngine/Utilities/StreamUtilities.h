#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "PointerUtilities.h"

namespace ECSEngine {

	// The type must have as its first field a size_t describing the stream size
	// It returns the total amount of data needed to copy this structure
	template<typename StreamType, typename Type>
	size_t EmbedStream(Type* type, Stream<StreamType> stream) {
		memcpy(OffsetPointer(type, sizeof(*type)), stream.buffer, stream.MemoryOf(stream.size));
		size_t* size_ptr = (size_t*)type;
		*size_ptr = stream.size;
		return stream.MemoryOf(stream.size) + sizeof(*type);
	}

	// The type must have its first field a size_t describing its stream size
	template<typename StreamType, typename Type>
	ECS_INLINE Stream<StreamType> GetEmbeddedStream(const Type* type) {
		const size_t* size_ptr = (const size_t*)type;
		return { OffsetPointer(type, sizeof(*type)), *size_ptr };
	}

	ECSENGINE_API void CopyStreamAndMemset(void* destination, size_t destination_capacity, Stream<void> data, int memset_value = 0);

	// It will advance the ptr with the destination_capacity
	ECSENGINE_API void CopyStreamAndMemset(uintptr_t& ptr, size_t destination_capacity, Stream<void> data, int memset_value = 0);

	// If allocating a stream alongside its data, this function sets it up
	ECSENGINE_API void* CoalesceStreamWithData(void* allocation, size_t size);

	// If allocating a capacity stream alongside its data, this function sets it up
	ECSENGINE_API void* CoalesceCapacityStreamWithData(void* allocation, size_t size, size_t capacity);

	// Returns a stream allocated alongside its data
	ECSENGINE_API void* CoalesceStreamWithData(AllocatorPolymorphic allocator, Stream<void> data, size_t element_size);

	// The offset is added to the sequence
	template<typename Stream>
	ECSENGINE_API void MakeSequence(Stream stream, size_t offset = 0);

	// The offset is added to the sequence
	template<typename Stream>
	ECSENGINE_API void MakeDescendingSequence(Stream stream, size_t offset = 0);

	// The size of data must contain the byte size of the element
	template<typename IndexStream>
	ECSENGINE_API void CopyStreamWithMask(void* buffer, Stream<void> data, IndexStream indices);

	// -----------------------------------------------------------------------------------------------------------------------

	template<typename Stream, typename Value, typename Function>
	void GetMinFromStream(const Stream& input, Value& value, Function&& function) {
		for (size_t index = 0; index < input.size; index++) {
			Value current_value = function(input[index]);
			value = min(value, current_value);
		}
	}

	template<typename Stream, typename Value, typename Function>
	void GetMaxFromStream(const Stream& input, Value& value, Function&& function) {
		for (size_t index = 0; index < input.size; index++) {
			Value current_value = function(input[index]);
			value = max(value, current_value);
		}
	}

	template<typename Stream, typename Value, typename Function>
	void GetExtremesFromStream(const Stream& input, Value& min, Value& max, Function&& accessor) {
		for (size_t index = 0; index < input.size; index++) {
			Value current_value = accessor(input[index]);
			min = ECSEngine::min(min, current_value);
			max = ECSEngine::max(max, current_value);
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// Writes the stream data right after the type and sets the size
	// Returns the total write size
	template<typename Type>
	unsigned int CoalesceStreamIntoType(Type* type, Stream<void> stream) {
		void* location = OffsetPointer(type, sizeof(*type));
		memcpy(location, stream.buffer, stream.size);
		*type->Size() = stream.size;
		return sizeof(*type) + stream.size;
	}

	// -----------------------------------------------------------------------------------------------------------------------

	template<typename Type>
	ECS_INLINE Stream<void> GetCoalescedStreamFromType(Type* type) {
		void* location = OffsetPointer(type, sizeof(*type));
		return { location, *type->Size() };
	}

	// -----------------------------------------------------------------------------------------------------------------------

	template<typename Type>
	ECS_INLINE Type* CreateCoalescedStreamIntoType(void* buffer, Stream<void> stream, unsigned int* write_size) {
		Type* type = (Type*)buffer;
		*write_size = CoalesceStreamIntoType(type, stream);
		return type;
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// Does not set the data size inside the type
	template<typename Type>
	Type* CreateCoalescedStreamsIntoType(void* buffer, Stream<Stream<void>> buffers, unsigned int* write_size) {
		Type* type = (Type*)buffer;
		unsigned int offset = 0;
		for (size_t index = 0; index < buffers.size; index++) {
			buffers[index].CopyTo(OffsetPointer(buffer, sizeof(*type) + offset));
			offset += buffers[index].size;
		}
		*write_size = sizeof(*type) + offset;
		return type;
	}

	// Sizes needs to be byte sizes of the coalesced streams
	template<typename Type>
	Stream<void> GetCoalescedStreamFromType(Type* type, unsigned int stream_index, unsigned int* sizes) {
		unsigned int offset = 0;
		for (unsigned int index = 0; index < stream_index; index++) {
			offset += sizes[index];
		}
		return { OffsetPointer(type, sizeof(*type) + offset), sizes[stream_index] };
	}

	// -----------------------------------------------------------------------------------------------------------------------

	template<typename FirstStream, typename... Streams>
	size_t StreamsTotalSize(FirstStream first_stream, Streams... streams) {
		size_t total_size = 0;
		if constexpr (std::is_pointer_v<FirstStream>) {
			total_size = first_stream->MemoryOf(first_stream->size);
		}
		else {
			total_size = first_stream.MemoryOf(first_stream.size);
		}

		if constexpr (sizeof...(Streams) > 0) {
			total_size += StreamsTotalSize(streams...);
		}
		return total_size;
	}

	template<typename FirstStream, typename... Streams>
	void CoalesceStreamsEmptyImplementation(void* allocation, FirstStream* first_stream, Streams... streams) {
		// Use memcpy to assign the pointer since in this case we don't need to cast to the stream type
		memcpy(&first_stream->buffer, &allocation, sizeof(allocation));

		if constexpr (sizeof...(Streams) > 0) {
			allocation = OffsetPointer(allocation, first_stream->MemoryOf(first_stream->size));
			CoalesceStreamsEmptyImplementation(allocation, streams...);
		}
	}

	template<typename FirstStream, typename... Streams>
	void CoalesceStreamsEmpty(AllocatorPolymorphic allocator, FirstStream* first_stream, Streams... streams) {
		size_t total_size = StreamsTotalSize(first_stream, streams...);
		void* allocation = AllocateEx(allocator, total_size);

		CoalesceStreamsEmptyImplementation(allocastion, first_stream, streams...);
	}

	// -----------------------------------------------------------------------------------------------------------------------

	template<typename FirstStream, typename... Streams>
	void CoalesceStreamsImplementation(void* allocation, FirstStream* first_stream, Streams... streams) {
		// Use memcpy to assign the pointer since in this case we don't need to cast to the stream type
		first_stream->CopyTo(allocation);
		memcpy(&first_stream->buffer, &allocation, sizeof(allocation));

		if constexpr (sizeof...(Streams) > 0) {
			allocation = OffsetPointer(allocation, first_stream->MemoryOf(first_stream->size));
			CoalesceStreamsImplementation(allocation, streams...);
		}
	}

	template<typename FirstStream, typename... Streams>
	void CoalesceStreams(AllocatorPolymorphic allocator, FirstStream* first_stream, Streams... streams) {
		size_t total_size = StreamsTotalSize(first_stream, streams...);
		void* allocation = AllocateEx(allocator, total_size);

		CoalesceStreamsImplementation(allocation, first_stream, streams...);
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// It will add all elements from the additions into the base which are not already found in base
	// It uses the comparison operator
	template<typename CapacityStream>
	void StreamAddUnique(CapacityStream& base, CapacityStream additions) {
		for (unsigned int index = 0; index < additions.size; index++) {
			unsigned int subindex = 0;
			for (; subindex < base.size; subindex++) {
				if (base[subindex] == additions[index]) {
					break;
				}
			}

			if (subindex == base.size) {
				base.AddAssert(additions[index]);
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// It will add all elements from the additions into the base which are not already found in base
	// It uses the provided functor. The functor must take as parameters the elements (T a, T b)
	// and return true if they match
	template<typename CapacityStream, typename Functor>
	void StreamAddUniqueFunctor(CapacityStream& base, CapacityStream additions, Functor&& functor) {
		for (unsigned int index = 0; index < additions.size; index++) {
			unsigned int subindex = 0;
			for (; subindex < base.size; subindex++) {
				if (functor(base[subindex], additions[index])) {
					break;
				}
			}

			if (subindex == base.size) {
				base.AddAssert(additions[index]);
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// It will add all elements from the additions into the base which are not already found in base
	// It uses SearchBytes to locate the element
	template<typename CapacityStream>
	void StreamAddUniqueSearchBytes(CapacityStream& base, CapacityStream additions) {
		size_t element_byte_size = additions.MemoryOf(1);
		for (unsigned int index = 0; index < additions.size; index++) {
			size_t existing_index = SearchBytes(base.buffer, base.size, *(size_t*)(&additions[index]), element_byte_size);
			if (existing_index == -1) {
				base.AddAssert(additions[index]);
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------

}

