#pragma once
#include "..\Core.h"
#include "Stream.h"

namespace ECSEngine {

	// Miscellaneous is used as the exponent for the power of two range selector
	template<typename T, typename RangeSelector>
	struct Deck {
		Deck() : buffers({ nullptr }, 0), chunk_size(0), miscellaneous(0) {}
		Deck(
			const void* _buffers, 
			size_t _buffers_capacity,
			size_t _chunk_size, 
			size_t _miscellaneous = 0
		) : buffers(_buffers, 0, _buffers_capacity), chunk_size(_chunk_size), miscellaneous(_miscellaneous) {}

		Deck(const Deck& other) = default;
		Deck& operator = (const Deck& other) = default;

		size_t Add(T element) {
			// The available chunks are completely full
			if (chunks_with_elements.size == 0) {
				// The deck is completely full, need to reallocate the buffer of buffers and the chunks with elements
				if (buffers.size == buffers.capacity) {
					Deallocate(buffers.allocator, chunks_with_elements.buffer);
					buffers.Resize((size_t)((float)buffers.capacity * ECS_RESIZABLE_STREAM_FACTOR + 1.0f));
					chunks_with_elements.Initialize(buffers.allocator, buffers.capacity);
				}

				// A new chunk must be allocated
				void* chunk_allocation = Allocate(buffers.allocator, sizeof(T) * chunk_size);
				buffers[buffers.size] = { chunk_allocation, 0, (unsigned int)chunk_size };
				chunks_with_elements.Add(buffers.size);
				buffers.size++;
			}

			unsigned int index = buffers[chunks_with_elements[0]].Add(element);
			size_t previous_elements_count = (size_t)chunks_with_elements[0] * chunk_size;

			if (buffers[chunks_with_elements[0]].size == buffers[chunks_with_elements[0]].capacity) {
				chunks_with_elements.Remove(0);
			}
			return previous_elements_count + (size_t)index;
		}

		size_t Add(const T* element) {
			// The available chunks are completely full
			if (chunks_with_elements.size == 0) {
				// The deck is completely full, need to reallocate the buffer of buffers and the chunks with elements
				if (buffers.size == buffers.capacity) {
					buffers.allocator->Deallocate(chunks_with_elements.buffer);
					buffers.Resize((size_t)((float)buffers.capacity * ECS_RESIZABLE_STREAM_FACTOR + 1.0f));
					chunks_with_elements.Initialize(buffers.allocator, buffers.capacity);
				}

				// A new chunk must be allocated
				void* chunk_allocation = buffers.allocator->Allocate(sizeof(T) * chunk_size);
				buffers[buffers.size] = { chunk_allocation, 0, chunk_size };
				chunks_with_elements.Add(buffers.size);
				buffers.size++;
			}

			unsigned int index = buffers[chunks_with_elements[0]].Add(element);
			size_t previous_elements_count = (size_t)chunks_with_elements[0] * chunk_size;

			if (buffers[chunks_with_elements[0]].size == buffers[chunks_with_elements[0]].capacity) {
				chunks_with_elements.Remove(0);
			}
			return previous_elements_count + (size_t)index;
		}

		void AddStream(Stream<T> other) {
			ReserveNewElements(other.size);
			for (size_t index = 0; index < other.size; index++) {
				Add(other[index]);
			}
		}

		// Allocates memory for extra count chunks
		void AllocateChunks(size_t count) {
			if (buffers.size + count > buffers.capacity) {
				Resize((unsigned int)((float)buffers.capacity * ECS_RESIZABLE_STREAM_FACTOR));
			}

			for (size_t index = buffers.size; index < buffers.size + count; index++) {
				void* allocation = Allocate(buffers.allocator, buffers[index].MemoryOf(chunk_size));
				buffers[index].InitializeFromBuffer(allocation, 0, chunk_size);
				chunks_with_elements.Add(index);
			}
			buffers.size += count;
		}

		void Copy(const void* memory, size_t count) {
			ResizeNoCopyElementCount(count);
			size_t written_count = 0;
			uintptr_t ptr = (uintptr_t)memory;
			for (size_t index = 0; index < buffers.size; index++) {
				size_t writes_to_remain = count - written_count;
				size_t write_count = chunk_size < writes_to_remain ? chunk_size : writes_to_remain;
				memcpy(buffers[index].buffer, (void*)ptr, sizeof(T) * write_count);
				buffers[index].size += write_count;
				written_count += write_count;
				ptr += sizeof(T) * write_count;
			}
			chunks_with_elements.size = buffers[buffers.size - 1].size < buffers[buffers.size - 1].capacity;
			chunks_with_elements[0] = buffers.size - 1;
		}

		void Copy(Stream<T> other) {
			Copy(other.buffer, other.size);
		}

		void CopyTo(void* memory) const {
			uintptr_t ptr = (uintptr_t)memory;
			CopyTo(ptr);
		}

		void CopyTo(uintptr_t& memory) const {
			for (size_t index = 0; index < buffers.size; index++) {
				memcpy((void*)memory, buffers[index].buffer, sizeof(T) * buffers[index].size);
				memory += sizeof(T) * buffers[index].size;
			}
		}

		size_t GetElementCount() const {
			size_t count = 0;
			for (size_t index = 0; index < buffers.size; index++) {
				count += buffers[index].size;
			}
			return count;
		}

		T* GetEntry() {
			T* value;
			ReserveNewElements(1);
			unsigned int index = buffers[chunks_with_elements[0]].size++;
			value = buffers[chunks_with_elements[0]].buffer + index;

			if (buffers[chunks_with_elements[0]].size == buffers[chunks_with_elements[0]].capacity) {
				chunks_with_elements.Remove(0);
			}

			return value;
		}

		// Returns a pointer to a contiguous portion with count elements 
		// Nullptr will be returned if there is no such zone in the current buffers
		// It does not allocate new chunks
		T* GetEntries(size_t count) {
			ECS_ASSERT(count <= chunk_size);

			for (size_t index = 0; index < chunks_with_elements.size; index++) {
				unsigned int difference = buffers[chunks_with_elements[0]].capacity - buffers[chunks_with_elements[0]].size;
				if (difference >= count) {
					unsigned int offset = buffers[chunks_with_elements[0]].size;
					buffers[chunks_with_elements[0]].size += count;
					if (difference == count) {
						chunks_with_elements.RemoveSwapBack(0);
					}
					return buffers[chunks_with_elements[0]].buffer + offset;
				}
			}

			return nullptr;
		}

		// Returns a pointer to a contiguous portion with count elements
		// It does allocate a new block if it doesn't find such a zone in the current buffers
		T* GetEntriesWithAllocation(size_t count) {
			ECS_ASSERT(count <= chunk_size);

			for (size_t index = 0; index < chunks_with_elements.size; index++) {
				unsigned int difference = buffers[chunks_with_elements[0]].capacity - buffers[chunks_with_elements[0]].size;
				if (difference >= count) {
					unsigned int offset = buffers[chunks_with_elements[0]].size;
					buffers[chunks_with_elements[0]].size += count;
					if (difference == count) {
						chunks_with_elements.RemoveSwapBack(0);
					}
					return buffers[chunks_with_elements[0]].buffer + offset;
				}
			}

			// Allocate a new chunk
			if (buffers.size == buffers.capacity) {
				Resize((unsigned int)((float)buffers.capacity * ECS_RESIZABLE_STREAM_FACTOR));
			}

			AllocateChunks(1);
			T* pointer = buffers[buffers.size - 1].buffer;
			buffers[buffers.size - 1].size += count;
			// If a new chunk was requested, remove it from the available chunks
			chunks_with_elements.size -= count == chunk_size;
		}

		// It will set the capacity to 0
		void FreeChunks() {
			for (size_t index = 0; index < buffers.size; index++) {
				Deallocate(buffers.allocator, buffers[index].buffer);
			}
			Deallocate(buffers.allocator, chunks_with_elements.buffer);
			buffers.FreeBuffer();
		}

		void RecalculateFreeChunks() {
			chunks_with_elements.size = 0;
			for (size_t index = 0; index < buffers.size; index++) {
				if (buffers[index].size < buffers[index].capacity) {
					chunks_with_elements.Add(index);
				}
			}
		}

		void Reset() {
			for (size_t index = 0; index < buffers.size; index++) {
				buffers[index].size = 0;
				chunks_with_elements[index] = index;
			}
			chunks_with_elements.size = buffers.size;
		}

		void RemoveSwapBack(size_t index) {
			size_t chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			size_t in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			buffers[chunk_index].RemoveSwapBack(in_chunk_index);
		}

		// Makes sure there is enough space for extra count elements
		// The new elements can be added using Add()
		void ReserveNewElements(size_t count) {
			size_t available_elements = 0;
			for (size_t index = 0; index < buffers.size && available_elements < count; index++) {
				available_elements += buffers[index].capacity - buffers[index].size;
			}

			// If there are more available elements than the count, do nothing
			// More chunks need to be allocated only if the available elements are less
			// than the count
			if (available_elements < count) {
				size_t difference = count - available_elements;
				size_t new_chunks = difference / chunk_size + (difference % chunk_size != 0);
				AllocateChunks(new_chunks);
			}
		}

		void ResizeElementCount(unsigned int element_count) {
			Resize(element_count / chunk_size + (element_count % chunk_size != 0));
		}

		void Resize(unsigned int new_chunk_count) {
			// Get the chunk difference
			unsigned int chunk_difference = new_chunk_count - buffers.size;

			unsigned int old_count = buffers.size;

			if (new_chunk_count > buffers.capacity) {
				// Keep the old valid chunks and copy them later into the new buffer
				unsigned int* old_counts = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * buffers.size);
				for (size_t index = 0; index < chunks_with_elements.size; index++) {
					old_counts[index] = chunks_with_elements[index];
				}

				buffers.Resize(new_chunk_count);
				Deallocate(buffers.allocator, chunks_with_elements.buffer);
				chunks_with_elements = { Allocate(buffers.allocator, sizeof(unsigned int) * new_chunk_count), new_chunk_count };
				// Copy the old counts
				for (size_t index = 0; index < old_count; index++) {
					chunks_with_elements[index] = old_counts[index];
				}

				for (size_t index = old_count; index < new_chunk_count; index++) {
					chunks_with_elements[index] = index;
				}
			}
		}

		void ResizeNoCopyElementCount(size_t element_count) {
			ResizeNoCopy(element_count / chunk_size + (element_count % chunk_size != 0));
		}

		// It will not copy the elements, they will be lost
		void ResizeNoCopy(size_t new_chunk_count) {
			// Deallocate every chunk
			for (size_t index = 0; index < buffers.size; index++) {
				Deallocate(buffers.allocator, buffers[index].buffer);
			}

			// Deallocate the available chunks
			Deallocate(buffers.allocator, chunks_with_elements.buffer);

			// Reallocate the available chunks
			chunks_with_elements = { Allocate(buffers.allocator, sizeof(unsigned int) * new_chunk_count), new_chunk_count };
			for (size_t index = 0; index < new_chunk_count; index++) {
				chunks_with_elements[index] = index;
			}

			buffers.ResizeNoCopy(new_chunk_count);
			// Allocate every chunk
			for (size_t index = 0; index < new_chunk_count; index++) {
				buffers[index] = { Allocate(buffers.allocator, sizeof(T) * chunk_size), 0, chunk_size };
			}
			buffers.size = new_chunk_count;
		}

		void Swap(size_t first, size_t second) {
			T copy = operator[](first);
			operator[](first) = operator[](second);
			operator[](second) = copy;
		}

		void Trim() {
			// Get the current count of elements
			size_t total_element_count = 0;
			for (size_t index = 0; index < buffers.size; index++) {
				total_element_count += buffers[index].size;
			}

			// Calculate the chunk count
			size_t chunk_count = total_element_count / chunk_size + total_element_count % chunk_size != 0;
			
			// If the chunk count is smaller than the current count, release the appropriate chunks
			for (size_t index = chunk_count; index < buffers.size; index++) {
				Deallocate(buffers.allocator, buffers[index].buffer);

				size_t copied_count = 0;
				// Shift elements from the upper chunks into the lower ones
				for (size_t subindex = 0; subindex < chunk_count && copied_count < buffers[index].size; subindex++) {
					size_t current_capacity = buffers[subindex].capacity - buffers[subindex].size;
					if (current_capacity > 0) {
						// Check the memcpy count
						size_t upper_buffer_count = buffers[index].size - copied_count;
						size_t copy_element_count = upper_buffer_count < current_capacity ? upper_buffer_count : current_capacity;

						memcpy(buffers[subindex].buffer + buffers[subindex].size, buffers[index].buffer, copy_element_count * sizeof(T));
						buffers[subindex].size += copy_element_count;
						copied_count += copy_element_count;
					}
				}
			}

			// Reset the valid chunks stream
			chunks_with_elements.size = 0;
			for (size_t index = 0; index < chunk_count; index++) {
				if (buffers[index].size < buffers[index].capacity) {
					chunks_with_elements.Add(index);
				}
			}

			buffers.capacity = chunk_count;
			buffers.size = chunk_count;
		}

		// it will leave another additional_elements over the current size
		void Trim(unsigned int additional_elements) {
			// Get the current count of elements
			size_t total_element_count = 0;
			for (size_t index = 0; index < buffers.size; index++) {
				total_element_count += buffers[index].size;
			}
			total_element_count += additional_elements;

			// Calculate the chunk count
			size_t total_chunk_count = total_element_count / chunk_size + total_element_count % chunk_size != 0;
			total_element_count -= additional_elements;
			size_t chunk_count = total_element_count / chunk_size + total_element_count % chunk_size != 0;

			// If the total chunk count is smaller than the current count, release the appropriate chunks
			for (size_t index = total_chunk_count; index < buffers.size; index++) {
				Deallocate(buffers.allocator, buffers[index].buffer);
			}

			for (size_t index = chunk_count; index < buffers.size; index++) {
				size_t copied_count = 0;
				// Shift elements from the upper chunks into the lower ones
				for (size_t subindex = 0; subindex < chunk_count && copied_count < buffers[index].size; subindex++) {
					size_t current_capacity = buffers[subindex].capacity - buffers[subindex].size;
					if (current_capacity > 0) {
						// Check the memcpy count
						size_t upper_buffer_count = buffers[index].size - copied_count;
						size_t copy_element_count = upper_buffer_count < current_capacity ? upper_buffer_count : current_capacity;

						memcpy(buffers[subindex].buffer + buffers[subindex].size, buffers[index].buffer, copy_element_count * sizeof(T));
						buffers[subindex].size += copy_element_count;
						copied_count += copy_element_count;
					}
				}
			}

			// Reset the valid chunks stream
			chunks_with_elements.size = 0;
			for (size_t index = 0; index < total_chunk_count; index++) {
				if (buffers[index].size < buffers[index].capacity) {
					chunks_with_elements.Add(index);
				}
			}

			buffers.capacity = total_chunk_count;
			buffers.size = total_chunk_count;
		}

		ECS_INLINE T& operator [](size_t index) {
			unsigned int chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			unsigned int in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			return buffers[chunk_index][in_chunk_index];
		}

		ECS_INLINE const T& operator [](size_t index) const {
			size_t chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			size_t in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			return buffers[chunk_index][in_chunk_index];
		}

		void Initialize(AllocatorPolymorphic _allocator, size_t _initial_chunk_count, size_t _chunk_size, size_t _miscellaneous = 0) {
			chunk_size = _chunk_size;
			miscellaneous = _miscellaneous;
			buffers.Initialize(_allocator, _initial_chunk_count);
			buffers.size = _initial_chunk_count;
			chunks_with_elements = { Allocate(_allocator, sizeof(unsigned int) * _initial_chunk_count), 0 };
			for (size_t index = 0; index < _initial_chunk_count; index++) {
				void* chunk = Allocate(_allocator, sizeof(T) * chunk_size);
				buffers[index] = { chunk, 0, (unsigned int)chunk_size };
				chunks_with_elements.Add((unsigned int)index);
			}
		}

		ResizableStream<CapacityStream<T>> buffers;
		Stream<unsigned int> chunks_with_elements;
		size_t chunk_size;
		size_t miscellaneous;
	};

	struct DeckRangeModulo {
		ECS_INLINE static size_t Chunk(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index / chunk_size;
		}

		ECS_INLINE static size_t Buffer(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index % chunk_size;
		}
	};

	struct DeckRangePowerOfTwo {
		ECS_INLINE static size_t Chunk(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index >> miscellaneous;
		}

		ECS_INLINE static size_t Buffer(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index & (chunk_size - 1);
		}
	};

	template<typename T>
	using DeckModulo = Deck<T, DeckRangeModulo>;

	template<typename T>
	using DeckPowerOfTwo = Deck<T, DeckRangePowerOfTwo>;

}