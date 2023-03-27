#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "Hashing.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

#define CHUNK_EMPTY (-1)
#define CHUNK_IN_PROGRESS (-2)

	/*
		The data is controlled as such to have a chunk fit nicely into cache lines
		Without wasting too much space for the padding.
		Can choose between a fixed table where the initial chunks don't have an index assigned
		and where the additional chunks are taken from the pool of chunks.
		The variable variant will allocate the elements as necessary
		This hash table shines for types which are small like pointers because
		they will fit nicely into the cache line. For types which are bigger (T + Identifier)
		than 28 bytes (i.e. just 1 inside the cache line) this table will not work well.
		The chunk layout is like this:
		Header header;
		T elements[];
		Identifier identifiers[];
	*/
	template<typename T, typename Identifier, typename TableHashFunction, bool fixed = true, typename ObjectHashFunction = ObjectHashFallthrough>
	struct ConcurrentHashTableSmall {
		struct Header {
			ReadWriteLock* Lock() const {
				return (ReadWriteLock*)this;
			}

			// The count is stored in the alignment bits of the pointer
			// So in the last 6 bits
			unsigned char GetCount() const {
				return count_bits;
			}

			void SetCount(unsigned char count) {
				count_bits = count;
			}

			unsigned int GetNextIndex() const {
				return (unsigned int)pointer_bits;
			}

			void SetNextIndex(unsigned int index) {
				pointer_bits = index;
			}

			void* GetPointer() const {
				// The pointer bits must be moved 6 bits to the left
				return function::SignExtendPointer((void*)(pointer_bits << 6));
			}

			void SetPointer(void* pointer) {
				uintptr_t ptr = (uintptr_t)pointer;
				ptr >>= 6;

				// The top most bits will get truncated
				pointer_bits = ptr;
			}

			size_t lock_bits : 16;
			size_t count_bits : 6;
			// The pointer bits are used as an index for the indirection type
			size_t pointer_bits : 42;
		};
		
		static_assert(sizeof(T) + sizeof(Identifier) + sizeof(Header) <= ECS_CACHE_LINE_SIZE, "Invalid concurrent hash table types");

		ConcurrentHashTableSmall() : chunks(nullptr), chunk_indirection_index(nullptr), allocated_chunk_count(0), capacity(0),
			hash_function(TableHashFunction(0)), allocated_chunks(nullptr) {}
		// The chunk count is needed for indirection tables. If not indirection table, ignore it (use any value)
		ConcurrentHashTableSmall(void* buffer, unsigned int capacity, unsigned int chunk_count, size_t additional_info = 0) {
			InitializeFromBuffer(buffer, capacity, chunk_count, additional_info);
		}
		// The chunk count is needed for indirection tables. If not indirection table, ignore it (use any value)
		ConcurrentHashTableSmall(AllocatorPolymorphic allocator, unsigned int capacity, unsigned int chunk_count, size_t additional_info = 0) {
			Initialize(allocator, capacity, chunk_count, additional_info);
		}

		ConcurrentHashTableSmall(const ConcurrentHashTableSmall& other) = default;
		ConcurrentHashTableSmall<T, Identifier, TableHashFunction, fixed, ObjectHashFunction>& operator =
			(const ConcurrentHashTableSmall<T, Identifier, TableHashFunction, fixed, ObjectHashFunction>& other) = default;

		// For indirection it returns the index that was allocated. It returns nullptr if it fails.
		// It can fail from the allocator or if no allocated chunks are left for indirection.
		void* AllocateChunk() {
			if constexpr (fixed) {
				for (unsigned int allocate_index = 0; allocate_index < allocated_chunk_count; allocate_index++) {
					// Use a load first to decide if an exchange is necessary
					if (!allocated_chunks[allocate_index].load(ECS_RELAXED)) {
						// Use the exchange
						if (!allocated_chunks[allocate_index].exchange(true, ECS_RELAXED)) {
							return (void*)allocate_index;
						}
						// Else somebody else got the lock before us - go to the next index
					}
					// If the value is set then just advance to the next elements
				}

				return CHUNK_EMPTY;
			}
			else {
				// Allocate from the allocator
				void* allocation = AllocateTs(allocator.allocator, allocator.allocator_type, ECS_CACHE_LINE_SIZE, ECS_CACHE_LINE_SIZE);
				return allocation;
			}
		}

		// This function only deallocates from the allocator for non-indirection tables 
		// or returns the index to the allocated_chunks array for the indirection ones
		void DeallocateChunk(void* chunk) {
			if constexpr (fixed) {
				// Determine the index
				unsigned int index = function::PointerDifference(chunk, chunks) / ECS_CACHE_LINE_SIZE;
				bool before = allocated_chunks[index].exchange(false, ECS_RELEASE);
				ECS_ASSERT(!before);
			}
			else {
				DeallocateTs(allocator.allocator, allocator.allocator_type, chunk);
			}
		}

		void Clear() {
			if constexpr (fixed) {
				// The indirections must be cleared.
				// Also the next indices inside the chunks must be cleared
				for (unsigned int index = 0; index < allocated_chunk_count; index++) {
					void* chunk_ptr = GetChunkRawIndex(index);
					Header* header = GetChunkHeader(chunk_ptr);
					header->SetNextIndex(CHUNK_EMPTY);
					header->SetCount(0);
				}

				memset(chunk_indirection_index, CHUNK_EMPTY, sizeof(unsigned int) * capacity);
				memset(allocated_chunks, 0, sizeof(bool) * allocated_chunk_count);
			}
			else {
				// The next allocations must be deallocated and the next indices must be reset
				for (unsigned int index = 0; index < capacity; index++) {
					void* chunk_ptr = GetChunk(index);
					// Walk through the chain and deallocate everything
					while (chunk_ptr != nullptr) {
						Header* chunk_header = GetChunkHeader(chunk_ptr);
						void* next = chunk_header->GetPointer();

						chunk_header->SetCount(0);
						chunk_header->SetPointer(nullptr);
						DeallocateChunk(chunk_ptr);

						chunk_ptr = next;
					}
				}

				memset(chunk_indirection_ptr, CHUNK_EMPTY, sizeof(void*) * capacity);
			}
		}

		// This variant is used by the one without the hash_index to retry finding without
		// recomputing the hash again for the element
		bool Find(Identifier identifier, T& value, unsigned int hash_index) const {
			void* chunk = GetChunk(hash_index);
			while (chunk != nullptr) {
				// Acquire the read lock
				Header* chunk_header = GetChunkHeader(chunk);
				ReadWriteLock* chunk_lock = chunk_header->Lock();

				chunk_lock->EnterRead();
				unsigned char count = chunk_header->GetCount();
				if (count == 0) {
					// The chunk was deallocated - retry searching
					// Should rarely happen
					chunk_lock->ExitRead();
					return Find(identifier, value, hash_index);
				}
				else {
					bool is_same_hash = IsSameChunkHash(chunk, hash_index);
					if (!is_same_hash) {
						// The chunk was deleted and reused
						chunk_lock->ExitRead();
						// Retry searching
						return Find(identifier, value);
					}

					// Same hash, count is greater than 0, the chunk is valid
					Identifier* identifiers = GetChunkIdentifier(chunk, 0);
					for (unsigned char element_index = 0; element_index < count; element_index) {
						if (identifiers[element_index] == identifier) {
							value = *GetChunkElement(chunk, element_index);
							chunk_lock->ExitRead();
							return true;
						}
					}

					// No match was found, read the next pointer and continue reading
					void* next_chunk = NextChunk(chunk);
					chunk_lock->ExitRead();

					chunk = next_chunk;
				}
			}

			// No match was found
			return false;
		}

		// This cannot return indices because the blocks might get shifted around
		// and the indices will become invalidated
		bool Find(Identifier identifier, T& value) const {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index for the array with the hash function
			unsigned int index = hash_function(key, capacity);

			return Find(identifier, value, index);
		}

		// This variant is used by the one without the hash_index to retry finding without
		// recomputing the hash again for the element
		// It returns false for fixed tables when all the allocated chunks are used
		// Else true.
		bool Insert(T value, Identifier identifier, unsigned int hash_index) {
			// Get its chunk
			void* chunk = GetChunk(hash_index);

			// We need to insert into the first chunk always
			// Grab the write lock, verify that it didn't get deleted
			// and good to go, except for when no chunk is allocated
			if (chunk == nullptr) {
				// Atomically update the index in order to indicate that we want to perform the 
				// allocation for this index
				if constexpr (fixed) {
					unsigned int expected = CHUNK_EMPTY;
					if (chunk_indirection_index[hash_index].compare_exchange_strong(expected, (void*)CHUNK_IN_PROGRESS)) {
						// We need to allocate a chunk
						unsigned int chunk_index = (unsigned int)AllocateChunk();
						if (chunk_index == CHUNK_EMPTY) {
							// No more chunks are left
							// Set the indirection to empty again and report that no more
							// space is available
							chunk_indirection_index[hash_index].store(CHUNK_EMPTY, ECS_RELEASE);
							return false;
						}

						chunk = GetChunkRawIndex(chunk_index);
						Header* chunk_header = GetChunkHeader(chunk);
						chunk_header->SetNextIndex(CHUNK_EMPTY);
						chunk_header->SetCount(1);

						*GetChunkElement(chunk, 0) = value;
						*GetChunkIdentifier(chunk, 0) = identifier;

						// Update the index to be the one desired
						chunk_indirection_index[hash_index].store(chunk_index, ECS_RELEASE);
						return true;
					}
					else {
						// When retring to insert it will get the chunk again and
						// that will wait for this to finish
						return Insert(value, identifier, hash_index);
					}
				}
				else {
					void* expected = (void*)CHUNK_EMPTY;
					if (chunk_indirection_ptr[hash_index].compare_exchange_strong(expected, (void*)CHUNK_IN_PROGRESS)) {
						chunk = AllocateChunk();

						Header* chunk_header = GetChunkHeader(chunk);
						chunk_header->SetPointer(nullptr);
						chunk_header->SetCount(1);

						*GetChunkElement(chunk, 0) = value;
						*GetChunkIdentifier(chunk, 0) = identifier;

						chunk_indirection_ptr[hash_index].store(chunk, ECS_RELEASE);
					}
					else {
						// When retring to insert it will get the chunk again and
						// that will wait for this to finish
						return Insert(value, identifier, hash_index);
					}
				}
			}
			else {
				Header* chunk_header = GetChunkHeader(chunk);
				ReadWriteLock* chunk_lock = chunk_header->Lock();
				chunk_lock->EnterWrite();

				unsigned char count = chunk_header->GetCount();
				if (count == 0) {
					// The chunk was deleted in the meantime, retry inserting
					chunk_lock->ExitWrite();
					return Insert(value, identifier, hash_index);
				}
				else {
					// We have a potentially valid chunk
					bool same_hash = IsSameChunkHash(chunk, hash_index);
					if (!same_hash) {
						chunk_lock->ExitWrite();
						return Insert(value, identifier, hash_index);
					}

					// If we can insert, do it
					if (count < ElementCount()) {
						// Valid chunk - continue
						*GetChunkElement(chunk, count) = value;
						*GetChunkIdentifier(chunk, count) = identifier;

						chunk_header->SetCount(count + 1);
						chunk_lock->ExitWrite();
						return true;
					}
					else {
						// A new chunk is needed. We don't need to update the head to pending
						// Because we have the write lock on this chunk
						void* new_chunk = AllocateChunk();

						// Used only in the fixed table
						unsigned int new_chunk_index = (unsigned int)new_chunk;
						if constexpr (fixed) {
							if (new_chunk_index == CHUNK_EMPTY){
								chunk_lock->ExitWrite();
								return false;
							}

							new_chunk = GetChunkRawIndex();
						}

						Header* new_chunk_header = GetChunkHeader(new_chunk);
						new_chunk_header->SetCount(1);

						if constexpr (fixed) {
							unsigned int chunk_index = function::PointerDifference(chunk, chunks) / ECS_CACHE_LINE_SIZE;
							new_chunk_header->SetNextIndex(chunk_index);
						}
						else {
							new_chunk_header->SetPointer(chunk);
						}

						*GetChunkElement(new_chunk, 0) = value;
						*GetChunkIdentifier(new_chunk, 0) = identifier;

						// Atomically set this as the new head
						if constexpr (fixed) {
							chunk_indirection_index[hash_index].store(new_chunk_index, ECS_RELEASE);
						}
						else {
							chunk_indirection_ptr[hash_index].store(new_chunk, ECS_RELEASE);
						}

						// Release the lock and exit
						chunk_lock->ExitWrite();
						return true;
					}
				}
			}

			// Should not reach this - we are exiting from the branches
			return false;
		}

		// The function returns false if the insertion fails. It can happen for indirection tables
		// that run out chunks. For non-indirection tables, this should never happen.
		bool Insert(T value, Identifier identifier) {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index at which the key wants to be
			unsigned int index = hash_function(key, capacity);
			
			return Insert(value, identifier, index);
		}

		// This variant is used by the one without the hash_index to retry finding without
		// recomputing the hash again for the element
		// Returns true if the erase was successful, else false (the element doesn't exist).
		bool Erase(Identifier identifier, unsigned int hash_index) {
			void* chunk = GetChunk(hash_index);

			// We will exit the loop when we find out the element or we need to retry
			while (true) {
				Header* chunk_header = GetChunkHeader(chunk);
				ReadWriteLock* chunk_lock = chunk_header->Lock();

				chunk_lock->EnterRead();
				unsigned char count = chunk_header->GetCount();
				// The chunk was deleted
				if (count == 0) {
					// Retry
					chunk_lock->ExitRead();
					return Erase(identifier, hash_index);
				}
				else {
					// Verify the hash of the chunk
					bool same_hash = IsSameChunkHash(chunk, hash_index);
					if (!same_hash) {
						// The chunk was deleted and reused
						chunk_lock->ExitRead();
						return Erase(identifier, hash_index);
					}

					Identifier* identifiers = GetChunkIdentifier(chunk, 0);
					// The chunk is good, verify it
					unsigned char element_index = 0;
					for (; element_index < count; element_index++) {
						if (identifiers[element_index] == identifier) {
							// They match
							break;
						}
					}

					// We have a match
					if (element_index < count) {
						// We can do something clever, we can transition
						// from a read lock into a write lock
						bool locked_by_us = chunk_lock->TransitionReadToWriteLock();

						// We also need to acquire the lock on the head
						void* chain_head = GetChunk(hash_index);

						// We will exit from the while manually
						while (true) {
							if (chain_head != nullptr) {
								// If the head is the same as the current chunk we are on,
								// we need to handle it differently
								if (chain_head == chunk) {
									// We can exit, we already have the write lock on the chunk
									break;
								}

								// Get a write lock on it
								Header* head_header = GetChunkHeader(chain_head);
								ReadWriteLock* head_lock = head_header->Lock();
								head_lock->EnterWrite();

								unsigned char head_count = head_header->GetCount();
								if (head_count > 0) {
									// Verify the hash index
									bool same_hash = IsSameChunkHash(chain_head, hash_index);
									if (same_hash) {
										// We can leave the while, we have the lock
										break;
									}
								}

								head_lock->ExitWrite();
							}

							chain_head = GetChunk(hash_index);
						}

						// The release for the ReadToWriteLock will be done inside the
						// if/else branches
						Header* head_header = GetChunkHeader(chain_head);
						ReadWriteLock* head_lock = head_header->Lock();
						unsigned char head_count = head_header->GetCount();
						head_count--;
						head_header->SetCount(head_count);

						*GetChunkElement(chunk, element_index) = *GetChunkElement(chain_head, head_count);
						*GetChunkIdentifier(chunk, element_index) = *GetChunkElement(chain_head, head_count);

						// If there is more than 1 element left, swap them
						if (head_count == 0) {
							// No more values inside the chunk, we need to deallocate it
							// This is the head also, we need to update to the new head
							// This should be the first instruction because new threads 
							// will start from there instead of here
							if constexpr (fixed) {
								unsigned int next_index = head_header->GetNextIndex();
								chunk_indirection_index[hash_index].store(next_index, ECS_RELEASE);

								// Remove the next link for safety
								chunk_header->SetNextIndex(CHUNK_EMPTY);
							}
							else {
								void* next_chunk = head_header->GetPointer();
								// If it is nullptr
								if (next_chunk == nullptr) {
									next_chunk = CHUNK_EMPTY;
								}
								// For the indirection we must store CHUNK_EMPTY, not nullptr
								// when there is no other link
								chunk_indirection_ptr[hash_index].store(next_chunk, ECS_RELEASE);

								// Remove the next link for safety
								chunk_header->SetPointer(nullptr);
							}

							DeallocateChunk(chunk);
						}

						// If the chunk is not the head, release the lock on the head aswell
						if (chain_head != chunk) {
							head_lock->ExitWrite();
						}
						chunk_lock->ReadToWriteUnlock(locked_by_us);
						return true;
					}

					// The chunk didn't have our element, get the next and release the lock for this chunk
					void* next_chunk = NextChunk(chunk);

					chunk_lock->ExitRead();
					// If there is no other next, exit the while
					if (next_chunk == nullptr) {
						// No more chunks to visit, the element is not in the table
						return false;
					}
					else {
						chunk = next_chunk;
					}
				}
			}

			// This should not be reached
			ECS_ASSERT(false);
			return false;
		}

		// Returns true if the erase was successful
		bool Erase(Identifier identifier) {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index at which the key wants to be
			unsigned int index = hash_function(key, capacity);

			return Erase(identifier, index);
		}

		// Verifies if the chunk has elements with the same hash
		bool IsSameChunkHash(void* chunk, unsigned int hash_index) const {
			Identifier* identifier = GetChunkIdentifier(chunk, 0);
			unsigned int identifier_hash = ObjectHashFunction::Hash(*identifier);
			identifier_hash = hash_function(identifier_hash, capacity);

			return identifier_hash == hash_index;
		}

		void* GetChunk(unsigned int index) const {
			// If indirection, check to see if the indirection is set or not
			if constexpr (fixed) {
				unsigned int chunk = chunk_indirection_index[index].load(ECS_RELAXED);
				// If the slot is not allocated, return nullptr
				if (chunk == CHUNK_EMPTY) {
					return nullptr;
				}

				if (chunk == CHUNK_IN_PROGRESS) {
					// Wait while the chunk is being allocated
					SpinWait<'!'>(&chunk_indirection_index[index], CHUNK_IN_PROGRESS);
					chunk = chunk_indirection_index[index].load(ECS_RELAXED);
				}
				// It is fine if somebody tries to set this index to CHUNK_EMPTY
				// after returning. It must do so while holding a write lock
				// and by setting the next index to CHUNK_EMPTY and the count to 0
				// which should not affect the find operations
				return GetChunkRawIndex(chunk);
			}
			else {
				// The chunk is already allocated - can return it as it is
				void* chunk = chunk_indirection_ptr[index].load(ECS_RELAXED);
				if (chunk == CHUNK_EMPTY) {
					return nullptr;
				}

				if (chunk == CHUNK_IN_PROGRESS) {
					SpinWait<'!'>(&chunk_indirection_ptr[index], CHUNK_IN_PROGRESS);
					chunk = chunk_indirection_ptr[index].load(ECS_RELAXED);
				}

				return chunk;
			}
		}

		// Offsets into the chunks based on the index
		void* GetChunkRawIndex(unsigned int index) const {
			return function::OffsetPointer(chunks, index * ECS_CACHE_LINE_SIZE);
		}

		T* GetChunkElement(void* chunk, unsigned int index) const {
			return (T*)function::OffsetPointer(chunk, sizeof(Header)) + index;
		}

		Identifier* GetChunkIdentifier(void* chunk, unsigned int index) const {
			return (Identifier*)function::OffsetPointer(chunk, sizeof(Header) + sizeof(T) * ElementCount()) + index;
		}

		Header* GetChunkHeader(void* chunk) const {
			return (Header*)chunk;
		}

		void* NextChunk(void* chunk) const {
			Header* header = GetChunkHeader(chunk);
			if constexpr (fixed) {
				unsigned int index = header->GetNextIndex();
				return index != CHUNK_EMPTY ? GetChunkRawIndex(index) : nullptr;
			}
			else {
				void* pointer = header->GetPointer();
				return pointer;
			}
		}

		// The chunk count is needed for indirection tables. If not indirection table, ignore it (use any value)
		void InitializeFromBuffer(void* buffer, unsigned int _capacity, unsigned int chunk_count, size_t additional_info = 0) {
			chunks = function::AlignPointer((uintptr_t)buffer, ECS_CACHE_LINE_SIZE);
			capacity = _capacity;
			hash_function = TableHashFunction(additional_info);

			if constexpr (fixed) {
				allocated_chunk_count = chunk_count;
				chunk_indirection_index = (std::atomic<unsigned int>*)function::OffsetPointer(chunks, ECS_CACHE_LINE_SIZE * chunk_count);
				allocated_chunks = (std::atomic<bool>*)function::OffsetPointer(chunk_indirection_index, sizeof(unsigned int) * capacity);

				// The initial state should be 0 for the allocated chunks
				memset(allocated_chunks, 0, sizeof(bool) * chunk_count);

				// The indirections needs to be CHUNK_EMPTY
				memset(chunk_indirection_index, -1, sizeof(unsigned int) * capacity);

				// Go through the chunks and set the counts to 0 and the nexts to nullptr/-1
				for (unsigned int index = 0; index < chunk_count; index++) {
					Header* header = GetChunkHeader(GetChunkRawIndex(index));
					header->SetCount(0);
					header->SetGeneration(0);

					if constexpr (fixed) {
						header->SetNextIndex(CHUNK_EMPTY);
					}
					else {
						header->SetPointer(nullptr);
					}
				}
			}
			else {
				chunk_indirection_ptr = (std::atomic<void*>*)buffer;
				// Basically making the pointers CHUNK_EMPTY
				memset(chunk_indirection_ptr, CHUNK_EMPTY, sizeof(void*) * capacity);
			}
		}

		// The chunk count is needed for indirection tables. If not indirection table, ignore it (use any value)
		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity, unsigned int chunk_count, size_t additional_info = 0) {
			unsigned int allocation_count = _capacity;
			void* allocation = Allocate(_allocator, MemoryOf(_capacity, chunk_count));
			InitializeFromBuffer(allocation, _capacity, chunk_count, additional_info);

			if constexpr (!fixed) {
				allocator = _allocator;
			}
		}

		// Returns how many elements are stored into a chunk
		constexpr static size_t ElementCount() {
			constexpr size_t count = ECS_CACHE_LINE_SIZE / (sizeof(T) + sizeof(Identifier));
			constexpr size_t remainder = ECS_CACHE_LINE_SIZE % (sizeof(T) + sizeof(Identifier));
			if constexpr (remainder >= sizeof(Header)) {
				return count;
			}
			else {
				return count - 1;
			}
		}

		// For the indirection version, the allocated chunk count needs to be specified. It should be the number
		// of chunks desired to be allocated. The capacity is the value of slots that the hash function will use
		// For the non-indirection type, it is the memory needed for the initial allocation.
		static size_t MemoryOf(unsigned int capacity, unsigned int allocated_chunk_count) {
			// Add a cache line in order to align the chunks to cache line boundaries
			if constexpr (fixed) {
				return (ECS_CACHE_LINE_SIZE + sizeof(bool)) * allocated_chunk_count + sizeof(unsigned int) * capacity + ECS_CACHE_LINE_SIZE;
			}
			else {
				return sizeof(void*) * capacity;
			}
		}

		union {
			// Indirection tables
			struct {
				std::atomic<unsigned int>* chunk_indirection_index;
				std::atomic<bool>* allocated_chunks;
				unsigned int allocated_chunk_count;
			};
			// Non-indirection tables
			struct {
				AllocatorPolymorphic allocator;
				std::atomic<void*>* chunk_indirection_ptr;

			};
		};
		void* chunks;
		unsigned int capacity;
		TableHashFunction hash_function;
	};

}

#undef CHUNK_EMPTY
#undef CHUNK_IN_PROGRESS