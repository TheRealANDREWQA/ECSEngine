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
		typedef T T;

		SparseSet() : buffer(nullptr), indirection_buffer(nullptr), size(0), capacity(0), first_empty_slot(0) {}
		SparseSet(void* _buffer, unsigned int _capacity) { InitializeFromBuffer(_buffer, _capacity); }

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(SparseSet);

		// Returns the handle to the element that was added
		unsigned int Add(T element) {		
			unsigned int handle = Allocate();
			// Place the element now
			buffer[GetIndexFromHandle(handle)] = element;
			return handle;
		}

		// Returns the handle to the element that was added
		unsigned int Add(const T* element) {
			unsigned int handle = Allocate();
			// Place the element now
			buffer[GetIndexFromHandle(handle)] = *element;
			return handle;
		}

		// Only allocates a handle, without setting the element
		unsigned int Allocate() {
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
			first_empty_slot = next_empty_slot == -1 ? -1 : next_empty_slot - capacity;
			size++;
			
			return handle;
		}

		// Resets the set, the same as initializing the buffer again with the same capacity but 0 elements
		void Clear() {
			InitializeFromBuffer(buffer, capacity);
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
			if (buffer != nullptr && capacity > 0) {
				ECSEngine::Deallocate(allocator, buffer);
				memset(this, 0, sizeof(*this));
			}
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

		// Returns true if the handle has associated an element
		ECS_INLINE bool Exists(unsigned int handle) const {
			// If it is occupied then the indirection is less than the capacity
			return indirection_buffer[handle].x < capacity;
		}

		ECS_INLINE unsigned int GetIndexFromHandle(unsigned int handle) const {
			return indirection_buffer[handle].x;
		}

		ECS_INLINE unsigned int GetHandleFromIndex(unsigned int index) const {
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
			indirection_buffer[array_index].y = indirection_index;
			first_empty_slot = handle;
			size--;
		}

		SparseSet<T> Copy(AllocatorPolymorphic allocator) const {
			SparseSet<T> result;

			result.Initialize(allocator, capacity);
			size_t copy_size = MemoryOf(capacity);
			memcpy(result.buffer, buffer, copy_size);
			result.first_empty_slot = first_empty_slot;
			result.size = size;

			return result;
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			void* allocation = allocator->Allocate(MemoryOf(_capacity), alignof(void*), debug_info);
			InitializeFromBuffer(allocator, _capacity);
		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _capacity) {
			InitializeFromBuffer((void*)_buffer, _capacity);
			_buffer = (uintptr_t)indirection_buffer + sizeof(uint2) * _capacity;
		}

		void InitializeFromBuffer(void* _buffer, unsigned int _capacity) {
			buffer = (T*)_buffer;
			indirection_buffer = (uint2*)((uintptr_t)_buffer + sizeof(T) * _capacity);

			// Has a built in check for capacity 0
			InitializeIndirectionBuffer(indirection_buffer, _capacity);

			size = 0;
			first_empty_slot = 0;
			capacity = _capacity;
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (_capacity > 0) {
				void* buffer = Allocate(allocator, MemoryOf(_capacity), alignof(void*), debug_info);
				InitializeFromBuffer(buffer, _capacity);
			}
			else {
				memset(this, 0, sizeof(*this));
			}
		}

		ECS_INLINE Stream<T> ToStream() const {
			return  { buffer, size };
		}

		ECS_INLINE static size_t MemoryOf(unsigned int capacity) {
			return (sizeof(T) + sizeof(uint2)) * capacity;
		}

		static void InitializeIndirectionBuffer(void* _indirection_buffer, unsigned int capacity) {
			if (capacity > 0) {
				uint2* indirection_buffer = (uint2*)_indirection_buffer;
				memset(indirection_buffer, 0, sizeof(uint2) * capacity);
				// Set the linked freed slots
				for (unsigned int index = 0; index < capacity - 1; index++) {
					// Add the capacity to indicate that the slot is free
					// Mostly for easier debugging
					indirection_buffer[index].x = index + 1 + capacity;
				}
				// Mostly for debug purposes
				indirection_buffer[capacity - 1].x = -1;
			}
		}

		T* buffer;
		// In the x component it is the index for that handle, in the y component is the handle for that index from the contiguous array
		uint2* indirection_buffer;
		unsigned int first_empty_slot;
		unsigned int size;
		unsigned int capacity;
	};

	// Copies a sparse set with its type unknown. Based only upon the byte size this can be performed
	ECSENGINE_API void SparseSetCopyTypeErased(const void* source, void* destination, size_t element_byte_size, AllocatorPolymorphic allocator);

	ECSENGINE_API void SparseSetCopyTypeErasedFunction(
		const void* source,
		void* destination,
		size_t element_byte_size,
		AllocatorPolymorphic allocator,
		void (*copy_function)(const void* source_element, void* destination_element, AllocatorPolymorphic allocator, void* extra_data),
		void* extra_data
	);

	// Destination needs to be a pointer to the SparseSet*
	ECSENGINE_API void SparseSetInitializeUntyped(void* destination, unsigned int capacity, unsigned int element_byte_size, void* _buffer);

	ECSENGINE_API void SparseSetDeallocateUntyped(void* sparse_set, AllocatorPolymorphic allocator);

	template<typename T>
	struct ResizableSparseSet {
		typedef T T;

		ECS_INLINE ResizableSparseSet() {}
		ECS_INLINE ResizableSparseSet(AllocatorPolymorphic _allocator, unsigned int initial_capacity = 0, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Initialize(_allocator, initial_capacity, debug_info);
		}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ResizableSparseSet);

		// Returns the handle to the element that was added
		ECS_INLINE unsigned int Add(T element) {
			if (set.size == set.capacity) {
				Resize(ECS_SPARSE_SET_RESIZE_FACTOR * set.capacity + 2);
			}

			return set.Add(element);
		}

		// Returns the handle to the element that was added
		ECS_INLINE unsigned int Add(const T* element) {
			if (set.size == set.capacity) {
				Resize(ECS_SPARSE_SET_RESIZE_FACTOR * set.capacity + 2);
			}

			return set.Add(element);
		}

		// Resets the set to contain only 0 elements
		ECS_INLINE void Clear() {
			set.Clear();
		}

		// Deallocates the buffer and clears the set
		ECS_INLINE void FreeBuffer(DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (set.buffer != nullptr) {
				DeallocateEx(allocator, set.buffer, debug_info);
			}
			set.InitializeFromBuffer(nullptr, 0);
		}

		ECS_INLINE T& operator [](unsigned int handle) {
			return set[handle];
		}

		ECS_INLINE const T& operator [](unsigned int handle) const {
			return set[handle];
		}

		// Returns the handle for the element or -1 if the element doesn't exist
		ECS_INLINE unsigned int Find(T element) const {
			return set.Find(element);
		}

		// Returns the index inside the contiguous stream or -1 if the element doesn't exist
		ECS_INLINE unsigned int FindIndex(T element) const {
			return set.Find(element);
		}

		template<typename Functor>
		ECS_INLINE unsigned int FindFunctor(Functor&& functor) const {
			return set.FindFunctor(functor);
		}

		template<typename Functor>
		ECS_INLINE unsigned int FindIndexFunctor(Functor&& functor) const {
			return set.FindIndexFunctor(functor);
		}

		ECS_INLINE unsigned int GetIndexFromHandle(unsigned int handle) const {
			return set.GetIndexFromHandle(handle);
		}

		ECS_INLINE unsigned int GetHandleFromIndex(unsigned int index) const {
			return set.GetHandleFromIndex(index);
		}

		ECS_INLINE void GetOccupiedHandles(Stream<unsigned int>& handles) const {
			set.GetOccupiedHandles(handles);
		}

		ECS_INLINE void GetOccupiedIndices(Stream<unsigned int>& indices) const {
			set.GetOccupiedIndices(indices);
		}

		// All the positions must be updated - potentially expensive
		ECS_INLINE void Remove(unsigned int handle) {
			set.Remove(handle);
		}

		// Remove using the index inside the contiguous array
		ECS_INLINE void RemoveIndex(unsigned int array_index) {
			set.RemoveIndex(array_index);
		}

		ECS_INLINE void RemoveSwapBack(unsigned int handle) {
			set.RemoveSwapBack(handle);
		}

		// Remove using the index of the element inside the contiguous array
		ECS_INLINE void RemoveSwapBackIndex(unsigned int array_index) {
			set.RemoveSwapBackIndex(array_index);
		}

		// Does not copy the elements
		ECS_INLINE void ResizeNoCopy(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (set.buffer != nullptr) {
				DeallocateEx(allocator, set.buffer, debug_info);
			}
			set.InitializeFromBuffer(AllocateEx(allocator, set.MemoryOf(new_capacity), debug_info), new_capacity);
		}

		// Copies the elements before that
		void Resize(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			void* new_buffer = AllocateEx(allocator, set.MemoryOf(new_capacity), debug_info);
			uint2* new_indirection_buffer = (uint2*)OffsetPointer(new_buffer, sizeof(T) * new_capacity);

			if (new_capacity < set.capacity) {
				// Get only the first new_capacity elements
				// These can be memcpy'ed directly
				memcpy(new_buffer, set.buffer, sizeof(T) * new_capacity);
				// The indirection can become invalid if the handles are not in the same range. But it's ok,
				// because it is impossible to maintain the references to those handles if the number is shrunken down
				// Remap the indirections such that every handle corresponds to its index
				for (unsigned int index = 0; index < new_capacity; index++) {
					new_indirection_buffer[index].x = index;
					new_indirection_buffer[index].y = index;
				}
			}
			else if (set.capacity > 0) {
				// More elements are allocated - the handles can all be copied
				memcpy(new_buffer, set.buffer, sizeof(T) * set.capacity);

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
				
				if (last_link != -1) {
					// Point the last element to the starting of the new capacity
					new_indirection_buffer[last_link].x = set.capacity + new_capacity;
				}
				// For all the other values, just set the link to the increment position
				for (unsigned int index = set.capacity; index < new_capacity - 1; index++) {
					new_indirection_buffer[index].x = index + new_capacity + 1;
				}
				new_indirection_buffer[new_capacity - 1].x = -1;
			}
			else {
				// If the size is 0, we need to set the links appropriately
				for (unsigned int index = 0; index < new_capacity - 1; index++) {
					new_indirection_buffer[index].x = index + new_capacity + 1;
				}
				new_indirection_buffer[new_capacity - 1].x = -1;
			}

			void* buffer_to_deallocate = set.buffer;
			set.buffer = (T*)new_buffer;
			set.indirection_buffer = new_indirection_buffer;
			// If the set is full and a resize happenend, for the case that it grew, set the first empty slot to the capacity
			// of the old set. If the capacity is reduced, it doesn't matter
			if (set.size == set.capacity) {
				set.first_empty_slot = set.capacity;
			}
			set.capacity = new_capacity;

			if (buffer_to_deallocate != nullptr) {
				DeallocateEx(allocator, buffer_to_deallocate, debug_info);
			}
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int initial_capacity = 0, DebugInfo debug_info = ECS_DEBUG_INFO) {
			allocator = _allocator;
			if (initial_capacity > 0) {
				set = SparseSet<T>(AllocateEx(allocator, set.MemoryOf(initial_capacity), debug_info), initial_capacity);
			}
			else {
				set = SparseSet<T>(nullptr, 0);
			}
		}

		ECS_INLINE Stream<T> ToStream() const {
			return set.ToStream();
		}

		ECS_INLINE static size_t MemoryOf(unsigned int capacity) {
			return SparseSet<T>::MemoryOf(capacity);
		}

		SparseSet<T> set;
		AllocatorPolymorphic allocator;
	};

}