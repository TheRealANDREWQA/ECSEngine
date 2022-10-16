#pragma once
#include "../Core.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"
#include "Hashing.h"
#include "../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	/*
		This hash table is suited for types and identifiers which are greater in size
		This offers the possibility of generating and element and identifier on the stack
		and have it stored in a multithreaded fashion. The small table has the disadvantage
		that if you want to store pointers to the elements when you actually wanted to store themselves
		you get an extra indirection - can hurt performance - and also need to make sure that the
		allocation of those objects is done threadsafe, possibly from the allocator or from a thread local pool,
		which may or may not be a huge deal. This table doesn't suffer from those problems but it might be slower for the find,
		insert and delete operations. Testing must be done to see which performs better.

		
		The principle of this table: Robin Hood hashing. It is a vanilla implementation, without any SIMD or fancy stuff.
		The hash bits are selected from the middle of the key such that it offers better culling for power of two hash tables at small sizes. 
		It keeps a single read write lock for each element, and the elements are stored in an AoS style in order to minimize false
		sharing between locks. There is no wrap around to avoid deadlocks and to simplify the code path. 
		There are an extra of 32 elements stored at the end in order to allow the last elements shift there without wraparound.
		It does not support resizing - it would add complexity that is not worth.
	*/
	template<typename T, typename Identifier, typename TableHashFunction, typename ObjectHashFunction = ObjectHashFallthrough>
	struct ConcurrentHashTable {
		ConcurrentHashTable() : buffer(nullptr), capacity(0), hash_function(TableHashFunction(0)) {}
		ConcurrentHashTable(void* buffer, unsigned int _capacity) {
			InitializeFromBuffer(buffer, _capacity);
		}

		ConcurrentHashTable(const ConcurrentHashTable& other) = default;
		ConcurrentHashTable<T, Identifier, TableHashFunction, ObjectHashFunction>& operator = 
			(const ConcurrentHashTable<T, Identifier, TableHashFunction, ObjectHashFunction>& other) = default;

		struct Node {
			T value;
			Identifier identifier;
			unsigned char distance;
			unsigned char hash_bits;
			ReadWriteLock lock;
		};

		unsigned int GetCapacity() const {
			return capacity;
		}

		unsigned int GetExtendedCapacity() const {
			return capacity + 31;
		}

		static unsigned char ExtractHashBits(unsigned int hash) {
			// Return the bits from the 12th to the 19th
			return hash >> 12;
		}

		void Clear() {
			// Clear the dummy element at the end aswell

			// Set the distances to 0 and unlock the locks
			for (unsigned int index = 0; index < GetExtendedCapacity() + 1; index++) {
				buffer[index].distance = 0;
				buffer[index].lock.Clear();
			}
		}

		// Returns the index for that element with a read lock on the element if it is found
		// If it is not found, then it will return -1
		template<bool use_compare_function = true>
		unsigned int FindLocked(Identifier identifier, unsigned int key, unsigned int hash_index) const {
			unsigned int hash_bits = ExtractHashBits(key);

			unsigned char current_distance = 1;
			// Acquire the read lock for the index
			while (true) {
				buffer[hash_index].lock.EnterRead();
				unsigned char element_distance = buffer[hash_index].distance;

				if (element_distance < current_distance) {
					// The chain was properly scanned - there are no more elements to look for
					buffer[hash_index].lock.ExitRead();
					return -1;
				}
				else if (element_distance == current_distance && buffer[hash_index].hash_bits == hash_bits) {
					// The element is a candidate
					if constexpr (use_compare_function) {
						if (buffer[hash_index].identifier.Compare(identifier)) {
							return hash_index;
						}
					}
					else {
						if (buffer[hash_index].identifier == identifier) {
							return hash_index;
						}
					}
				}

				// Keep searching, increment the index and the distance
				current_distance++;
				hash_index++;
			}

			// Should not be reached
			return -1;
		}

		// This cannot return indices because the blocks might get shifted around
		// and the indices will become invalidated
		template<bool use_compare_function = true>
		bool Find(Identifier identifier, T& value) const {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index at which the key wants to be
			unsigned int index = hash_function(key, capacity);

			unsigned int element_index = FindLocked(identifier, key, index);
			if (element_index != -1) {
				value = buffer[element_index].value;
				buffer[element_index].lock.ExitRead();
				return true;
			}
			return false;
		}

		// The function returns true if the insertion was succesful - there is still space left in the table
		// else it returns false.
		bool Insert(T value, Identifier identifier) {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index at which the key wants to be
			unsigned int index = hash_function(key, capacity);

			unsigned char current_distance = 1;
			unsigned char hash_bits = ExtractHashBits(key);

			// We will exit manually from the loop
			while (current_distance < 31) {
				// Acquire the write lock directly - it is not worth to get the read lock
				// and then transition into the write lock
				buffer[index].lock.EnterWrite();
				unsigned char element_distance = buffer[index].distance;

				if (element_distance == 0) {
					// Set the distance, the hash bits, the value and the identifier
					buffer[index].distance = current_distance;
					buffer[index].hash_bits = hash_bits;
					buffer[index].value = value;
					buffer[index].identifier = identifier;

					// Can release the lock and exit
					buffer[index].lock.ExitWrite();
					return true;
				}

				// Swap the elements
				T temp_value = buffer[index].value;
				Identifier temp_identifier = buffer[index].identifier;
				unsigned char temp_hash_bits = buffer[index].hash_bits;

				buffer[index].value = value;
				buffer[index].identifier = identifier;
				buffer[index].distance = current_distance;
				buffer[index].hash_bits = hash_bits;

				// Release the lock at this pointer because the stack swaps don't need to be protected
				buffer[index].lock.ExitWrite();

				value = temp_value;
				identifier = temp_identifier;
				current_distance = element_distance + 1;
				hash_bits = temp_hash_bits;

				// Increment the index
				index++;
			}

			// If we couldn't get a slot in 31 iterations, there is no more space or too many collisions
			return false;
		}

		// Returns true if the erase was successful, else false
		template<bool use_compare_function = true>
		bool Erase(Identifier identifier) {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index at which the key wants to be
			unsigned int index = hash_function(key, capacity);

			unsigned int element_index = FindLocked(identifier, key, index);
			if (element_index != -1) {
				// We have a read lock on it - transition into a write lock
				bool is_locked_by_us = buffer[element_index].lock.TransitionReadToWriteLock();

				// Acquire the lock for the next element
				buffer[element_index + 1].lock.EnterWrite();
				// If the distance is zero or 1, then we can stop
				if (buffer[element_index + 1].distance <= 1) {
					// Release the locks
					buffer[element_index].lock.ReadToWriteUnlock(is_locked_by_us);
					buffer[element_index + 1].lock.ExitWrite();
				}
				else {
					// Do a manual iteration because we need to release the read_to_write lock
					// for the first item
					
					// Swap the elements
					buffer[element_index].value = buffer[element_index + 1].value;
					buffer[element_index].identifier = buffer[element_index + 1].identifier;
					buffer[element_index].distance = buffer[element_index + 1].distance - 1;
					buffer[element_index].hash_bits = buffer[element_index + 1].hash_bits;

					// Release the read_to_write lock on the first item
					buffer[element_index].lock.ReadToWriteUnlock(is_locked_by_us);

					element_index++;
					// We can start the loop now - we have the lock on the first item
					// It is safe to index into the next element even if we are the last one
					// (the capacity + 31st) because we have a dummy 32nd
					// Grab the lock for the next element first
					buffer[element_index + 1].lock.EnterWrite();
					while (buffer[element_index + 1].distance > 1) {
						buffer[element_index].value = buffer[element_index + 1].value;
						buffer[element_index].identifier = buffer[element_index + 1].identifier;
						buffer[element_index].distance = buffer[element_index + 1].distance - 1;
						buffer[element_index].hash_bits = buffer[element_index + 1].hash_bits;

						// Release the lock on the current one and grab it for the next element
						buffer[element_index].lock.ExitWrite();
						element_index++;

						buffer[element_index + 1].lock.EnterWrite();
					}

					// We need to release 2 locks, one for the current and the other for the next
					buffer[element_index].lock.ExitWrite();
					buffer[element_index + 1].lock.ExitWrite();
				}
		
				return true;
			}

			return false;
		}

		void* GetAllocatedBuffer() const {
			return buffer;
		}

		void InitializeFromBuffer(void* buffer, unsigned int _capacity) {
			buffer = (Node*)buffer;
			capacity = _capacity;

			Clear();
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity) {
			void* allocation = AllocateEx(MemoryOf(_capacity));
			InitializeFromBuffer(allocation, _capacity);
		}

		static size_t MemoryOf(unsigned int capacity) {
			// Extended capacity here + 1. We add one because for erase we will use a 
			// dummy element at the end to avoid a check.
			return sizeof(Node) * (capacity + 32);
		}

		Node* buffer;
		unsigned int capacity;
		TableHashFunction hash_function;
	};

}