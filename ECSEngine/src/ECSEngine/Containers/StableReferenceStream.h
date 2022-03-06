#pragma once
#include "../Core.h"
#include "Stream.h"

namespace ECSEngine {

	// This is basically a pooled list
	// It keeps an array of free indices
	// Set boolean_flags to true if you want to iterate through the stream
	// It will keep a compacted frequency bit mask - every element gets a bit
	// To reduce the memory footprint and the memory bandwidth
	template<typename T, bool queue_free_list = false>
	struct StableReferenceStream {
		StableReferenceStream() {}
		StableReferenceStream(void* buffer, unsigned int _capacity) {
			InitializeFromBuffer(buffer, _capacity);
		}

		StableReferenceStream(const StableReferenceStream& other) = default;
		StableReferenceStream<T, queue_free_list>& operator = (const StableReferenceStream<T, queue_free_list>& other) = default;

		// Returns an index that can be used for the lifetime of the object to access it
		unsigned int Add(T element) {
			unsigned int index = ReserveOne();
			buffer[index] = element;
			return index;
		}

		// Returns an index that can be used for the lifetime of the object to access it
		unsigned int Add(const T* element) {
			unsigned int remapping_value = ReserveOne();
			buffer[remapping_value] = *element;
			return remapping_value;
		}

		// Returns an index that can be used for the lifetime of the object to access it
		ECS_INLINE bool AddSafe(T element, unsigned int& index) {
			if (size == capacity) {
				return false;
			}
			index = Add(element);
			return true;
		}

		ECS_INLINE void AddStream(Stream<T> elements, unsigned int* indices) {
			ECS_ASSERT(elements.size + size < capacity);
			for (size_t index = 0; index < elements.size; index++) {
				indices[index] = Add(elements[index]);
			}
		}

		unsigned int ReserveOne() {
			ECS_ASSERT(size < capacity);
			size++;
			unsigned int index;

			if constexpr (!queue_free_list) {
				index = free_list[capacity - size];
			}
			else {
				index = free_list[free_list_starting_index++];
				free_list_starting_index = free_list_starting_index == capacity ? 0 : free_list_starting_index;
			}

			return index;
		}

		// It will fill in the handles buffer with the new handles
		void Reserve(Stream<unsigned int> indices) {
			ECS_ASSERT(size + indices.size < capacity);
			unsigned int current_size = size;
			size += indices.size;
			for (size_t index = 0; index < indices.size; index++) {
				if constexpr (!queue_free_list) {
					indices[index] = free_list[capacity - current_size - index];
				}
				else {
					indices[index] = free_list[free_list_starting_index++];
					free_list_starting_index = free_list_starting_index == capacity ? 0 : free_list_starting_index;
				}
			}
		}

		// it will reset the free list
		ECS_INLINE void Reset() {
			size = 0;
			for (unsigned int index = 0; index < capacity; index++) {
				free_list[index] = index;
			}
		}

		// Add the index to the free list
		ECS_INLINE void Remove(unsigned int index) {
			if constexpr (!queue_free_list) {
				free_list[capacity - size] = index;
				size--;
			}
			else {
				unsigned int add_position = free_list_starting_index + capacity - size;
				add_position = add_position >= capacity ? add_position - capacity : add_position;
				free_list[add_position] = index;
				size--;
			}
		}

		ECS_INLINE const T& operator [](unsigned int index) const {
			return buffer[index];
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return buffer[index];
		}

		// It's stable
		ECS_INLINE T* ElementPointer(unsigned int index) {
			return buffer + index;
		}

		// It's stable
		ECS_INLINE const T* ElementPointer(unsigned int index) const {
			return buffer + index;
		}

		ECS_INLINE static size_t MemoryOf(unsigned int count) {
			size_t main_size = (sizeof(T) + sizeof(unsigned int)) * count;
			return main_size;
		}

		void InitializeFromBuffer(void* _buffer, unsigned int _capacity) {
			buffer = (T*)_buffer;
			capacity = _capacity;
			uintptr_t ptr = (uintptr_t)_buffer;
			ptr += sizeof(T) * capacity;
			free_list = (unsigned int*)ptr;
			Reset();
		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _capacity) {
			InitializeFromBuffer((void*)buffer, _capacity);
			buffer += MemoryOf(_capacity);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _capacity) {
			size_t memory_size = MemoryOf(_capacity);
			void* allocation = allocator->Allocate(memory_size, 8);
			InitializeFromBuffer(allocation, _capacity);
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity) {
			size_t memory_size = MemoryOf(_capacity);
			void* allocation = Allocate(allocator, memory_size, 8);
			InitializeFromBuffer(allocation, _capacity);
		}

		// Considers this stream to be already allocated. It will just memcpy into the buffers
		// Those buffers must have the same capacity. Otherwise it will fail
		void Copy(StableReferenceStream<T, queue_free_list> other) {
			ECS_ASSERT(other.capacity == capacity);

			memcpy(buffer, other.buffer, sizeof(T) * capacity);
			memcpy(free_list, other.free_list, sizeof(unsigned int) * capacity);
			free_list_starting_index = other.free_list_starting_index;
			size = other.size;
		}

		T* buffer;
		unsigned int* free_list;
		unsigned int free_list_starting_index;
		unsigned int size;
		unsigned int capacity;
	};


}