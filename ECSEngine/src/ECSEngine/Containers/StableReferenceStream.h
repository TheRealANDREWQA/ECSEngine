#pragma once
#include "../Core.h"
#include "Stream.h"

namespace ECSEngine {

	namespace containers {

		// This is basically a pooled list
		// It keeps an array of free indices
		// Set boolean_flags to true if you want to iterate through the stream
		// It will keep a compacted frequency bit mask - every element gets a bit
		// To reduce the memory footprint and the memory bandwidth
		template<typename T, bool activate_frequency_counting = false>
		struct StableReferenceStream {
			StableReferenceStream() {}
			StableReferenceStream(void* buffer, unsigned int _capacity) {
				InitializeFromBuffer(buffer, _capacity);
			}

			StableReferenceStream(const StableReferenceStream& other) = default;
			StableReferenceStream<T, activate_frequency_counting>& operator = (const StableReferenceStream<T, activate_frequency_counting>& other) = default;

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
			bool AddSafe(T element, unsigned int& index) {
				if (size == capacity) {
					return false;
				}
				index = Add(element);
				return true;
			}

			void AddStream(Stream<T> elements, unsigned int* indices) {
				ECS_ASSERT(elements.size + size < capacity);
				for (size_t index = 0; index < elements.size; index++) {
					indices[index] = Add(elements[index]);
				}
			}

			unsigned int ReserveOne() {
				ECS_ASSERT(size < capacity);
				size++;
				unsigned int index = free_list[capacity - size];
				if constexpr (activate_frequency_counting) {
					SetFrequency(index);
				}
				return index;
			}

			// It will fill in the handles buffer with the new handles
			void Reserve(Stream<unsigned int> indices) {
				ECS_ASSERT(size + indices.size < capacity);
				unsigned int current_size = size;
				size += indices.size;
				for (size_t index = 0; index < indices.size; index++) {
					indices[index] = free_list[capacity - current_size - index];
				}
			}

			// it will reset the free list
			void Reset() {
				size = 0;
				for (unsigned int index = 0; index < capacity; index++) {
					free_list[index] = index;
				}

				if constexpr (activate_frequency_counting) {
					memset(is_item_at, 0, (capacity >> 3) + 1);
				}
			}

			// Add the index to the free list
			void Remove(unsigned int index) {
				free_list[capacity - size] = index;
				if constexpr (activate_frequency_counting) {
					ClearFrequency(index);
				}
				size--;
			}

			void SetFrequency(unsigned int index) {
				if constexpr (activate_frequency_counting) {
					unsigned int byte_index = index >> 3;
					unsigned int bit_index = index & 7;
					is_item_at[byte_index] |= 1 << bit_index;
				}
				else {
					ECS_ASSERT(false);
				}
			}

			void ClearFrequency(unsigned int index) {
				if constexpr (activate_frequency_counting) {
					unsigned int byte_index = index >> 3;
					unsigned int bit_index = index & 7;
					is_item_at[byte_index] &= ~(1 << bit_index);
				}
				else {
					ECS_ASSERT(false);
				}
			}

			bool IsItemAt(unsigned int index) {
				if constexpr (activate_frequency_counting) {
					unsigned int byte_index = index >> 3;
					unsigned int bit_index = index & 7;
					return (is_item_at[byte_index] & (1 << bit_index)) != 0;
				}
				else {
					ECS_ASSERT(false);
					return false;
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

			static size_t MemoryOf(unsigned int count) {
				size_t main_size = (sizeof(T) + sizeof(unsigned int)) * count;
				if constexpr (activate_frequency_counting) {
					// The number of bytes needed to keep the frequency array
					// Every element gets a bit
					main_size += (count >> 3) + 1;
				}
				return main_size;
			}

			void InitializeFromBuffer(void* _buffer, unsigned int _capacity) {
				buffer = (T*)_buffer;
				capacity = _capacity;
				uintptr_t ptr = (uintptr_t)_buffer;
				ptr += sizeof(T) * capacity;
				free_list = (unsigned int*)ptr;

				if constexpr (activate_frequency_counting) {
					ptr += sizeof(unsigned int) * _capacity;
					is_item_at = (char*)ptr;
				}

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

			T* buffer;
			unsigned int* free_list;
			char* is_item_at;
			unsigned int size;
			unsigned int capacity;
		};

	}

}