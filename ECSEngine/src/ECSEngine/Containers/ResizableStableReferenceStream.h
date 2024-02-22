#pragma once
#include "../Core.h"
#include "StableReferenceStream.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "Stream.h"

#define ECS_RESIZABLE_STABLE_REFERENCE_STREAM_GROWTH_FACTOR 1.3f

namespace ECSEngine {

	template<typename T, bool queue_indirection_list = false>
	struct ResizableStableReferenceStream {
		ResizableStableReferenceStream() {}
		ResizableStableReferenceStream(AllocatorPolymorphic _allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) : allocator(_allocator)  {
			if (_capacity > 0) {
				void* allocation = Allocate(allocator, stream.MemoryOf(_capacity), alignof(void*), debug_info);
				stream.InitializeFromBuffer(allocation, 0, _capacity);
			}
			else {
				stream.InitializeFromBuffer(nullptr, 0, 0);
			}
		}

		ResizableStableReferenceStream(const ResizableStableReferenceStream& other) = default;
		ResizableStableReferenceStream<T, queue_indirection_list>& operator = (const ResizableStableReferenceStream<T, queue_indirection_list>& other) = default;

		// Returns an index that can be used for the lifetime of the object to access it
		unsigned int Add(T element) {
			unsigned int index = ReserveOne();
			stream.buffer[index] = element;
			return index;
		}

		// Returns an index that can be used for the lifetime of the object to access it
		unsigned int Add(const T* element) {
			unsigned int index = ReserveOne();
			stream.buffer[index] = *element;
			return index;
		}

		ECS_INLINE void AddStream(Stream<T> elements, unsigned int* indices) {
			Reserve({ indices, elements.size });
		}

		void FreeBuffer(DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (stream.capacity > 0) {
				Deallocate(allocator, stream.buffer, debug_info);
			}

			stream.buffer = nullptr;
			stream.indirection_list = nullptr;
			stream.indirection_list_start_index = 0;
			stream.size = 0;
			stream.capacity = 0;
		}

		unsigned int ReserveOne() {
			if (stream.size == stream.capacity) {
				size_t new_capacity = (size_t)((float)stream.capacity * ECS_RESIZABLE_STABLE_REFERENCE_STREAM_GROWTH_FACTOR + 1.0f);
				SetNewCapacity(new_capacity);
			}
			return stream.ReserveOne();
		}

		// It will fill in the handles buffer with the new handles
		void Reserve(Stream<unsigned int> indices) {
			size_t new_size = stream.size + indices.size;
			if (stream.size + indices.size > stream.capacity) {
				size_t new_capacity = (size_t)((float)new_size * ECS_RESIZABLE_STABLE_REFERENCE_STREAM_GROWTH_FACTOR + 1.0f);
				SetNewCapacity(new_capacity);
			}
			stream.Reserve(indices);
		}

		// it will reset the free list
		ECS_INLINE void Reset() {
			stream.Reset();
		}

		// Add the index to the free list
		ECS_INLINE void Remove(unsigned int index) {
			stream.Remove(index);
		}

		// Only use if new capacity greater than the current size
		void SetNewCapacity(size_t new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			// Create a new buffer
			void* allocation = Allocate(allocator, stream.MemoryOf(new_capacity), alignof(void*), debug_info);

			StableReferenceStream<T, queue_indirection_list> old_stream = stream;
			stream.InitializeFromBuffer(allocation, new_capacity);
			// Now copy the old content
			stream.Copy(old_stream);
			// Deallocate the old buffer
			if (old_stream.buffer != nullptr) {
				Deallocate(allocator, old_stream.buffer, debug_info);
			}
		}

		// It will determine how many elements at the end are freed and will move all the existing elements
		// to a new buffer of that size
		// The threshold tells the function how many elements are needed to trigger the trim
		// Since trimming a single element or a handful might not be that helpful
		void Trim(unsigned int threshold = 1, DebugInfo debug_info = ECS_DEBUG_INFO) {
			// Get the last index in use. That is the threshold
			unsigned int last_index = 0;
			stream.ForEachIndex([&](unsigned int index) {
				last_index = max(index, last_index);
			});

			// The capacity to sustain the last index needs to be 1 more
			last_index++;
			if (stream.capacity - last_index >= threshold) {
				void* allocation = Allocate(allocator, stream.MemoryOf(last_index), alignof(T), debug_info);
				StableReferenceStream<T, queue_indirection_list> old_stream = stream;
				stream.InitializeFromBuffer(allocation, last_index);
				stream.Copy(old_stream);
				Deallocate(allocator, old_stream.buffer, debug_info);
			}
		}

		ECS_INLINE const T& operator [](unsigned int index) const {
			return stream[index];
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return stream[index];
		}

		ECS_INLINE static size_t MemoryOf(unsigned int count) {
			return StableReferenceStream<T>::MemoryOf(count);
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity) {
			allocator = _allocator;
			stream.Initialize(allocator, _capacity);
		}

		StableReferenceStream<T, queue_indirection_list> stream;
		AllocatorPolymorphic allocator;
	};

}