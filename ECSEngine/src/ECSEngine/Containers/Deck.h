#pragma once
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/Utilities.h"
#include "../Utilities/Iterator.h"

namespace ECSEngine {

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

	// Miscellaneous is used as the exponent for the power of two range selector
	template<typename T, typename RangeSelector>
	struct Deck {
		typedef T T;

		ECS_INLINE Deck() : buffers(nullptr, 0), chunk_size(0), miscellaneous(0) {}

		// Disregard the template argument, it is used only to avoid a compilation error to distinguish between this overload
		// And the overload for the power of two
		template<typename Dummy = RangeSelector, std::enable_if_t<std::is_same_v<Dummy, DeckRangePowerOfTwo>, int> = 0>
		ECS_INLINE Deck(
			const void* _buffers, 
			size_t _buffers_capacity,
			size_t _chunk_exponent 
		) : buffers(_buffers, 0, _buffers_capacity), chunk_size(1 << _chunk_exponent), power_of_two_exponent(_chunk_exponent) {}

		// Disregard the template argument, it is used only to avoid a compilation error to distinguish between this overload
		// And the overload for the power of two
		template<typename Dummy = RangeSelector, std::enable_if_t<!std::is_same_v<Dummy, DeckRangePowerOfTwo>, int> = 0>
		ECS_INLINE Deck(
			const void* _buffers,
			size_t _buffers_capacity,
			size_t _chunk_size
		) : buffers(_buffers, 0, _buffers_capacity), chunk_size(_chunk_size), miscellaneous(0) {}

		Deck(const Deck& other) = default;
		Deck& operator = (const Deck& other) = default;

	private:
		template<typename ElementType>
		size_t AddImpl(ElementType element_type) {
			if (size == GetAllocatedElementCount()) {
				Reserve();
			}
			
			size_t chunk_to_add = RangeSelector::Chunk(size, chunk_size, miscellaneous);
			buffers[chunk_to_add].Add(element_type);
			size++;
			return size - 1;
		}

	public:

		ECS_INLINE size_t Add(T element) {
			return AddImpl(element);
		}

		ECS_INLINE size_t Add(const T* element) {
			return AddImpl(element);
		}

		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return buffers.allocator;
		}

		size_t AddStream(Stream<T> other) {
			Reserve(other.size);
			size_t initial_size = size;

			// Use an efficient memcpy version, instead of the element
			// By element add
			size_t starting_chunk = RangeSelector::Chunk(size, chunk_size, miscellaneous);
			size += other.size;
			size_t chunk_index = starting_chunk;
			while (other.size > 0) {
				size_t current_chunk_space = chunk_size - buffers[chunk_index].size;
				Stream<T> current_addition = other;
				current_addition.size = ClampMax(current_addition.size, current_chunk_space);
				buffers[chunk_index].AddStream(current_addition);
				other.Advance(current_addition.size);
				chunk_index++;
			}

			return initial_size;
		}

		size_t AddDeck(const Deck<T, RangeSelector>& deck) {
			size_t deck_size = deck.GetElementCount();
			size_t initial_size = size;
			
			Reserve(deck_size);
			// We can use the chunk by chunk addition
			size_t deck_chunks = SlotsFor(deck.size, deck.chunk_size);
			for (size_t index = 0; index < deck_chunks; index++) {
				AddStream(deck.buffers[index]);
			}

			return initial_size;
		}

		// Allocates memory for extra count chunks
		void AllocateChunks(size_t count) {
			unsigned int current_capacity = buffers.capacity;
			unsigned int new_capacity = 0;
			if (buffers.size + count > buffers.capacity) {
				new_capacity = (unsigned int)((float)buffers.capacity * ECS_RESIZABLE_STREAM_FACTOR + 2.0f);
				buffers.Resize(new_capacity);	
			}
			else {
				new_capacity = buffers.size + count;
			}

			for (unsigned int index = 0; index < count; index++) {
				buffers[buffers.size + index].Initialize(buffers.allocator, 0, chunk_size);
			}
			buffers.size += count;
		}

		// Returns true if the elements can be added without having to allocate new chunks
		bool CanAddNoResize(size_t count) const {
			size_t allocated_elements = GetAllocatedElementCount();
			return allocated_elements - size >= count;
		}

		Deck<T, RangeSelector> Copy(AllocatorPolymorphic allocator) const {
			Deck<T, RangeSelector> copy;
			if constexpr (std::is_same_v<RangeSelector, DeckRangePowerOfTwo>) {
				copy.Initialize(allocator, 0, power_of_two_exponent);
			}
			else {
				copy.Initialize(allocator, 0, chunk_size);
			}
			copy.miscellaneous = miscellaneous;
			copy.AddDeck(*this);
			return copy;
		}

		void CopyOther(const void* memory, size_t count) {
			ResizeNoCopy(count);
			AddStream(Stream<T>(memory, count));
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
			// If the allocator is unspecified (like in the case of temp reference),
			// Don't do anything, except for resetting the size
			if (buffers.allocator.allocator == nullptr) {
				buffers.Clear();
				size = 0;
				return;
			}

			for (unsigned int index = 0; index < buffers.size; index++) {
				ECSEngine::Deallocate(buffers.allocator, buffers[index].buffer);
			}
			buffers.FreeBuffer();
			size = 0;
		}

		// The functor receives a ulong2 which has in the x component
		// the chunk index and in the y the stream index for that chunk
		// Return true to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachIndex(Functor&& functor) const {
			for (size_t index = 0; index < buffers.size; index++) {
				for (unsigned int subindex = 0; subindex < buffers[index].size; subindex++) {
					ulong2 indices = { index, subindex };
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

		// The functor receives a ulong2 which has in the x component
		// the chunk index and in the y the stream index for that chunk
		// Return true when you want to delete an item, else false
		// Don't perform the erase by yourself, this function will do it
		// Returns true if it early exited, else false
		// If the many_erases is set to true, it will use a more efficient erasing
		// Strategy, that will not involve swaps. The default one is with swaps,
		// And if many elements will be removed, it can result in many unneeded swaps
		template<bool early_exit = false, bool many_erases = false, typename Functor>
		bool ForEachIndexWithErase(Functor&& functor) {
			static_assert((early_exit && !many_erases) || !early_exit, "Invalid ForEachIndexWithErase options - can't choose many erases with early exit");

			bool* valid_mask = nullptr;
			bool any_valid_mask = false;
			size_t valid_mask_allocation_size = size * sizeof(bool);
			if constexpr (many_erases) {
				valid_mask = (bool*)ECS_MALLOCA_ALLOCATOR_DEFAULT(valid_mask_allocation_size, buffers.allocator);
			}

			for (size_t index = 0; index < buffers.size; index++) {
				for (unsigned int subindex = 0; subindex < buffers[index].size; subindex++) {
					ulong2 indices = { index, subindex };
					if constexpr (early_exit) {
						if (functor(indices)) {
							// Don't need to take care of MALLOCA, since it can't compile with both of them on
							return true;
						}
					}
					else {
						if (functor(indices)) {
							if constexpr (!many_erases) {
								RemoveSwapBack(index * chunk_size + subindex);
								subindex--;
							}
							else {
								valid_mask[index * chunk_size + subindex] = false;
							}
						}
						else {
							if constexpr (many_erases) {
								valid_mask[index * chunk_size + subindex] = true;
								any_valid_mask = true;
							}
						}
					}
				}
			}

			if constexpr (many_erases) {
				if (any_valid_mask) {
					// Iterate through the boolean mask and add the elements that are marked
					size_t write_index = 0;
					for (size_t index = 0; index < size; index++) {
						if (valid_mask[index]) {
							operator[](write_index) = operator[](index);
							write_index++;
						}
					}
					size = write_index;
					for (size_t index = 0; index < buffers.size; index++) {
						buffers[index].size = 0;
					}
					size_t chunks_with_elements = SlotsFor(size, chunk_size);
					for (size_t index = 0; index < chunks_with_elements; index++) {
						buffers[index].size = ClampMax(write_index, chunk_size);
						write_index -= buffers[index].size;
					}
				}
				else {
					// Can reset
					Reset();
				}
				ECS_FREEA_ALLOCATOR_DEFAULT(valid_mask, valid_mask_allocation_size, buffers.allocator);
			}

			return false;
		}

		// Return true to early exit, else false (for the early exit mode)
		// Return true when you delete an item, else false (when normal iteration -
		// in this mode you can erase elements while you are iterating - but don't
		// remove the elements by yourself, this function will do it!)
		// Returns true if it early exited, else false
		// If the many_erases is set to true, it will use a more efficient erasing
		// Strategy, that will not involve swaps. The default one is with swaps,
		// And if many elements will be removed, it can result in many unneeded swaps
		template<bool early_exit = false, bool many_erases = false, typename Functor>
		bool ForEachWithErase(Functor&& functor) {
			return ForEachIndexWithErase<early_exit, many_erases>([&](ulong2 indices) {
				return functor(buffers[indices.x][indices.y]);
			});
		}

		// Return true to early exit, else false
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
						functor(*element);
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

		// Return true to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachChunk(Functor&& functor) const {
			for (size_t index = 0; index < buffers.size; index++) {
				if constexpr (early_exit) {
					if (functor(buffers[index].ToStream())) {
						return true;
					}
				}
				else {
					functor(buffers[index].ToStream());
				}
			}
			return false;
		}

		// Use this method in case you have the indices valid or from iterating and storing the values
		ECS_INLINE T GetValue(size_t chunk_index, size_t in_chunk_index) const {
			return buffers[chunk_index][in_chunk_index];
		}

		// Use this method in case you have the indices valid or from iterating and storing the values
		// In the x the chunk index must be stored, while in the y the in chunk index.
		ECS_INLINE T GetValue(ulong2 indices) const {
			return GetValue(indices.x, indices.y);
		}

		// Use this method in case you have the indices valid or from iterating and storing the values
		ECS_INLINE T* GetValuePtr(size_t chunk_index, size_t in_chunk_index) {
			return &buffers[chunk_index][in_chunk_index];
		}

		// Use this method in case you have the indices valid or from iterating and storing the values
		// In the x the chunk index must be stored, while in the y the in chunk index.
		ECS_INLINE T* GetValuePtr(ulong2 indices) {
			return GetValuePtr(indices.x, indices.y);
		}

		// Use this method in case you have the indices valid or from iterating and storing the values
		ECS_INLINE const T* GetValuePtr(size_t chunk_index, size_t in_chunk_index) const {
			return &buffers[chunk_index][in_chunk_index];
		}

		// Use this method in case you have the indices valid or from iterating and storing the values
		// In the x the chunk index must be stored, while in the y the in chunk index.
		ECS_INLINE const T* GetValuePtr(ulong2 indices) const {
			return GetValuePtr(indices.x, indices.y);
		}

		ECS_INLINE size_t GetChunkCount() const {
			return buffers.size;
		}

		ECS_INLINE size_t GetElementCount() const {
			return size;
		}

		// Returns the count of elements that the current chunks support
		ECS_INLINE size_t GetAllocatedElementCount() const {
			return buffers.size * chunk_size;
		}

		// If the boolean is set, it will deallocate the buffers as well
		void Reset(bool deallocate_existing_chunks = false) {
			if (deallocate_existing_chunks) {
				Deallocate();
			}
			else {
				size = 0;
				for (size_t index = 0; index < buffers.size; index++) {
					buffers[index].size = 0;
				}
			}
		}

		void RemoveSwapBack(size_t index) {
			// Remove swap back the last element in the whole deck with the current one
			// Don't use remove swap back on the chunk since it will leave empty holes
			size_t chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			size_t in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			
			size_t last_element_chunk_index = RangeSelector::Chunk(size - 1, chunk_size, miscellaneous);
			size_t last_element_in_chunk_index = RangeSelector::Buffer(size - 1, chunk_size, miscellaneous);

			buffers[chunk_index][in_chunk_index] = buffers[last_element_chunk_index][last_element_in_chunk_index];
			buffers[last_element_chunk_index].size--;
			size--;
		}

		// Makes sure that the given count of elements will be added without
		// Any new allocations. It will not update the size!!!
		void Reserve(size_t count = 1) {
			size_t allocated_count = GetAllocatedElementCount();
			size_t available_element_count = allocated_count - size;

			if (count <= available_element_count) {
				return;
			}

			// Calculate the chunk count necessary to be allocated
			size_t difference = count - available_element_count;
			size_t new_chunks = SlotsFor(difference, chunk_size);
			AllocateChunks(new_chunks);
		}

		// Makes sure that the given count of elements will be added without
		// Any new allocations. It will update the size
		size_t ReserveAndUpdateSize(size_t count = 1) {
			Reserve(count);
			size_t initial_size = size;
			size += count;

			// We must update the size of the buffers
			// For that individual buffer as well
			size_t initial_chunk = RangeSelector::Chunk(initial_size, chunk_size, miscellaneous);
			for (size_t index = initial_chunk; index < buffers.size; index++) {
				size_t add_count = chunk_size - buffers[index].size;
				// Clamp the add count to the count
				add_count = add_count < count ? add_count : count;
				buffers[index].size += add_count;
				count -= add_count;
			}

			return initial_size;
		}

		// It does not change the size, unless elements are trimmed
		void Resize(size_t new_element_count) {
			if (new_element_count < size) {
				// Set the size of all chunks to 0, and then
				// Set the size for those chunks that have elements
				for (size_t index = 0; index < buffers.size; index++) {
					buffers[index].size = 0;
				}
				size_t max_chunk_count = SlotsFor(new_element_count, chunk_size);
				for (size_t index = 0; index < max_chunk_count; index++) {
					// Clamp the size to the chunk size
					buffers[index].size = new_element_count < chunk_size ? new_element_count : chunk_size;
					new_element_count -= buffers[index].size;
				}
				size = new_element_count;
			}
			else {
				Reserve(new_element_count - size);
			}
		}

		// It does not change the size, unless elements are trimmed
		void ResizeNoCopy(size_t element_count) {
			size_t initial_size = size;
			Deallocate();
			if constexpr (std::is_same_v<RangeSelector, DeckRangePowerOfTwo>) {
				Initialize(Allocator(), SlotsFor(element_count, chunk_size), power_of_two_exponent);
			}
			else {
				Initialize(Allocator(), SlotsFor(element_count, chunk_size), chunk_size);
			}
			size = element_count < initial_size ? element_count : initial_size;
		}

		ECS_INLINE void Swap(size_t first, size_t second) {
			swap(operator[](first), operator[](second));
		}

		// It will remove any unnecessary chunks taking into account any extra elements
		void Trim(size_t additional_elements = 0) {
			size_t required_chunks = SlotsFor(size + additional_elements, chunk_size);
			if (required_chunks < buffers.size) {
				for (size_t index = required_chunks; index < buffers.size; index++) {
					buffers[index].Deallocate(Allocator());
				}
				buffers.size = required_chunks;
			}
		}

		ECS_INLINE T& operator [](size_t index) {
			size_t chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			size_t in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			return buffers[chunk_index][in_chunk_index];
		}

		ECS_INLINE const T& operator [](size_t index) const {
			size_t chunk_index = RangeSelector::Chunk(index, chunk_size, miscellaneous);
			size_t in_chunk_index = RangeSelector::Buffer(index, chunk_size, miscellaneous);
			return buffers[chunk_index][in_chunk_index];
		}

		// Disregard the template argument, it is used only to avoid a compilation error to distinguish between this overload
		// And the overloads
		template<typename Dummy = RangeSelector, std::enable_if_t<std::is_same_v<Dummy, DeckRangePowerOfTwo>, int> = 0>
		void Initialize(AllocatorPolymorphic _allocator, size_t _initial_chunk_count, size_t _power_of_two_exponent) {
			chunk_size = 1 << _power_of_two_exponent;
			power_of_two_exponent = _power_of_two_exponent;
			size = 0;
			buffers.Initialize(_allocator, _initial_chunk_count);
			buffers.size = _initial_chunk_count;
			for (size_t index = 0; index < _initial_chunk_count; index++) {
				buffers[index].Initialize(_allocator, 0, chunk_size);
			}
		}

		// Disregard the template argument, it is used only to avoid a compilation error to distinguish between this overload
		// And the overload for the power of two
		template<typename Dummy = RangeSelector, std::enable_if_t<!std::is_same_v<Dummy, DeckRangePowerOfTwo>, int> = 0>
		void Initialize(AllocatorPolymorphic _allocator, size_t _initial_chunk_count, size_t _chunk_size) {
			chunk_size = _chunk_size;
			power_of_two_exponent = 0;
			size = 0;
			buffers.Initialize(_allocator, _initial_chunk_count);
			buffers.size = _initial_chunk_count;
			for (size_t index = 0; index < _initial_chunk_count; index++) {
				buffers[index].Initialize(_allocator, 0, chunk_size);
			}
		}

		// Temp buffer needs to have at least 64 bytes long
		// You should only read from this deck, not peform any mutable operations
		static Deck<T, RangeSelector> InitializeTempReference(Stream<T> data, CapacityStream<void>& temp_buffer) {
			Deck<T, RangeSelector> deck;

			if (data.size > 0) {
				ulong2 next_capacity = RangeSelector::GetNextCapacity(data.size);
				deck.buffers.buffer = temp_buffer.Reserve<CapacityStream<T>>();
				deck.buffers.capacity = 1;
				deck.buffers.size = 1;
				// Set the allocator to nullptr, to indicate that no longer has been used
				deck.buffers.allocator = nullptr;

				deck.buffers[0] = { data.buffer, (unsigned int)data.size, (unsigned int)data.size };
				deck.chunk_size = next_capacity.x;
				deck.miscellaneous = next_capacity.y;
				deck.size = data.size;
			}

			return deck;
		}

		template<typename DeckType, typename ValueType>
		struct IteratorTemplate : IteratorInterface<ValueType> {
			IteratorTemplate(DeckType* _deck, size_t _chunk_index, size_t _stream_index) : deck(_deck), chunk_index(_chunk_index), stream_index(_stream_index),
				IteratorInterface<ValueType>(0) {
				remaining_count = deck->size - deck->chunk_size * chunk_index - stream_index;
			}

			ECS_CLASS_MEMCPY_ASSIGNMENT_OPERATORS(IteratorTemplate);

		protected:
			ValueType* GetImpl() override {
				size_t overall_index = chunk_index * deck->chunk_size + stream_index;
				if (overall_index >= deck->GetElementCount()) {
					return nullptr;
				}
				ValueType* value = deck->GetValuePtr(chunk_index, stream_index);
				stream_index++;
				if (stream_index == deck->chunk_size) {
					stream_index = 0;
					chunk_index++;
				}
				return value;
			}

			IteratorInterface<ValueType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
				IteratorTemplate<DeckType, ValueType>* iterator = AllocateAndConstruct<IteratorTemplate<DeckType, ValueType>>(allocator, deck, chunk_index, stream_index);
				// Set the remaining count to the given subrange.
				iterator->remaining_count = count;

				size_t chunk_forward_count = count / deck->chunk_size;
				size_t chunk_remainder_count = count % deck->chunk_size;
				chunk_index += chunk_forward_count;
				stream_index += chunk_remainder_count;
				if (stream_index >= deck->chunk_size) {
					chunk_index++;
					stream_index -= deck->chunk_size;
				}
				return iterator;
			}

		public:

			bool IsContiguous() const override {
				return false;
			}

			IteratorInterface<ValueType>* Clone(AllocatorPolymorphic allocator) override { return CloneHelper<decltype(*this)>(allocator); }

			DeckType* deck;
			size_t chunk_index;
			// The index inside the deck's chunk
			size_t stream_index;
		};

		typedef IteratorTemplate<Deck<T, RangeSelector>, T> IteratorType;
		typedef IteratorTemplate<const Deck<T, RangeSelector>, const T> ConstIteratorType;

		ECS_INLINE ConstIteratorType ConstIterator(size_t starting_index = 0) const {
			size_t chunk_index = starting_index / chunk_size;
			size_t stream_index = starting_index % chunk_size;
			return { this, chunk_index, stream_index };
		}

		ECS_INLINE IteratorType MutableIterator(size_t starting_index = 0) {
			size_t chunk_index = starting_index / chunk_size;
			size_t stream_index = starting_index % chunk_size;
			return { this, chunk_index, stream_index };
		}

		ResizableStream<CapacityStream<T>> buffers;
		// This is the current element count
		size_t size;
		size_t chunk_size;
		union {
			// Used only for the power of two variant
			size_t power_of_two_exponent;
			// This name is here just to indicate that it can have a different meaning for other
			// Range selectors
			size_t miscellaneous;
		};
	};

	template<typename T>
	using DeckModulo = Deck<T, DeckRangeModulo>;

	template<typename T>
	using DeckPowerOfTwo = Deck<T, DeckRangePowerOfTwo>;

}