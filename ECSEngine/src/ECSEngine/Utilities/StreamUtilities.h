// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "PointerUtilities.h"
#include "Reflection/ReflectionMacros.h"

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
	unsigned int CoalesceStreamIntoType(Type* type, size_t type_capacity, Stream<void> stream) {
		ECS_ASSERT(sizeof(*type) + stream.size <= type_capacity);
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
	ECS_INLINE Type* CreateCoalescedStreamIntoType(CapacityStream<void>& buffer, Stream<void> stream, unsigned int* write_size) {
		buffer.AssertCapacity(stream.size);
		Type* type = (Type*)OffsetPointer(buffer);
		*write_size = CoalesceStreamIntoType(type, buffer.capacity - buffer.size, stream);
		buffer.size += stream.size;
		return type;
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// Does not set the data size inside the type
	template<typename Type>
	Type* CreateCoalescedStreamsIntoType(CapacityStream<void>& buffer, Stream<Stream<void>> buffers, unsigned int* write_size) {
		unsigned int initial_size = buffer.size;
		Type* type = (Type*)OffsetPointer(buffer);
		buffer.size += sizeof(*type);
		for (size_t index = 0; index < buffers.size; index++) {
			buffer.AssertCapacity(buffers[index].size);
			buffers[index].CopyTo(OffsetPointer(buffer));
			buffer.size += buffers[index].size;
		}
		*write_size = buffer.size - initial_size;
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

	// Each stream parameter must be a mutable reference
	template<typename FirstStream, typename... Streams>
	void CoalesceStreamsEmptyImplementation(void* allocation, FirstStream& first_stream, Streams... streams) {
		// Use memcpy to assign the pointer since in this case we don't need to cast to the stream type
		memcpy(&first_stream.buffer, &allocation, sizeof(allocation));

		if constexpr (sizeof...(Streams) > 0) {
			allocation = OffsetPointer(allocation, first_stream.MemoryOf(first_stream.size));
			CoalesceStreamsEmptyImplementation(allocation, streams...);
		}
	}

	// Each stream parameter must be a mutable reference
	template<typename FirstStream, typename... Streams>
	void CoalesceStreamsEmpty(AllocatorPolymorphic allocator, FirstStream& first_stream, Streams... streams) {
		size_t total_size = StreamsTotalSize(first_stream, streams...);
		void* allocation = Allocate(allocator, total_size);

		CoalesceStreamsEmptyImplementation(allocation, first_stream, streams...);
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// Each stream parameter must be a mutable reference
	template<typename FirstStream, typename... Streams>
	void CoalesceStreamsImplementation(void* allocation, FirstStream& first_stream, Streams... streams) {
		// Use memcpy to assign the pointer since in this case we don't need to cast to the stream type
		first_stream.CopyTo(allocation);
		memcpy(&first_stream.buffer, &allocation, sizeof(allocation));

		if constexpr (sizeof...(Streams) > 0) {
			allocation = OffsetPointer(allocation, first_stream.MemoryOf(first_stream.size));
			CoalesceStreamsImplementation(allocation, streams...);
		}
	}

	// Each stream parameter must be a mutable reference
	template<typename FirstStream, typename... Streams>
	void CoalesceStreams(AllocatorPolymorphic allocator, FirstStream& first_stream, Streams... streams) {
		size_t total_size = StreamsTotalSize(first_stream, streams...);
		void* allocation = Allocate(allocator, total_size);

		CoalesceStreamsImplementation(allocation, first_stream, streams...);
	}
	
	// -----------------------------------------------------------------------------------------------------------------------

	// Does not deallocate the passed stream parameters
	template<typename ElementType, typename... Streams>
	void CoalesceStreamsSameTypeImplementation(Stream<ElementType>& allocated_stream, Stream<ElementType> first_stream, Streams... streams) {
		allocated_stream.AddStream(first_stream);

		if constexpr (sizeof...(Streams) > 0) {
			CoalesceStreamsSameTypeImplementation(allocated_stream, streams...);
		}
	}

	// Does not deallocate the passed stream parameters
	template<typename ElementType, typename... Streams>
	Stream<ElementType> CoalesceStreamsSameType(AllocatorPolymorphic allocator, Streams... streams) {
		size_t total_size = StreamsTotalSize(streams...);

		Stream<ElementType> allocated_stream;
		// The returned size is the byte size
		allocated_stream.Initialize(allocator, total_size / sizeof(ElementType));
		allocated_stream.size = 0;
		CoalesceStreamsSameTypeImplementation(allocated_stream, streams...);
		return allocated_stream;
	}

	// Deallocates the passed stream parameters
	template<typename ElementType, typename FirstStream, typename... Streams>
	void CoalesceStreamsSameTypeWithDeallocateImplementation(AllocatorPolymorphic allocator, Stream<ElementType>& allocated_stream, FirstStream& first_stream, Streams... streams) {
		// We take in another template parameter FirstStream in case different Stream types are passed in (i.e. Stream, CapacityStream, ResizableStream)
		allocated_stream.AddStream(first_stream);
		first_stream.Deallocate(allocator);

		if constexpr (sizeof...(Streams) > 0) {
			CoalesceStreamsSameTypeWithDeallocateImplementation(allocator, allocated_stream, streams...);
		}
	}

	// Deallocates all streams parameters passed in. The streams must be mutable references.
	template<typename ElementType, typename... Streams>
	Stream<ElementType> CoalesceStreamsSameTypeWithDeallocate(AllocatorPolymorphic allocator, Streams... streams) {
		size_t total_size = StreamsTotalSize(streams...);

		Stream<ElementType> allocated_stream;
		// The returned size is the byte size
		allocated_stream.Initialize(allocator, total_size / sizeof(ElementType));
		allocated_stream.size = 0;
		CoalesceStreamsSameTypeWithDeallocateImplementation(allocator, allocated_stream, streams...);
		return allocated_stream;
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

	template<typename IntegerType>
	struct ECS_REFLECT MovedElementIndex {
		static_assert(std::is_unsigned_v<IntegerType>, "MovedElementIndex accepts only unsigned integer types");

		IntegerType previous;
		IntegerType current;
	};

	// Moves the elements according to the provided moves, while taking into account the original indices of the elements.
	// It will perform moves.size shuffles, without using any additional memory, except for a temporary index array, but
	// It does not copy the original elements to a temporary buffer in order to look up the elements. This is more efficient
	// For large types, since fewer memory is needed. If the provided temporary allocator is nullptr, it will simply use Malloc.
	// Each element to be moved must appear only once, and there should not be any conflicts (no 2 elements should target the same index)
	// Only unsigned integer types are allowed as template argument
	template<typename MovedElementIntegerType>
	ECSENGINE_API void MoveElements(void* elements, size_t element_count, size_t element_byte_size, IteratorInterface<const MovedElementIndex<MovedElementIntegerType>>* moves, AllocatorPolymorphic temporary_allocator = { nullptr });

	// The temporary allocator can be nullptr, in which case it will use Malloc.
	// This function is the same as MoveElements, but instead of performing the shuffles, it instructs
	// The functor as to what indices need to be moved where.
	// The functor will be called with (IntegerType previous_index, IntegerType current_index)
	template<typename MovedElementIntegerType, typename Functor>
	void MoveElementsFunctor(size_t element_count, IteratorInterface<const MovedElementIndex<MovedElementIntegerType>>* moves, AllocatorPolymorphic temporary_allocator, Functor&& functor) {
		// If there are no moves, early exit. If it is unbounded, skip the perf check.
		if (!moves->IsUnbounded()) {
			if (moves->GetRemainingCount() == 0) {
				return;
			}
		}
		
		if (temporary_allocator.allocator == nullptr) {
			temporary_allocator = ECS_MALLOC_ALLOCATOR;
		}

		// Allocate an array to hold the indices where the element is at for their initial position
		// Need to remove the const in the type, if it is specified
		Stream<std::remove_const_t<MovedElementIntegerType>> element_indices;
		element_indices.Initialize(temporary_allocator, element_count);
		MakeSequence(element_indices);

		__try {
			moves->ForEach([&](MovedElementIndex<MovedElementIntegerType> move) {
				size_t current_index = element_indices[move.previous];
				if (current_index != move.current) {
					functor(current_index, move.current);
					// Update the mapping
					element_indices[move.current] = current_index;
					// No need to update element_indices[moves[index].previous] since it has already been used
				}
			});
		}
		__finally {
			element_indices.Deallocate(temporary_allocator);
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------

	// Removes a collection of indices from an "array" like container which implements RemoveSwapBack.
	// Since RemoveSwapBack swaps elements, the initial indices might not correspond to the proper index
	// After a few removals. This function takes care of that. The temporary allocator is needed to allocate
	// A temporary array, if left as default, it will use Malloc. The functor will be called for each appropriate index.
	// The integer type must be an unsigned integer. The container size is needed to know which elements get swapped.
	template<typename IntegerType>
	ECSENGINE_API void RemoveArrayElements(
		IteratorInterface<IntegerType>* indices,
		size_t container_size,
		void (*functor)(void* user_data, IntegerType index), 
		void* functor_data, 
		AllocatorPolymorphic temporary_allocator = { nullptr }
	);

	// The same as the other overload, except that it takes a lambda functor instead of an untyped functor.
	// The functor will be called with parameters (IntegerType index).
	template<typename Functor, typename IntegerType>
	void RemoveArrayElements(IteratorInterface<IntegerType>* indices, size_t container_size, AllocatorPolymorphic temporary_allocator, Functor functor) {
		auto wrapper = [](void* user_data, IntegerType index) {
			Functor* functor = (Functor*)user_data;
			(*functor)(index);
		};

		RemoveArrayElements<IntegerType>(indices, container_size, wrapper, &functor, temporary_allocator);
	}

	// -----------------------------------------------------------------------------------------------------------------------
	
	// For an array like container, it creates new entries that must be placed at specific indices. It ensures that these
	// New entries are at their proper location. The temporary allocator is needed to make a temporary allocation, if left
	// As nullptr, it will use Malloc. 
	// create_functor will be called without any parameters (it should know by itself which element it should create) 
	// And must return the index of the entry where it should be located at.
	// move_functor will be called with parameters (size_t previous_index, size_t new_index) and must swap the 
	template<typename CreateFunctor, typename MoveFunctor>
	void CreateArrayElementsWithIndexLocation(
		size_t new_entry_count, 
		size_t container_size, 
		AllocatorPolymorphic temporary_allocator, 
		CreateFunctor&& create_functor,
		MoveFunctor&& move_functor
	) {
		if (new_entry_count == 0) {
			return;
		}

		if (temporary_allocator.allocator == nullptr) {
			temporary_allocator = ECS_MALLOC_ALLOCATOR;
		}

		// In this array the entries which cannot be moved right away are stored. For example, if element 4 is created, but it needs
		// To be at location 6, there are only 5 elements in the array, and that can result in an incorrect memory access. For this
		// Reason, elements that cannot be moved right away need to stored.
		Stream<MovedElementIndex<size_t>> unmatched_entries;
		unmatched_entries.Initialize(temporary_allocator, new_entry_count);
		unmatched_entries.size = 0;

		__try {
			for (size_t index = 0; index < new_entry_count; index++) {
				size_t swap_index = create_functor();
				// If the element is not in final position, a swap is needed
				// The swap can be performed now
				if (swap_index < container_size) {
					move_functor(container_size, swap_index);
				}
				else if (swap_index > container_size) {
					// The swap must be postponed
					unmatched_entries.Add({ container_size, swap_index });
				}
				container_size++;
			}

			// Finalize the unmatched entries now, they should all be possible to be placed at their indicated location.
			// We could use MoveElements, but it would be a bit more inefficient since it will make another allocation.
			// What we can do instead is to patch the index reference in the unmatched_entries_array.
			for (size_t index = 0; index < unmatched_entries.size; index++) {
				move_functor(unmatched_entries[index].previous, unmatched_entries[index].current);

				// Can't use SearchBytes unfortunately, it might make this a little more slower than it should be,
				// But it shouldn't be a high frequency operation anyways.
				size_t swapped_index = unmatched_entries[index].current;
				size_t swapped_new_index = unmatched_entries[index].previous;
				for (size_t subindex = index + 1; subindex < unmatched_entries.size; subindex++) {
					if (unmatched_entries[subindex].previous == swapped_index) {
						unmatched_entries[subindex].previous = swapped_new_index;
						break;
					}
				}
			}
		}
		__finally {
			unmatched_entries.Deallocate(temporary_allocator);
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------

}

