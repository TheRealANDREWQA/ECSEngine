#pragma once
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/Utilities.h"

namespace ECSEngine {

	// Miscellaneous is used as the exponent for the power of two range selector
	template<typename T, typename RangeSelector>
	struct Deck {
		typedef T T;

		ECS_INLINE Deck() : buffers({ nullptr }, 0), chunk_size(0), miscellaneous(0) {}
		ECS_INLINE Deck(
			const void* _buffers, 
			size_t _buffers_capacity,
			size_t _chunk_size, 
			size_t _miscellaneous = 0
		) : buffers(_buffers, 0, _buffers_capacity), chunk_size(_chunk_size), miscellaneous(_miscellaneous) {}

		Deck(const Deck& other) = default;
		Deck& operator = (const Deck& other) = default;

	private:
		template<typename ElementType>
		size_t AddImpl(ElementType element_type) {
			// The available chunks are completely full
			if (chunks_with_space.size == 0) {
				// The deck is completely full, need to reallocate the buffer of buffers and the chunks with elements
				if (buffers.size == buffers.capacity) {
					if (buffers.capacity > 0) {
						chunks_with_space.Deallocate(buffers.allocator);
					}
					buffers.Resize((size_t)((float)buffers.capacity * ECS_RESIZABLE_STREAM_FACTOR + 2.0f));
					chunks_with_space.Initialize(buffers.allocator, buffers.capacity);
					// Make the size 0
					chunks_with_space.size = 0;
				}

				// A new chunk must be allocated
				void* chunk_allocation = AllocateEx(buffers.allocator, sizeof(T) * chunk_size);
				unsigned int add_position = buffers.Add({ chunk_allocation, 0, (unsigned int)chunk_size });
				chunks_with_space.Add(add_position);
			}

			unsigned int index = buffers[chunks_with_space[0]].Add(element_type);
			size_t previous_elements_count = (size_t)chunks_with_space[0] * chunk_size;

			if (buffers[chunks_with_space[0]].size == buffers[chunks_with_space[0]].capacity) {
				chunks_with_space.Remove(0);
			}
			return previous_elements_count + (size_t)index;
		}

	public:

		ECS_INLINE size_t Add(T element) {
			return AddImpl(element);
		}

		ECS_INLINE size_t Add(const T* element) {
			return AddImpl(element);
		}

		ECS_INLINE void AddStream(Stream<T> other) {
			ReserveNewElements(other.size);
			for (size_t index = 0; index < other.size; index++) {
				Add(other[index]);
			}
		}

		void AddDeck(const Deck<T, RangeSelector>& deck) {
			size_t deck_size = deck.GetElementCount();
			ReserveNewElements(deck_size);
			for (size_t index = 0; index < deck_size; index++) {
				Add(deck[index]);
			}
		}

		// Allocates memory for extra count chunks
		void AllocateChunks(size_t count) {
			if (buffers.size + count > buffers.capacity) {
				Resize((unsigned int)((float)buffers.capacity * ECS_RESIZABLE_STREAM_FACTOR + 2.0f));
			}

			for (size_t index = buffers.size; index < buffers.size + count; index++) {
				void* allocation = AllocateEx(buffers.allocator, buffers[index].MemoryOf(chunk_size));
				buffers[index].InitializeFromBuffer(allocation, 0, chunk_size);
				chunks_with_space.Add(index);
			}
			buffers.size += count;
		}

		Deck<T, RangeSelector> Copy(AllocatorPolymorphic allocator) const {
			Deck<T, RangeSelector> copy;
			copy.Initialize(allocator, 0, chunk_size, miscellaneous);
			copy.AddDeck(*this);
			return copy;
		}

		void CopyOther(const void* memory, size_t count) {
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
			chunks_with_space.size = buffers[buffers.size - 1].size < buffers[buffers.size - 1].capacity;
			chunks_with_space[0] = buffers.size - 1;
		}

		ECS_INLINE void CopyOther(Stream<T> other) {
			Copy(other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			uintptr_t ptr = (uintptr_t)memory;
			CopyTo(ptr);
		}

		ECS_INLINE void CopyTo(uintptr_t& memory) const {
			for (size_t index = 0; index < buffers.size; index++) {
				memcpy((void*)memory, buffers[index].buffer, sizeof(T) * buffers[index].size);
				memory += sizeof(T) * buffers[index].size;
			}
		}

		void Deallocate() {
			for (unsigned int index = 0; index < buffers.size; index++) {
				ECSEngine::DeallocateEx(buffers.allocator, buffers[index].buffer);
			}
			buffers.FreeBuffer();
			chunks_with_space.Deallocate(buffers.allocator);
			chunks_with_space = {};
		}

		bool Exists(size_t index) const {
			size_t chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			size_t in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			// Search for the chunk index inside the chunks_with_space
			if (SearchBytes(chunks_with_space, (unsigned int)chunk_index) == -1) {
				return false;
			}

			return in_chunk_index < buffers[chunk_index].size;
		}

		// The functor receives a uint2 which has in the x component
		// the chunk index and in the y the stream index for that chunk
		// Return true to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachIndex(Functor&& functor) const {
			for (unsigned int index = 0; index < buffers.size; index++) {
				for (unsigned int subindex = 0; subindex < buffers[index].size; subindex++) {
					uint2 indices = { index, subindex };
					if constexpr (early_exit) {
						if (functor(indices)) {
							return true;
						}
					}
					else {
						functor(indices);
					}
				}
			}
			return false;
		}

		// The functor receives a uint2 which has in the x component
		// the chunk index and in the y the stream index for that chunk
		// Return true when you want to delete an item, else false
		// Don't perform the erase by yourself, this function will do it
		// Returns true if it early exited, else false
		template<typename Functor>
		bool ForEachIndexWithErase(Functor&& functor) {
			for (unsigned int index = 0; index < buffers.size; index++) {
				for (unsigned int subindex = 0; subindex < buffers[index].size; subindex++) {
					uint2 indices = { index, subindex };
					if constexpr (early_exit) {
						if (functor(indices)) {
							return true;
						}
					}
					else {
						if (functor(indices)) {
							buffers[index].RemoveSwapBack(subindex);
							subindex--;
						}
					}
				}
			}
			return false;
		}

		// Return true to early exit, else false (for the early exit mode)
		// Return true when you delete an item, else false (when normal iteration -
		// in this mode you can erase elements while you are iterating - but don't
		// remove the elements by yourself, this function will do it!)
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			for (size_t index = 0; index < buffers.size; index++) {
				for (unsigned int subindex = 0; subindex < buffers[index].size; subindex++) {
					T* element = &buffers[index][subindex];
					if constexpr (early_exit) {
						if (functor(*element)) {
							return true;
						}
					}
					else {
						if (functor(*element)) {
							buffers[index].RemoveSwapBack(subindex);
							subindex--;
						}
					}
				}
			}
			return false;
		}

		// CONST VARIANT
		// Return true to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) const {
			for (size_t index = 0; index < buffers.size; index++) {
				for (unsigned int subindex = 0; subindex < buffers[index].size; subindex++) {
					const T* element = &buffers[index][subindex];
					if constexpr (early_exit) {
						if (functor(*element)) {
							return true;
						}
					}
					else {
						functor(*element);
					}
				}
			}
			return false;
		}

		// Use this method in case you have the indices valid or from iterating and storing the values
		ECS_INLINE T GetValue(size_t chunk_index, size_t in_chunk_index) const {
			return buffers[chunk_index][in_chunk_index];
		}

		ECS_INLINE T* GetValuePtr(size_t chunk_index, size_t in_chunk_index) {
			return &buffers[chunk_index][in_chunk_index];
		}

		ECS_INLINE const T* GetValuePtr(size_t chunk_index, size_t in_chunk_index) const {
			return &buffers[chunk_index][in_chunk_index];
		}

		ECS_INLINE size_t GetChunkCount() const {
			return buffers.size;
		}

		size_t GetElementCount() const {
			size_t count = 0;
			for (size_t index = 0; index < buffers.size; index++) {
				count += buffers[index].size;
			}
			return count;
		}

		void RecalculateFreeChunks() {
			chunks_with_space.size = 0;
			for (size_t index = 0; index < buffers.size; index++) {
				if (buffers[index].size < buffers[index].capacity) {
					chunks_with_space.Add(index);
				}
			}
		}

		void Reset() {
			for (size_t index = 0; index < buffers.size; index++) {
				buffers[index].size = 0;
				chunks_with_space[index] = index;
			}
			chunks_with_space.size = buffers.size;
		}

		void RemoveSwapBack(size_t index) {
			size_t chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			size_t in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			buffers[chunk_index].RemoveSwapBack(in_chunk_index);
		}

		// Returns a new entry index. The boolean controls whether or not it will
		// Make a new allocation in case there is not enough space. If it doesn't
		// make an allocation and there is no empty entry, it will return -1
		size_t ReserveIndex(bool allocate_if_not_present = true) {
			return ReserveIndices(1, allocate_if_not_present);
		}

		// Returns the starting index for a range large enough for the count
		// The boolean controls whether or not it will make an allocation in
		// case there is no such range available. Returns -1 in that case
		size_t ReserveIndices(size_t count, bool allocate_if_not_available) {
			ECS_ASSERT(count <= chunk_size);

			for (size_t index = 0; index < chunks_with_space.size; index++) {
				unsigned int difference = buffers[chunks_with_space[index]].capacity - buffers[chunks_with_space[index]].size;
				if (difference >= count) {
					unsigned int offset = buffers[chunks_with_space[index]].size;
					buffers[chunks_with_space[index]].size += count;
					size_t final_index = chunks_with_space[index] * chunk_size + offset;
					if (difference == count) {
						chunks_with_space.RemoveSwapBack(index);
					}
					return final_index;
				}
			}

			if (allocate_if_not_available) {
				AllocateChunks(1);
				T* pointer = buffers[buffers.size - 1].buffer;
				buffers[buffers.size - 1].size += count;
				// If a new chunk was requested and it is entirely occupated, remove it from the available chunks
				chunks_with_space.size -= count == chunk_size;
				return (buffers.size - 1) * chunk_size;
			}
			return -1;
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
				unsigned int old_chunk_count = chunks_with_space.size;
				unsigned int* old_counts = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * old_chunk_count);
				for (size_t index = 0; index < old_chunk_count; index++) {
					old_counts[index] = chunks_with_space[index];
				}

				bool should_deallocate_buffers = buffers.capacity > 0;
				buffers.Resize(new_chunk_count);
				if (should_deallocate_buffers) {
					chunks_with_space.Deallocate(buffers.allocator);
				}
				chunks_with_space = { AllocateEx(buffers.allocator, sizeof(unsigned int) * new_chunk_count), old_chunk_count };
				// Copy the old counts
				for (size_t index = 0; index < old_chunk_count; index++) {
					chunks_with_space[index] = old_counts[index];
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
				ECSEngine::DeallocateEx(buffers.allocator, buffers[index].buffer);
			}

			if (buffers.capacity > 0) {
				// Deallocate the available chunks
				chunks_with_space.Deallocate(buffers.allocator);
			}

			// Reallocate the available chunks
			chunks_with_space = { AllocateEx(buffers.allocator, sizeof(unsigned int) * new_chunk_count), new_chunk_count };
			for (size_t index = 0; index < new_chunk_count; index++) {
				chunks_with_space[index] = index;
			}

			buffers.ResizeNoCopy(new_chunk_count);
			// Allocate every chunk
			for (size_t index = 0; index < new_chunk_count; index++) {
				buffers[index].InitializeFromBuffer(AllocateEx(buffers.allocator, sizeof(T) * chunk_size), 0, chunk_size);
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
				ECSEngine::DeallocateEx(buffers.allocator, buffers[index].buffer);

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
			chunks_with_space.size = 0;
			for (size_t index = 0; index < chunk_count; index++) {
				if (buffers[index].size < buffers[index].capacity) {
					chunks_with_space.Add(index);
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
				ECSEngine::DeallocateEx(buffers.allocator, buffers[index].buffer);
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
			chunks_with_space.size = 0;
			for (size_t index = 0; index < total_chunk_count; index++) {
				if (buffers[index].size < buffers[index].capacity) {
					chunks_with_space.Add(index);
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

		// For power of two deck, miscellaneous is the power of two exponent
		void Initialize(AllocatorPolymorphic _allocator, size_t _initial_chunk_count, size_t _chunk_size, size_t _miscellaneous = 0) {
			chunk_size = _chunk_size;
			miscellaneous = _miscellaneous;
			buffers.Initialize(_allocator, _initial_chunk_count);
			buffers.size = _initial_chunk_count;
			chunks_with_space = { AllocateEx(_allocator, sizeof(unsigned int) * _initial_chunk_count), 0 };
			for (size_t index = 0; index < _initial_chunk_count; index++) {
				void* chunk = AllocateEx(_allocator, sizeof(T) * chunk_size);
				buffers[index] = { chunk, 0, (unsigned int)chunk_size };
				chunks_with_space.Add((unsigned int)index);
			}
		}

		// Temp buffer needs to have at least 64 bytes long
		// You should only read from this deck, not peform any mutable operations
		static Deck<T, RangeSelector> InitializeTempReference(Stream<T> data, void* temp_buffer) {
			Deck<T, RangeSelector> deck;

			if (data.size > 0) {
				ulong2 next_capacity = RangeSelector::GetNextCapacity(data.size);
				deck.buffers.buffer = (CapacityStream<T>*)temp_buffer;
				deck.buffers.capacity = 1;
				deck.buffers.size = 1;
				deck.buffers.allocator = { nullptr };
				temp_buffer = OffsetPointer(temp_buffer, sizeof(deck.buffers.buffer[0]));

				deck.buffers[0] = { data.buffer, (unsigned int)data.size, (unsigned int)data.size };
				deck.chunk_size = next_capacity.x;
				deck.miscellaneous = next_capacity.y;

				deck.chunks_with_space.InitializeFromBuffer(temp_buffer, 1);
				deck.chunks_with_space[0] = 0;
			}

			return deck;
		}

		ResizableStream<CapacityStream<T>> buffers;
		Stream<unsigned int> chunks_with_space;
		size_t chunk_size;
		size_t miscellaneous;
	};

	// The GetNextCapacity() is used to initialize a temp reference to an existing Stream<T>
	// It needs to return a ulong2 that represents { chunk_size, miscellaneous }

	struct DeckRangeModulo {
		ECS_INLINE static size_t Chunk(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index / chunk_size;
		}

		ECS_INLINE static size_t Buffer(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index % chunk_size;
		}

		ECS_INLINE static ulong2 GetNextCapacity(size_t capacity) {
			return { capacity, 0 };
		}
	};

	struct DeckRangePowerOfTwo {
		ECS_INLINE static size_t Chunk(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index >> miscellaneous;
		}

		ECS_INLINE static size_t Buffer(size_t index, size_t chunk_size, size_t miscellaneous) {
			return index & (chunk_size - 1);
		}

		ECS_INLINE static ulong2 GetNextCapacity(size_t capacity) {
			return PowerOfTwoGreaterEx(capacity);
		}
	};

	template<typename T>
	using DeckModulo = Deck<T, DeckRangeModulo>;

	template<typename T>
	using DeckPowerOfTwo = Deck<T, DeckRangePowerOfTwo>;

}