#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Utilities/Assert.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "Stream.h"

#define ECS_SPARSE_SET_RESIZE_FACTOR (1.5f)

namespace ECSEngine {

	// It is represented as a dense array and a sparse array. The sparse array contains indices that indirect the access and 
	// that allow fast removal. Constant time addition and removal by keeping an intrusive linked list inside the indirection buffer
	template<typename T>
	struct SparseSet {
		SparseSet() : buffer(nullptr), indirection_buffer(nullptr), size(0), capacity(0), first_empty_slot(0) {}
		SparseSet(void* _buffer, unsigned int _capacity) { InitializeFromBuffer(_buffer, _capacity); }

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(SparseSet);

		// Returns the handle to the element that was added
		unsigned int Add(T element) {
			ECS_ASSERT(size < capacity);

			// Grab the next valid slot indicated by first empty slot
			// This value should be greater or equal to the capacity in order to be free
			// otherise it is occupied
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

		// Returns the handle to the element that was added
		unsigned int Add(const T* element) {
			ECS_ASSERT(size < capacity);

			// Grab the next valid slot indicated by first empty slot
			// This value should be greater or equal to the capacity in order to be free
			// otherise it is occupied
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

		// Resets the set, the same as initializing the buffer again with the same capacity but 0 elements
		void Clear() {
			InitializeFromBuffer(buffer, capacity);
		}

		ECS_INLINE T& operator [](unsigned int handle) {
			unsigned int index = GetIndexFromHandle(handle);

			ECS_ASSERT(index < size);
			return buffer[index];
		}

		ECS_INLINE const T& operator [](unsigned int handle) const {
			unsigned int index = GetIndexFromHandle(handle);

			ECS_ASSERT(index < size);
			return buffer[index];
		}

		// Returns the handle for the element or -1 if the element doesn't exist
		unsigned int Find(T element) const {
			for (unsigned int index = 0; index < size; index++) {
				if (buffer[index] == element) {
					return indirection_buffer[index].y;
				}
			}

			return -1;
		}

		// Returns the index inside the contiguous stream or -1 if the element doesn't exist
		unsigned int FindIndex(T element) const {
			for (unsigned int index = 0; index < size; index++) {
				if (buffer[index] == element) {
					return index;
				}
			}

			return -1;
		}

		template<typename Functor>
		unsigned int FindFunctor(Functor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (functor(buffer[index])) {
					return indirection_buffer[index].y;
				}
			}

			return -1;
		}

		template<typename Functor>
		unsigned int FindIndexFunctor(Functor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (functor(buffer[index])) {
					return index;
				}
			}

			return -1;
		}

		unsigned int GetIndexFromHandle(unsigned int handle) const {
			return indirection_buffer[handle].x;
		}

		unsigned int GetHandleFromIndex(unsigned int index) const {
			return indirection_buffer[index].y;
		}

		// The buffer needs to have at least size elements
		void GetOccupiedHandles(Stream<unsigned int>& handles) const {
			handles.size = 0;

			for (unsigned int index = 0; index < capacity; index++) {
				if (indirection_buffer[index].x < capacity) {
					handles[handles.size++] = indirection_buffer[index].y;
				}
			}
			ECS_ASSERT(handles.size == size);
		}
		
		// The buffer needs to have at least size elements
		void GetOccupiedIndices(Stream<unsigned int>& indices) const {
			indices.size = 0;

			for (unsigned int index = 0; index < capacity; index++) {
				if (indirection_buffer[index].x < capacity) {
					indices[indices.size++] = index;
				}
			}
			ECS_ASSERT(indices.size == size);
		}

		// All the positions must be updated - potentially expensive
		void Remove(unsigned int handle) {
			unsigned int array_index = GetIndexFromHandle(handle);
			RemoveIndex(array_index);
		}

		// Remove using the index inside the contiguous array
		void RemoveIndex(unsigned int array_index) {
			ECS_ASSERT(array_index < size);
			unsigned int handle = GetHandleFromIndex(array_index);

			for (unsigned int index = array_index; index < size - 1; index++) {
				buffer[index] = buffer[index + 1];
				unsigned int indirection_index = indirection_buffer[index + 1].y;
				indirection_buffer[index].y = indirection_index;
				indirection_buffer[indirection_index].x = index;
			}

			// Invalidate the handle and add it to the linked list
			// If the set was previously full, the next needs to be -1
			// Mostly for debug purposes tho
			indirection_buffer[handle].x = size == capacity ? -1 : first_empty_slot + capacity;
			first_empty_slot = handle;
			size--;
		}

		void RemoveSwapBack(unsigned int handle) {
			unsigned int array_index = GetIndexFromHandle(handle);
			RemoveSwapBackIndex(array_index);
		}

		// Remove using the index of the element inside the contiguous array
		void RemoveSwapBackIndex(unsigned int array_index) {
			ECS_ASSERT(array_index < size);
			unsigned int handle = GetHandleFromIndex(array_index);

			buffer[array_index] = buffer[size - 1];
			unsigned int indirection_index = indirection_buffer[size - 1].y;
			indirection_buffer[indirection_index].x = array_index;

			// Invalidate the handle and add it to the linked list
			// If the set was previously full, the next needs to be -1
			// Mostly for debug purposes tho
			indirection_buffer[handle].x = size == capacity ? -1 : first_empty_slot + capacity;
			first_empty_slot = handle;
			size--;
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _capacity) {
			void* allocation = allocator->Allocate(MemoryOf(_capacity));
			InitializeFromBuffer(allocator, _capacity);
		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _capacity) {
			InitializeFromBuffer((void*)_buffer, _capacity);
			_buffer = (uintptr_t)indirection_buffer + sizeof(uint2) * _capacity;
		}

		void InitializeFromBuffer(void* _buffer, unsigned int _capacity) {
			buffer = (T*)_buffer;
			indirection_buffer = (uint2*)((uintptr_t)_buffer + sizeof(T) * _capacity);

			// Don't do anything if the capacity is 0
			if (_capacity > 0) {
				memset(indirection_buffer, 0, sizeof(uint2) * _capacity);
				// Set the linked freed slots
				for (unsigned int index = 0; index < _capacity - 1; index++) {
					// Add the capacity to indicate that the slot is free
					// Mostly for easier debugging
					indirection_buffer[index].x = index + 1 + _capacity;
				}
				// Mostly for debug purposes
				indirection_buffer[_capacity - 1].x = -1;
			}

			size = 0;
			first_empty_slot = 0;
			capacity = _capacity;
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity) {
			void* buffer = Allocate(allocator, MemoryOf(_capacity));
			InitializeFromBuffer(buffer, _capacity);
		}

		Stream<T> ToStream() const {
			return  { buffer, size };
		}

		static size_t MemoryOf(unsigned int capacity) {
			return (sizeof(T) + sizeof(uint2)) * capacity;
		}

		T* buffer;
		// In the x component it is the index for that handle, in the y component is the handle for that index from the contiguous array
		uint2* indirection_buffer;
		unsigned int first_empty_slot;
		unsigned int size;
		unsigned int capacity;
	};

	template<typename T>
	struct ResizableSparseSet {
		ResizableSparseSet() {}
		ResizableSparseSet(AllocatorPolymorphic _allocator, unsigned int initial_capacity = 0) : allocator(_allocator) {
			if (initial_capacity > 0) {
				set = SparseSet<T>(AllocateEx(allocator, set.MemoryOf(initial_capacity)), initial_capacity);
			}
			else {
				set = SparseSet<T>(nullptr, 0);
			}
		}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ResizableSparseSet);

		// Returns the handle to the element that was added
		unsigned int Add(T element) {
			if (set.size == set.capacity) {
				Resize(ECS_SPARSE_SET_RESIZE_FACTOR * set.capacity);
			}

			return set.Add(element);
		}

		// Returns the handle to the element that was added
		unsigned int Add(const T* element) {
			if (set.size == set.capacity) {
				Resize(ECS_SPARSE_SET_RESIZE_FACTOR * set.capacity);
			}

			return set.Add(element);
		}

		// Resets the set to contain only 0 elements
		void Clear() {
			set.Clear();
		}

		// Deallocates the buffer and clears the set
		void FreeBuffer() {
			if (set.buffer != nullptr) {
				DeallocateEx(allocator, set.buffer);
			}
			Clear();
		}

		ECS_INLINE T& operator [](unsigned int handle) {
			return set[handle];
		}

		ECS_INLINE const T& operator [](unsigned int handle) const {
			return set[handle];
		}

		// Returns the handle for the element or -1 if the element doesn't exist
		unsigned int Find(T element) const {
			return set.Find(element);
		}

		// Returns the index inside the contiguous stream or -1 if the element doesn't exist
		unsigned int FindIndex(T element) const {
			return set.Find(element);
		}

		template<typename Functor>
		unsigned int FindFunctor(Functor&& functor) const {
			return set.FindFunctor(functor);
		}

		template<typename Functor>
		unsigned int FindIndexFunctor(Functor&& functor) const {
			return set.FindIndexFunctor(functor);
		}

		unsigned int GetIndexFromHandle(unsigned int handle) const {
			return set.GetIndexFromHandle(handle);
		}

		unsigned int GetHandleFromIndex(unsigned int index) const {
			return set.GetHandleFromIndex(index);
		}

		void GetOccupiedHandles(Stream<unsigned int>& handles) const {
			set.GetOccupiedHandles(handles);
		}

		void GetOccupiedIndices(Stream<unsigned int>& indices) const {
			set.GetOccupiedIndices(indices);
		}

		// All the positions must be updated - potentially expensive
		void Remove(unsigned int handle) {
			set.Remove(handle);
		}

		// Remove using the index inside the contiguous array
		void RemoveIndex(unsigned int array_index) {
			set.RemoveIndex(array_index);
		}

		void RemoveSwapBack(unsigned int handle) {
			set.RemoveSwapBack(handle);
		}

		// Remove using the index of the element inside the contiguous array
		void RemoveSwapBackIndex(unsigned int array_index) {
			set.RemoveSwapBackIndex(array_index);
		}

		// Does not copy the elements
		void ResizeNoCopy(unsigned int new_capacity) {
			if (set.buffer != nullptr) {
				DeallocateEx(allocator, set.buffer);
			}
			set.InitializeFromBuffer(AllocateEx(allocator, set.MemoryOf(new_capacity)), new_capacity);
		}

		// Copies the elements before that
		void Resize(unsigned int new_capacity) {
			void* new_buffer = AllocateEx(allocator, set.MemoryOf(new_capacity));
			
			if (new_capacity < set.capacity) {
				// Get only the first new_capacity elements
				// These can be memcpy'ed directly
				memcpy(new_buffer, set.buffer, sizeof(T) * new_capacity);
				uint2* new_indirection_buffer = (uint2*)function::OffsetPointer(new_buffer, sizeof(T) * new_capacity);
				// The indirection can become invalid if the handles are not in the same range. But it's ok,
				// because it is impossible to maintain the references to those handles if the number is shrunken down
				// Remap the indirections such that every handle corresponds to its index
				for (unsigned int index = 0; index < new_capacity; index++) {
					new_indirection_buffer[index].x = index;
					new_indirection_buffer[index].y = index;
				}
			}
			else {
				// More elements are allocated - the handles can all be copied
				memcpy(new_buffer, set.buffer, sizeof(T) * set.capacity);
				uint2* new_indirection_buffer = (uint2*)function::OffsetPointer(new_buffer, sizeof(T) * new_capacity);

				// The indirections can also be copied - the free ones need to be modified
				memcpy(new_indirection_buffer, set.indirection_buffer, sizeof(uint2) * set.capacity);
				
				// Walk using the head of the linked list
				unsigned int head = set.first_empty_slot;
				unsigned int last_link = head;
				while (head != -1) {
					last_link = head;
					new_indirection_buffer[head].x -= set.capacity;
					unsigned int next_link = new_indirection_buffer[head].x;

					// Make sure to keep the relation with the capacity intact
					new_indirection_buffer[head].x += new_capacity;
					head = next_link;
				}

				// Point the last element to the starting of the new capacity
				new_indirection_buffer[last_link].x = set.capacity + new_capacity;
				// For all the other values, just set the link to the increment position
				for (unsigned int index = set.capacity; index < new_capacity - 1; index++) {
					new_indirection_buffer[index].x = index + new_capacity + 1;
				}
				new_indirection_buffer[new_capacity - 1].x = -1;
			}

			void* buffer_to_deallocate = set.buffer;
			set.buffer = (T*)new_buffer;
			set.indirection_buffer = (uint2*)function::OffsetPointer(new_buffer, sizeof(T) * set.capacity);
			// If the set is full and a resize happenend, for the case that it grew, set the first empty slot to the capacity
			// of the old set. If the capacity is reduced, it doesn't matter since
			if (set.size == set.capacity) {
				set.first_empty_slot = set.capacity;
			}
			set.capacity = new_capacity;

			if (buffer_to_deallocate != nullptr) {
				DeallocateEx(allocator, buffer_to_deallocate);
			}
		}

		Stream<T> ToStream() const {
			return set.ToStream();
		}

		static size_t MemoryOf(unsigned int capacity) {
			return SparseSet<T>::MemoryOf(capacity);
		}

		SparseSet<T> set;
		AllocatorPolymorphic allocator;
	};

}