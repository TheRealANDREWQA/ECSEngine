#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Utilities/Assert.h"
#include "../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	// It is represented as a dense array and a sparse array. The sparse array contains indices that indirect the access and 
	// that allow fast removal. Constant time addition and removal by keeping an intrusive linked list inside the indirection buffer
	template<typename T>
	struct SparseSet {
		SparseSet() : buffer(nullptr), indirection_buffer(nullptr), size(0), capacity(0), first_empty_slot(0) {}
		SparseSet(void* _buffer, unsigned int _capacity) { InitializeFromBuffer(_buffer, _capacity); }

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(SparseSet);

		unsigned int Add(T element) {
			ECS_ASSERT(size < capacity);

			// Grab the next valid slot indicated by first empty slot
			unsigned int next_empty_slot = indirection_buffer[first_empty_slot].x;

			// Now set the index to be the size of the array and that slot to point to this one
			indirection_buffer[first_empty_slot].x = size;
			indirection_buffer[size].y = first_empty_slot;
	
			unsigned int handle = first_empty_slot;
			// Update the new head
			first_empty_slot = next_empty_slot - capacity;

			// Place the element now
			buffer[size] = element;
			size++;
			return handle;
		}

		unsigned int Add(const T* element) {
			ECS_ASSERT(size < capacity);

			// Grab the next valid slot indicated by first empty slot
			unsigned int next_empty_slot = indirection_buffer[first_empty_slot].x;

			// Now set the index to be the size of the array and that slot to point to this one
			indirection_buffer[first_empty_slot].x = size;
			indirection_buffer[size].y = first_empty_slot;

			unsigned int handle = first_empty_slot;
			// Update the new head
			first_empty_slot = next_empty_slot - capacity;

			// Place the element now
			buffer[size] = *element;
			size++;
			return handle;
		}

		ECS_INLINE T& operator [](unsigned int handle) {
			ECS_ASSERT(indirection_buffer[handle].x < size);
			return buffer[indirection_buffer[handle].x];
		}

		ECS_INLINE const T& operator [](unsigned int handle) const {
			ECS_ASSERT(indirection_buffer[handle].x < size);
			return buffer + indirection_buffer[handle].x;
		}

		// All the positions must be updated - potentially expensive
		void Remove(unsigned int handle) {
			unsigned int array_index = indirection_buffer[handle].x;
			ECS_ASSERT(array_index < size);
			for (unsigned int index = array_index; index < size - 1; index++) {
				buffer[index] = buffer[index + 1];
				unsigned int indirection_index = indirection_buffer[index + 1].y;
				indirection_buffer[index].y = indirection_index;
				indirection_buffer[indirection_index].x = index;
			}

			// Invalidate the handle and add it to the linked list
			indirection_buffer[handle].x = first_empty_slot + capacity;
			first_empty_slot = handle;
			size--;
		}

		void RemoveSwapBack(unsigned int handle) {
			unsigned int array_index = indirection_buffer[handle].x;
			ECS_ASSERT(array_index < size);
			
			buffer[array_index] = buffer[size - 1];
			unsigned int indirection_index = indirection_buffer[size - 1].y;
			indirection_buffer[indirection_index].x = array_index;
			
			// Invalidate the handle and add it to the linked list
			indirection_buffer[handle].x = first_empty_slot + capacity;
			first_empty_slot = handle;
			size--;
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _capacity) {
			capacity = _capacity;

			void* allocation = allocator->Allocate(MemoryOf(capacity));
			buffer = (T*)allocation;
			indirection_buffer = (uint2*)((uintptr_t)allocation + sizeof(T) * capacity);
			size = 0;
			first_empty_slot = 0;

		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _capacity) {
			buffer = (T*)_buffer;
			indirection_buffer = (uint2*)((uintptr_t)_buffer + sizeof(T) * _capacity);

			memset(indirection_buffer, 0, sizeof(uint2) * _capacity);
			// Set the linked freed slots
			for (unsigned int index = 0; index < _capacity - 1; index++) {
				indirection_buffer[index].x = index + 1 + _capacity;
			}
			indirection_buffer[_capacity].x = -1;
			size = 0;
			first_empty_slot = 0;
			capacity = _capacity;

			_buffer = (uintptr_t)indirection_buffer + sizeof(uint2) * _capacity;
		}

		void InitializeFromBuffer(void* _buffer, unsigned int _capacity) {
			buffer = (T*)_buffer;
			indirection_buffer = (uint2*)((uintptr_t)_buffer + sizeof(T) * _capacity);

			memset(indirection_buffer, 0, sizeof(uint2) * _capacity);
			// Set the linked freed slots
			for (unsigned int index = 0; index < _capacity - 1; index++) {
				indirection_buffer[index].x = index + 1 + _capacity;
			}
			indirection_buffer[_capacity].x = -1;
			size = 0;
			first_empty_slot = 0;
			capacity = _capacity;
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity) {
			void* buffer = Allocate(allocator, MemoryOf(_capacity));
			InitializeFromBuffer(buffer, _capacity);
		}

		static size_t MemoryOf(unsigned int capacity) {
			return (sizeof(T) + sizeof(uint2)) * capacity;
		}

		T* buffer;
		uint2* indirection_buffer;
		unsigned int first_empty_slot;
		unsigned int size;
		unsigned int capacity;
	};

}