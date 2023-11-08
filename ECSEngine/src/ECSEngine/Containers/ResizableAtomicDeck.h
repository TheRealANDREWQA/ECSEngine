#pragma once
#include "AtomicStream.h"
#include "Stream.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	// The maximum amount of chunks needs to be fixed in order to not complicate matters a lot
	template<typename T>
	struct ResizableAtomicDeck {
		ECS_INLINE ResizableAtomicDeck() {}
		ECS_INLINE ResizableAtomicDeck(AllocatorPolymorphic _allocator, unsigned int _chunk_size, unsigned int max_chunk_count, unsigned int initial_chunks = 1) {
			Initialize(_allocator, _chunk_size, max_chunk_count, initial_chunks);
		}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ResizableAtomicDeck<T>);

		// Returns the index at which it wrote
		// If there is not enough space, it will return with -1 and signal the boolean
		unsigned int Add(T element, bool& success) {
			unsigned int write_index = Request(success);
			if (success) {
				stream[write_index] = element;
				FinishRequest(write_index);
			}
			return write_index;
		}

		// Returns the index at which it wrote
		// It will assert if there is not enough space
		unsigned int Add(T element) {
			bool success = false;
			unsigned int position = Add(element, success);
			ECS_ASSERT(success);
			return position;
		}

	private:
		void WriteElementsIntoChunk(Stream<T>& elements, unsigned int chunk_index, unsigned int write_position) {
			if (write_position < chunk_size) {
				// Write as many elements as we can into this chunk
				unsigned int write_element_count = chunk_size - write_position;
				write_element_count = std::min(write_element_count, (unsigned int)elements.size);
				memcpy(stream[chunk_index].buffer + write_position, elements.buffer, write_element_count * sizeof(T));
				// Finish with the entire count
				stream[chunk_index].FinishRequest(elements.size);
				elements.Advance(write_element_count);
			}
		}

	public:
		// This does not guarantee that elements will be one after the other
		// The elements in the stream will keep their relative positioning
		// But other additions can appear in between elements
		void AddStreamLoose(Stream<T> elements) {
			unsigned int chunk_index = stream.size - 1;
			unsigned int write_position = stream[chunk_index].RequestInt(elements.size);

			// We will break out of this loop
			while (true) {
				WriteElementsIntoChunk(elements, chunk_index, write_position);

				if (elements.size > 0) {
					// Try to get the lock and resize
					bool locked_by_us = lock.TryLock();
					if (locked_by_us) {
						// We need to check to see if someone else expanded the chunks before use
						// And write into the last chunk if so
						chunk_index++;
						if (chunk_index < stream.size - 1) {
							// Write into that newly created chunk
							chunk_index = stream.size - 1;
							write_position = stream[stream.size - 1].RequestInt(elements.size);
							WriteElementsIntoChunk(elements, chunk_index, write_position);
						}

						// This will account correctly for the chunk which is incomplete
						unsigned int resize_chunk_count = (elements.size + chunk_size - 1) / chunk_size;
						
						if (resize_chunk_count > 0) {
							// We need to expand the chunks, request the slots and then quickly unlock
							// Such that we block the other threads for the least amount of time
							Expand(resize_chunk_count);

							// Reduce the elements size inside the loop such that for the last chunk
							// We have the correct remainder
							for (unsigned int index = 0; index < resize_chunk_count - 1; index++) {
								stream[chunk_index + index].RequestInt(chunk_size);
								elements.size -= chunk_size;
							}
							stream[chunk_index + resize_chunk_count - 1].RequestInt(elements.size);
							lock.Unlock();

							// After requesting all of these values, we can start writing the values
							for (unsigned int index = 0; index < resize_chunk_count - 1; index++) {
								memcpy(stream[chunk_index + index].buffer, elements.buffer, ChunkByteSize(chunk_size));
								elements.buffer += chunk_size;
								stream[chunk_index + index].FinishRequest(chunk_size);
							}

							memcpy(stream[chunk_index + resize_chunk_count - 1].buffer, elements.buffer, sizeof(T) * elements.size);
							stream[chunk_index + resize_chunk_count - 1].FinishRequest(elements.size);
						}
						else {
							// We need to release the lock since we've written into a newly created chunk by someone else
							lock.Unlock();
						}

						// We can exit now - we've written all elements
						break;
					}
					else {
						lock.WaitLocked();
						chunk_index = stream.size - 1;
						write_position = stream[chunk_index].RequestInt(elements.size);
					}
				}
				else {
					break;
				}
			}
		}

		// This version will add all the elements all at once without any other additions
		// in between unlike the AddStreamLoose but it does come at the cost of acquiring
		// The lock as the first operation - so basically multiple AddStreamAtomic cannot
		// run concurrently
		unsigned int AddStreamAtomic(Stream<T> elements) {
			lock.lock();
			unsigned int chunk_index = stream.size - 1;
			unsigned int write_position = stream[chunk_index].RequestInt(elements.size);
			if (write_position < chunk_size) {
				WriteElementsIntoChunk(elements, chunk_index, write_position);
			}

			if (elements.size > 0) {
				chunk_index++;
				unsigned int resize_chunk_count = (elements.size + chunk_size - 1) / chunk_size;
				Expand(resize_chunk_count);

				for (unsigned int index = 0; index < resize_chunk_count - 1; index++) {
					memcpy(stream[chunk_index + index].buffer, elements.buffer, ChunkByteSize(chunk_size));
					stream[chunk_index + index].RequestInt(chunk_size);
					stream[chunk_index + index].FinishRequest(chunk_size);
					elements.size -= chunk_size;
				}
				unsigned int last_chunk_index = chunk_index + resize_chunk_count - 1;
				memcpy(stream[last_chunk_index].buffer, elements.buffer, sizeof(T) * elements.size);
				stream[last_chunk_index].RequestInt(elements.size);
				stream[last_chunk_index].FinishRequest(elements.size);
			}

			lock.unlock();
		}

		void Clear() {
			for (unsigned int index = 1; index < stream.size; index++) {
				DeallocateEx(allocator, stream[index].buffer);
			}
			stream[0].Reset();
			stream.size = 1;
		}

		// Assumes the lock is grabbed or it is a single threaded context
		void Expand(unsigned int new_chunk_count) {
			ECS_ASSERT(stream.size + new_chunk_count <= stream.capacity);
			for (unsigned int index = 0; index < new_chunk_count; index++) {
				void* allocation = AllocateEx(allocator, ChunkByteSize(chunk_size));
				stream.Add({ allocation, 0, chunk_size });
			}
		}

		void Free() {
			Clear();
			DeallocateEx(allocator, stream[0].buffer);
			DeallocateEx(allocator, stream.buffer);
			memset(this, 0, sizeof(*this));
		}

		void FinishRequest(unsigned int index) {
			unsigned int chunk_index = index / chunk_size;
			stream[chunk_index].FinishRequest(1);
		}

		// Return true for early_exit if you want to end the iteration, else false
		// Returns true if it early exited, else false. Assumes that you made sure
		// that the range was written by all threads
		template<bool early_exit = false, typename Functor>
		bool ForEach(unsigned int starting_index, unsigned int final_index, Functor&& functor, unsigned int* success_index = nullptr) {
			uint2 starting_indices = IndexIndices(starting_index);
			uint2 final_indices = IndexIndices(final_index);

			auto iterate_first_chunk = [&](unsigned int last_chunk_index) {
				for (unsigned int index = starting_indices.y; index < last_chunk_index; index++) {
					if constexpr (early_exit) {
						if (functor(stream[starting_indices.x][index])) {
							if (success_index != nullptr) {
								*success_index = starting_indices.x * chunk_size + index;
							}
							return true;
						}
					}
					else {
						functor(stream[starting_indices.x][index]);
					}
				}
				return false;
			};

			if (starting_indices.x == final_indices.x) {
				bool return_value = iterate_first_chunk(final_indices.y);
				if (!return_value) {
					if (success_index != nullptr) {
						*success_index = final_index;
					}
				}
				return return_value;
			}
			else {
				bool should_early_exit = iterate_first_chunk(chunk_size);
				if (should_early_exit) {
					return true;
				}

				for (unsigned int chunk_index = starting_indices.x + 1; chunk_index < final_indices.x; chunk_index++) {
					for (unsigned int index = 0; index < chunk_size; index++) {
						if constexpr (early_exit) {
							if (functor(stream[chunk_index][index])) {
								if (success_index != nullptr) {
									*success_index = chunk_index * chunk_size + index;
								}
								return true;
							}
						}
						else {
							functor(stream[chunk_index][index]);
						}
					}
				}

				for (unsigned int index = 0; index < final_indices.y; index++) {
					if constexpr (early_exit) {
						if (functor(stream[final_indices.x][index])) {
							if (success_index != nullptr) {
								*success_index = final_indices.x * chunk_size + index;
							}
							return true;
						}
					}
					else {
						functor(stream[final_indices.x][index]);
					}
				}
			}

			if (success_index != nullptr) {
				*success_index = final_indices.x * chunk_size + final_indices.y;
			}
			return false;
		}

		// Iterates over all elements at a current moment. Makes sure that all threads finished writing that region
		// Return true to early exit in the functor, false false
		// Returns true if it early exited, else false
		// Can optionally give a uint pointer to fill in the size of the elements iterated
		// In case the iteration early exited, it will fill in the index at which it failed
		// In case there are no elements, it will leave the element count unchanged - make
		// sure you have a reasonable value for it
		template<bool early_exit = false, typename Functor>
		bool ForAllCurrentElements(Functor&& functor, unsigned int* element_count = nullptr) {
			unsigned int size = WaitWrites();
			if (size > 0) {
				return ForEach<early_exit>(0, size, functor, element_count);
			}
			else {
				return false;
			}
		}

		ECS_INLINE uint2 IndexIndices(unsigned int index) const {
			return { index / chunk_size, index % chunk_size };
		}

		// Returns the index to write at. It will set the boolean to true if it managed to find a slot, else false
		// After you write your value into it, you must use FinishRequest()
		unsigned int Request(bool& success) {
			unsigned int chunk_index = stream.size - 1;
			unsigned int write_position = stream[chunk_index].RequestInt(1);
			while (write_position >= chunk_size) {
				// If we are at the last chunk, then return
				if (chunk_index == stream.capacity - 1) {
					success = false;
					return -1;
				}
				else {
					// The chunk is full, we need another one
					bool locked_by_us = lock.TryLock();
					if (locked_by_us) {
						// Verify if the size changed
						bool found_spot = false;
						if (stream.size - 1 != chunk_index) {
							// Someone expanded before us, check to see if there
							// is space in that chunk
							chunk_index = stream.size - 1;
							write_position = stream[chunk_index].RequestInt(1);
							if (write_position < chunk_size) {
								found_spot = true;
							}
							else if (chunk_index == stream.capacity - 1) {
								// Fail - no more space
								success = false;
								lock.Unlock();
								return -1;
							}
						}

						if (!found_spot) {
							Expand(1);
							chunk_index++;
							stream[chunk_index].RequestInt(1);
							lock.Unlock();
							write_position = 0;
						}
						else {
							lock.Unlock();
						}
					}
					else {
						lock.WaitLocked();
						chunk_index = stream.size - 1;
						// Re-request the value - this has to be valid since t
						write_position = stream[chunk_index].RequestInt(1);
					}
				}
			}

			success = true;
			return chunk_index * chunk_size + write_position;
		}

		// It will wait for all writes to finish and return the size at that point
		unsigned int WaitWrites() const {
			unsigned int chunk_count = stream.size;
			// For all chunks except the last one wait for the capacity writes to finish
			unsigned int last_count = 0;
			for (unsigned int index = 0; index < chunk_count; index++) {
				last_count = stream[index].SpinWaitCapacity();
			}
			return last_count + (chunk_count - 1) * chunk_size;
		}

		// This will return the size at a current point - additions can still be
		// Made during this call and not be reflected in this count. This function
		// should be used only when you know additions are no longer being made
		// If there are still addition being made, use WaitWrites()
		unsigned int Size() const {
			unsigned int chunk_count = stream.size;
			unsigned int current_last_chunk_size = stream[chunk_count - 1].size;
			current_last_chunk_size = std::min(current_last_chunk_size, chunk_size);
			return current_last_chunk_size + (chunk_count - 1) * chunk_size;
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _chunk_size, unsigned int max_chunk_count, unsigned int initial_chunks = 1) {
			chunk_size = _chunk_size; 
			allocator = _allocator;

			stream.Initialize(allocator, 0, max_chunk_count);
			// Always have at least 1 chunk initialized
			initial_chunks = std::max(initial_chunks, (unsigned int)1);

			for (unsigned int index = 0; index < initial_chunks; index++) {
				void* allocation = AllocateEx(allocator, ChunkByteSize(chunk_size));
				stream.Add(AtomicStream<T>(allocation, 0, chunk_size));
			}
		}

		ECS_INLINE T& operator[](unsigned int index) {
			uint2 indices = IndexIndices(index);
			return stream[indices.x][indices.y];
		}

		ECS_INLINE const T& operator[](unsigned int index) const {
			uint2 indices = IndexIndices(index);
			return stream[indices.x][indices.y];
		}

		ECS_INLINE static size_t ChunkByteSize(unsigned int chunk_size) {
			return chunk_size * sizeof(T);
		}

		CapacityStream<AtomicStream<T>> stream;
		AllocatorPolymorphic allocator;
		SpinLock lock;
		unsigned int chunk_size;
	};

}