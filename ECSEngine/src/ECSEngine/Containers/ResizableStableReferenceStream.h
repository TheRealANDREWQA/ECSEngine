#pragma once
#include "../Core.h"
#include "StableReferenceStream.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "Stream.h"

#define ECS_RESIZABLE_STABLE_REFERENCE_STREAM_GROWTH_FACTOR 1.5f

namespace ECSEngine {

	namespace containers {

		template<typename T, bool activate_frequency_counting = false>
		struct ResizableStableReferenceStream {
			ResizableStableReferenceStream() {}
			ResizableStableReferenceStream(AllocatorPolymorphic _allocator, unsigned int _capacity) : allocator(_allocator)  {
				if (_capacity > 0) {
					void* allocation = Allocate(allocator, stream.MemoryOf(_capacity));
					stream.InitializeFromBuffer(allocation, 0, _capacity);
				}
				else {
					stream.InitializeFromBuffer(nullptr, 0, 0);
				}
			}

			ResizableStableReferenceStream(const ResizableStableReferenceStream& other) = default;
			ResizableStableReferenceStream<T, activate_frequency_counting>& operator = (const ResizableStableReferenceStream<T, activate_frequency_counting>& other) = default;

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

			void AddStream(Stream<T> elements, unsigned int* indices) {
				Reserve({ indices, elements.size });
			}

			// Only use if new capacity greater than the current size
			void SetNewCapacity(size_t new_capacity) {
				// Create a new buffer
				void* allocation = Allocate(allocator, stream.MemoryOf(new_capacity));

				// Copy all the values into the new buffer
				memcpy(allocation, stream.buffer, sizeof(T) * stream.capacity);
				// For the free list, the indices are written in order capacity - size
				// So the first difference slots should be written - write the values according to the indices that are free
				unsigned int* free_list_indices = (unsigned int*)((uintptr_t)allocation + sizeof(T) * stream.capacity);
				char* is_item_at = (char*)((uintptr_t)free_list_indices + sizeof(unsigned int) * stream.capacity);

				size_t difference = new_capacity - stream.capacity;
				for (size_t index = 0; index < difference; index++) {
					free_list_indices[index] = index + difference;
				}

				Deallocate(allocator, stream.buffer);
				stream.buffer = allocation;
				stream.free_list = free_list_indices;

				if constexpr (activate_frequency_counting) {
					// Writing the frequency list - full of ones because all elements are alive
					size_t byte_count_to_write = (new_capacity & ~7) >> 3;
					memset(is_item_at, 0xFF, byte_count_to_write);
					memset(is_item + byte_count_to_write, 0, (new_capacity >> 3) + 1 - byte_count_to_write);
					for (size_t index = byte_count_to_write << 3; index < new_capacity; index++) {
						is_item_at[byte_count_to_write] |= (1 << (index & 7));
					}
					stream.is_item_at = is_item_at;
				}
				stream.capacity = new_capacity;
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
			void Reset() {
				stream.Reset();
			}

			// Add the index to the free list
			void Remove(unsigned int index) {
				stream.Remove(index);
			}

			ECS_INLINE const T& operator [](unsigned int index) const {
				return stream[index];
			}

			ECS_INLINE T& operator [](unsigned int index) {
				return stream[index];
			}

			static size_t MemoryOf(unsigned int count) {
				return StableReferenceStream<T>::MemoryOf(count);
			}

			void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity) {
				allocator = _allocator;
				stream.Initialize(allocator, _capacity);
			}

			void FreeBuffer() {
				if (stream.buffer != nullptr) {
					Deallocate(allocator, stream.buffer);
				}

				stream.buffer = nullptr;
				stream.is_item_at = nullptr;
				stream.free_list = nullptr;
				stream.size = 0;
				stream.capacity = 0;
			}

			// It will determine how many elements at the end are freed and will move all the existing elements
			// to a new buffer of that size
			// The threshold tell the function how many elements are needed to trigger the trim
			// Since trimming a single element or a handful is not that helpful
			void Trim(unsigned int threshold = 1) {
				// Use a frequency buffer to determine how many elements at the end are freed
				const size_t MAX_STACK_ELEMENTS = ECS_KB * 64;
				bool* frequency = nullptr;
				if (stream.capacity > MAX_STACK_ELEMENTS) {
					frequency = (bool*)ECS_MALLOCA(sizeof(bool) * stream.capacity);
				}
				else {
					frequency = (bool*)ECS_STACK_ALLOC(sizeof(bool) * stream.capacity);
				}
				memset(frequency, 0, sizeof(bool) * stream.capacity);

				for (size_t index = stream.size; index < stream.capacity; index++) {
					frequency[stream.free_list[stream.capacity - index]] = true;
				}

				size_t index = stream.capacity - 1;
				while (frequency[index]) {
					index--;
				}
				if (stream.capacity - 1 - index > threshold) {
					void* allocation = Allocate(allocator, stream.MemoryOf(index), alignof(T));
					// Optimization heurestic - if the count of elements is smaller than half of the elements
					// to be copied, just copy those elements (determining which element is valid inside the stream
					// is being done with the frequency array that was just populated)
					T* old_buffer = stream.buffer;
					unsigned int* old_free_list = stream.free_list;

					stream.buffer = (T*)allocation;
					stream.free_list = (unsigned int*)((uintptr_t)allocation + sizeof(T) * index);
					unsigned int free_list_index = 0;

					if (stream.size <= index >> 1) {
						// Copy element by element
						for (size_t element_index = 0; element_index < index; element_index) {
							if (frequency[element_index]) {
								stream[element_index] = old_buffer[element_index];
							}
							else {
								stream[free_list_index++] = element_index;
							}
						}
					}
					else {
						// Blit the data to the new buffer
						memcpy(stream.buffer, old_buffer, sizeof(T) * index);

						// Copy the free list indices from the frequency list
						for (size_t element_index = 0; element_index < index; element_index) {
							if (!frequency[element_index]) {
								stream[free_list_index++] = element_index;
							}
						}
					}

					stream.capacity = index;
				}

				if (stream.capacity > MAX_STACK_ELEMENTS) {
					ECS_FREEA(frequency);
				}
			}

			StableReferenceStream<T, activate_frequency_counting> stream;
			AllocatorPolymorphic allocator;
		};

	}

}