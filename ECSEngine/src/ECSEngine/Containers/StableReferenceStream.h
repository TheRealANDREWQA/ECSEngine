#pragma once
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	// This is basically a pooled list
	// It keeps an array of free indices
	// Set boolean_flags to true if you want to iterate through the stream
	// It will keep a compacted frequency bit mask - every element gets a bit
	// To reduce the memory footprint and the memory bandwidth
	template<typename T, bool queue_indirection_list = false>
	struct StableReferenceStream {
		StableReferenceStream() {}
		StableReferenceStream(void* buffer, unsigned int _capacity) {
			InitializeFromBuffer(buffer, _capacity);
		}

		StableReferenceStream(const StableReferenceStream& other) = default;
		StableReferenceStream<T, queue_indirection_list>& operator = (const StableReferenceStream<T, queue_indirection_list>& other) = default;

		// Forcefully allocates an index. If the index is occupied, it will assert
		// It increases the size as well
		void AllocateIndex(unsigned int index) {
			unsigned int indirection_index = 0;
			if constexpr (queue_indirection_list) {
				// A - used, B - free
				if (indirection_list_start_index == 0) {
					// AAAA BBBBBB
					indirection_index = function::SearchBytes(indirection_list + size, capacity - size, index, sizeof(index));
					indirection_index = indirection_index == -1 ? -1 : indirection_index + size;
				}
				else {
					if (indirection_list_start_index + size < capacity) {
						// BBB AAAA BBBB
						// 2 searches, one before and one after
						indirection_index = function::SearchBytes(indirection_list, indirection_list_start_index, index, sizeof(index));
						if (indirection_index == -1) {
							unsigned int offset = indirection_list_start_index + size;
							indirection_index = function::SearchBytes(indirection_list + offset, capacity - offset, index, sizeof(index));
							indirection_index = indirection_index == -1 ? -1 : indirection_index + offset;
						}
					}
					else {
						if (indirection_list_start_index + size == capacity) {
							// BBBBBB AAAAA
							// One search
							indirection_index = function::SearchBytes(indirection_list, capacity - size, index, sizeof(index));
						}
						else {
							// AA BBBBBB AAA
							// 1 search in the middle of the wrap around
							unsigned int offset = indirection_list_start_index + size - capacity;
							indirection_index = function::SearchBytes(indirection_list + offset, capacity - size, index, sizeof(index));
							indirection_index = indirection_index == -1 ? -1 : indirection_index + offset;
						}
					}
				}
			}
			else {
				indirection_index = function::SearchBytes(indirection_list + size, capacity - size, index, sizeof(index));
				indirection_index = indirection_index == -1 ? -1 : indirection_index + size;
			}

			// If -1 then its already occupied
			ECS_ASSERT(indirection_index != -1);

			// Now move the index such that it gets used
			if constexpr (queue_indirection_list) {
				unsigned int offset = indirection_list_start_index + size;
				offset = offset >= capacity ? offset - capacity : offset;
				std::swap(indirection_list[offset], indirection_list[indirection_index]);
			}
			else {
				std::swap(indirection_list[size], indirection_list[indirection_index]);
			}

			size++;
		}

		// Returns an index that can be used for the lifetime of the object to access it
		ECS_INLINE unsigned int Add(T element) {
			unsigned int index = ReserveOne();
			buffer[index] = element;
			return index;
		}

		// Returns an index that can be used for the lifetime of the object to access it
		ECS_INLINE unsigned int Add(const T* element) {
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

		bool ExistsItem(unsigned int index) const {
			if (index >= capacity) {
				return false;
			}

			if (size < capacity / 2) {
				// Iterate the occupied indices since they are fewer
				for (unsigned int occupied_index = 0; occupied_index < size; occupied_index++) {
					unsigned int current_index = 0;
					if constexpr (!queue_indirection_list) {
						current_index = indirection_list[occupied_index];
					}
					else {
						unsigned int queue_occupied_index = (indirection_list_start_index + occupied_index) == capacity ? 0 : indirection_list_start_index + occupied_index;
						current_index = indirection_list[queue_occupied_index];
					}

					if (current_index == index) {
						return true;
					}
				}
				return false;
			}
			else {
				// Iterate the freed indices since they are fewer
				unsigned int freed_start = indirection_list_start_index + size;
				freed_start = freed_start >= capacity ? freed_start - capacity : freed_start;

				for (unsigned int free_index = 0; free_index < capacity - size; free_index++) {
					unsigned int current_index = 0;
					if constexpr (!queue_indirection_list) {
						current_index = indirection_list[size + free_index];
					}
					else {
						current_index = indirection_list[freed_start++];
						freed_start = freed_start == capacity ? 0 : freed_start;
					}

					if (current_index == index) {
						return false;
					}
				}
				return true;
			}
		}

		// Return true in the functor to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachIndex(Functor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				unsigned int current_index = 0;
				if constexpr (!queue_indirection_list) {
					current_index = indirection_list[index];
				}
				else {
					unsigned int occupied_list_index = (indirection_list_start_index + index) == capacity ? 0 : indirection_list_start_index + index;
					current_index = indirection_list[occupied_list_index];
				}
				if constexpr (early_exit) {
					if (functor(current_index)) {
						return true;
					}
				}
				else {
					functor(current_index);
				}
			}
			return false;
		}

		// Return true to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			return ForEachIndex<early_exit>([&](unsigned int index) {
				if constexpr (early_exit) {
					return functor(buffer[index]);
				}
				else {
					functor(buffer[index]);
				}
			});
		}

		// Return true to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachConst(Functor&& functor) const {
			return ForEachIndex<early_exit>([&](unsigned int index) {
				if constexpr (early_exit) {
					return functor(buffer[index]);
				}
				else {
					functor(buffer[index]);
				}
			});
		}

		unsigned int ReserveOne() {
			ECS_ASSERT(size < capacity);
			unsigned int index;

			if constexpr (!queue_indirection_list) {
				// Search the value in the unused part
				index = indirection_list[size];
			}
			else {
				unsigned int list_index = indirection_list_start_index + size;
				list_index = list_index >= capacity ? list_index - capacity : list_index;
				index = indirection_list[list_index];
			}

			size++;
			return index;
		}

		// It will fill in the handles buffer with the new handles
		ECS_INLINE void Reserve(Stream<unsigned int> indices) {
			ECS_ASSERT(size + indices.size < capacity);
			for (size_t index = 0; index < indices.size; index++) {
				indices[index] = ReserveOne();
			}
		}

		// it will reset the free list
		ECS_INLINE void Reset() {
			size = 0;
			indirection_list_start_index = 0;
			for (unsigned int index = 0; index < capacity; index++) {
				indirection_list[index] = index;
			}
		}

		// Add the index to the free list
		void Remove(unsigned int index) {
			if constexpr (!queue_indirection_list) {
				// Search the value in the occupied portion
				// Swap to the end and then decrease the size
				size_t free_list_index = function::SearchBytes(indirection_list, size, index, sizeof(index));
				ECS_ASSERT(free_list_index != -1);
				size--;
				unsigned int replacement = indirection_list[size];
				indirection_list[size] = index;
				indirection_list[free_list_index] = replacement;
			}
			else {
				size_t free_list_index = 0;

				if (indirection_list_start_index + size < capacity) {
					// No wrapping
					free_list_index = function::SearchBytes(indirection_list + indirection_list_start_index, size, index, sizeof(index));
					ECS_ASSERT(free_list_index != -1);
					free_list_index += indirection_list_start_index;
				}
				else {
					// Two checks
					size_t first_chunk_count = capacity - indirection_list_start_index;
					free_list_index = function::SearchBytes(indirection_list + indirection_list_start_index, first_chunk_count, index, sizeof(index));
					if (free_list_index == -1) {
						free_list_index = function::SearchBytes(indirection_list, size - first_chunk_count, index, sizeof(index));
						ECS_ASSERT(free_list_index != -1);
					}
					else {
						free_list_index += indirection_list_start_index;
					}
				}

				// Put the item at the start_index
				indirection_list[free_list_index] = indirection_list[indirection_list_start_index];
				size--;
				indirection_list[indirection_list_start_index] = index;
				indirection_list_start_index++;
				indirection_list_start_index = indirection_list_start_index == capacity ? 0 : indirection_list_start_index;
			}
		}

		ECS_INLINE void GetItemIndices(CapacityStream<unsigned int>& indices) const {
			ForEachIndex([&](unsigned int index) {
				indices.Add(index);
			});
			indices.AssertCapacity();
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
			return (sizeof(T) + sizeof(unsigned int)) * count;
		}

		void InitializeFromBuffer(void* _buffer, unsigned int _capacity) {
			buffer = (T*)_buffer;
			capacity = _capacity;
			uintptr_t ptr = (uintptr_t)_buffer;
			ptr += sizeof(T) * capacity;
			indirection_list = (unsigned int*)ptr;
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

		// Considers this stream to be already allocated. If the other stream is smaller than the current capacity
		// then it will use efficient memcpy's to move the data. If the other stream is bigger in capacity, then it
		// assumes that there are no indices allocated from the high part of the other stream that would be lost in
		// this stream (it iterates through the used and unused indices and asserts if it finds one used greater than
		// the current capacity).
		void Copy(StableReferenceStream<T, queue_indirection_list> other) {
			ECS_ASSERT(other.size <= capacity);

			if (other.capacity <= capacity) {
				memcpy(buffer, other.buffer, sizeof(T) * other.capacity);
				if (other.capacity == capacity) {
					memcpy(indirection_list, other.indirection_list, sizeof(unsigned int) * other.capacity);
					indirection_list_start_index = other.indirection_list_start_index;
				}
				else {
					// Copy the used indices and then append the unused ones
					if constexpr (queue_indirection_list) {
						if (other.indirection_list_start_index + other.size > other.capacity) {
							// There is wrap around
							unsigned int first_write_size = other.capacity - other.indirection_list_start_index;
							memcpy(indirection_list, other.indirection_list + other.indirection_list_start_index, first_write_size * sizeof(unsigned int));
							unsigned int second_write_size = other.size - first_write_size;
							memcpy(indirection_list + first_write_size, other.indirection_list, second_write_size * sizeof(unsigned int));

							// Copy all the unused ones
							if (other.size != other.capacity) {
								memcpy(indirection_list + other.size, other.indirection_list + second_write_size, (other.indirection_list_start_index - second_write_size) * sizeof(unsigned int));
							}
						}
						else {
							// Can memcpy the data in a single slice
							memcpy(indirection_list, other.indirection_list + other.indirection_list_start_index, other.size * sizeof(unsigned int));
							if (other.size != other.capacity) {
								// Need 2 writes or 1
								if (other.indirection_list_start_index == 0) {
									// 1 write
									memcpy(indirection_list + other.size, other.indirection_list + other.size, (other.size - other.capacity) * sizeof(unsigned int));
								}
								else if (other.indirection_list_start_index + other.size == other.capacity) {
									// 1 write
									memcpy(indirection_list + other.size, other.indirection_list, (other.size - other.capacity) * sizeof(unsigned int));
								}
								else {
									// 2 writes
									unsigned int first_write_size = other.indirection_list_start_index;
									memcpy(indirection_list + other.size, other.indirection_list, first_write_size * sizeof(unsigned int));
									unsigned int second_write_size = other.capacity - other.size - first_write_size;
									memcpy(indirection_list + other.size + first_write_size, other.indirection_list + other.indirection_list_start_index + other.size, second_write_size * sizeof(unsigned int));
								}
							}
						}
					}
					else {
						// Can memcpy directly the buffer
						memcpy(indirection_list, other.indirection_list, sizeof(unsigned int) * other.capacity);
					}

					// Now fill in the rest of the indices
					for (unsigned int index = other.capacity; index < capacity; index++) {
						indirection_list[index] = index;
					}
					indirection_list_start_index = 0;
				}
			}
			else {
				if constexpr (queue_indirection_list) {
					unsigned int current_index = other.indirection_list_start_index;
					for (unsigned int index = 0; index < other.size; index++) {
						unsigned int indirection = other.indirection_list[current_index];
						ECS_ASSERT(indirection < capacity);
						indirection_list[index] = indirection;
						buffer[indirection] = other.buffer[indirection];
						current_index++;
						current_index = current_index == other.capacity ? 0 : current_index;
					}

					unsigned int indirection_copy_count = other.size;

					// Now copy the rest of the indices (the unused ones that fit)

					// A - unused, B - used
					// 4 cases:
					// AAA BBB AAA
					// BBB AAAAAAA
					// BBA AAAABBB
					// AAAAAAA BBB
					unsigned int last_index = other.indirection_list_start_index + other.size;

					auto loop = [&](unsigned int start, unsigned int end) {
						for (unsigned int index = start; index < end && indirection_copy_count < capacity; index++) {
							unsigned int indirection = other.indirection_list[index];
							if (indirection < capacity) {
								indirection_list[indirection_copy_count++] = indirection;
							}
						}
					};

					if (last_index < other.capacity) {
						// No wrap around, either the 1st, 2nd case or the 4th
						loop(0, other.indirection_list_start_index);
						loop(last_index, other.capacity);
					}
					else {
						// Wrap around, 3rd case
						last_index -= capacity;
						loop(last_index, other.indirection_list_start_index);
					}
 
				}
				else {
					for (unsigned int index = 0; index < other.size; index++) {
						unsigned int indirection = other.indirection_list[index];
						ECS_ASSERT(indirection < capacity);
						buffer[indirection] = other.buffer[indirection];
						indirection_list[index] = indirection;
					}

					unsigned int write_index = other.size;
					for (unsigned int index = other.size; write_index < capacity && index < other.capacity; index++) {
						unsigned int indirection = other.indirection_list[index];
						if (indirection < capacity) {
							indirection_list[write_index++] = indirection;
						}
					}
				}

				indirection_list_start_index = 0;
			}

			size = other.size;
		}

		T* buffer;
		unsigned int* indirection_list;
		unsigned int indirection_list_start_index;
		unsigned int size;
		unsigned int capacity;
	};


}