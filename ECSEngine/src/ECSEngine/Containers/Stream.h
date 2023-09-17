#pragma once
#include "../Core.h"
#include "../Utilities/Assert.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include <stdint.h>
#include <string.h>
#include <type_traits>

#define ECS_RESIZABLE_STREAM_FACTOR (1.5f)

namespace ECSEngine {

#define ECS_STACK_CAPACITY_STREAM(type, name, capacity) type _##name[capacity]; ECSEngine::CapacityStream<type> name(_##name, 0, capacity);
#define ECS_STACK_CAPACITY_STREAM_DYNAMIC(type, name, capacity) void* _##name = ECS_STACK_ALLOC(sizeof(type) * (capacity)); ECSEngine::CapacityStream<type> name(_##name, 0, (capacity));

#define ECS_STACK_CAPACITY_STREAM_OF_STREAMS(type, name, count, capacity_per_stream)	ECS_STACK_CAPACITY_STREAM(type, __##name, capacity_per_stream * count); \
																						ECS_STACK_CAPACITY_STREAM(ECSEngine::CapacityStream<type>, name, count); \
																						for (size_t name##_index = 0; name##_index < count; name##_index++) { \
																							name[name##_index].InitializeFromBuffer(__##name.buffer + capacity_per_stream * name##_index, 0, capacity_per_stream); \
																						}

#define ECS_STACK_VOID_STREAM(name, capacity) char __storage##name[capacity]; ECSEngine::CapacityStream<void> name(__storage##name, 0, capacity);
#define ECS_STACK_VOID_STREAM_DYNAMIC(name, capacity) void* __storage##name = ECS_STACK_ALLOC(capacity); ECSEngine::CapacityStream<void> name(__storage##name, 0, capacity);

#define ECS_TEMP_ASCII_STRING(name, size) char name##_temp_memory[size]; \
ECSEngine::CapacityStream<char> name(name##_temp_memory, 0, size);

#define ECS_TEMP_STRING(name, size) wchar_t name##_temp_memory[size]; \
ECSEngine::CapacityStream<wchar_t> name(name##_temp_memory, 0, size);

	template<typename T>
	struct CapacityStream;

	template <typename T>
	struct Stream
	{
		typedef T T;

		ECS_INLINE Stream() : buffer(nullptr), size(0) {}
		ECS_INLINE Stream(const void* buffer, size_t size) : buffer((T*)buffer), size(size) {}
		ECS_INLINE Stream(CapacityStream<T> other) : buffer(other.buffer), size(other.size) {}

		template<typename = std::enable_if_t<std::is_same_v<char, T>>>
		ECS_INLINE Stream(const char* string) : buffer((char*)string) {
			ECS_ASSERT(string != nullptr);
			size = strlen(string);
		}

		template<typename = std::enable_if_t<std::is_same_v<wchar_t, T>>>
		ECS_INLINE Stream(const wchar_t* string) : buffer((wchar_t*)string) {
			ECS_ASSERT(string != nullptr);
			size = wcslen(string);
		}

		// Cannot use this because Intellisense freaks out and gives false alarms that are really annoying
		//template<size_t static_size, typename = std::enable_if_t<std::negation_v<std::disjunction<std::is_same<wchar_t, T>, std::is_same<char, T>>>>>
		//ECS_INLINE Stream(const T(&static_array)[static_size]) : buffer((T*)static_array), size(static_size) {}

		ECS_INLINE Stream(const Stream& other) = default;
		ECS_INLINE Stream<T>& operator = (const Stream<T>& other) = default;

		ECS_INLINE unsigned int Add(T element) {
			buffer[size++] = element;
			return size - 1;
		}

		ECS_INLINE unsigned int Add(const T* element) {
			buffer[size++] = *element;
			return size - 1;
		}

		// Returns the first index
		unsigned int AddStream(Stream<T> other) {
			CopySlice(size, other);
			unsigned int previous_size = size;
			size += other.size;
			return previous_size;
		}

		// Returns the old buffer
		void* AddResize(T element, AllocatorPolymorphic allocator, bool deallocate_old = false) {
			void* old_buffer = Expand(allocator, 1, deallocate_old);
			buffer[size - 1] = element;
			return old_buffer;
		}

		// Returns the old buffer
		void* AddResize(const T* element, AllocatorPolymorphic allocator, bool deallocate_old = false) {
			void* old_buffer = Expand(allocator, 1, deallocate_old);
			buffer[size - 1] = *element;
			return old_buffer;
		}

		// Returns the old buffer
		void* AddResize(Stream<T> elements, AllocatorPolymorphic allocator, bool deallocate_old = false) {
			size_t old_size = elements.size;
			void* old_buffer = Expand(allocator, elements.size, deallocate_old);
			CopySlice(old_size, elements);
			return old_buffer;
		}

		// Increments the pointer and decrements the size
		ECS_INLINE void Advance(int64_t amount = 1) {
			// Adding a negative value will result in the appropriate outcome
			size_t unsigned_amount = (size_t)amount;
			buffer += unsigned_amount;
			size -= unsigned_amount;
		}

		// Increments the pointer and decrements the size with the given amount
		// and returns the stream with those values. It does not modify this one
		ECS_INLINE Stream<T> AdvanceReturn(int64_t amount = 1) const {
			Stream<T> copy = *this;
			copy.Advance(amount);
			return copy;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, size_t count) {
			memcpy(buffer, memory, count * sizeof(T));
			size = count;
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<T> other) {
			Copy(other.buffer, other.size);
		}

		ECS_INLINE Stream<T> Copy(AllocatorPolymorphic allocator) const {
			Stream<T> result;
			result.InitializeAndCopy(allocator, *this);
			return result;
		}

		// it will not set the size
		ECS_INLINE void CopySlice(size_t starting_index, const void* memory, size_t count) {
			memcpy(buffer + starting_index, memory, sizeof(T) * count);
		}

		// it will not set the size
		ECS_INLINE void CopySlice(size_t starting_index, Stream<T> other) {
			CopySlice(starting_index, other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, sizeof(T) * size);
		}

		// Added return type in order to be compatible with StreamCoalescedDeepCopy
		ECS_INLINE Stream<T> CopyTo(uintptr_t& memory) const {
			return CopyTo(memory, size);
		}

		// Can select how many elements to copy
		ECS_INLINE Stream<T> CopyTo(uintptr_t& memory, size_t element_count) const {
			void* current_buffer = (void*)memory;

			memcpy(current_buffer, buffer, sizeof(T) * element_count);
			memory += sizeof(T) * element_count;

			return Stream<T>(current_buffer, element_count);
		}

		ECS_INLINE size_t CopySize() const {
			return sizeof(T) * size;
		}

		// Does not change the size or the pointer
		// It only deallocates if the size is greater than 0
		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) const {
			if (size > 0 && buffer != nullptr) {
				ECSEngine::Deallocate(allocator, buffer, debug_info);
			}
		}

		// Does not change the size or the pointer
		// It only deallocates if the size is greater than 0
		template<typename Allocator>
		ECS_INLINE void Deallocate(Allocator* allocator, DebugInfo debug_info = ECS_DEBUG_INFO) const {
			if (size > 0 && buffer != nullptr) {
				allocator->Deallocate(buffer, debug_info);
			}
		}

		ECS_INLINE bool DeallocateIfBelongs(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) const {
			if (size > 0 && buffer != nullptr) {
				return ECSEngine::DeallocateIfBelongs(allocator, buffer, debug_info);
			}
			return false;
		}

		// Does not update the size
		void DisplaceElements(size_t starting_index, int64_t displacement) {
			// If the displacement is negative, start from the bottom up
			// Else start from the up downwards
			if (displacement < 0) {
				for (int64_t index = (int64_t)starting_index; index < (int64_t)size; index++) {
					buffer[index + displacement] = buffer[index];
				}
			}
			else if (displacement > 0) {
				for (int64_t index = (int64_t)size - 1; index >= (int64_t)starting_index; index--) {
					buffer[index + displacement] = buffer[index];
				}
			}
		}

		ECS_INLINE bool EndsWith(Stream<T> other) const {
			if (other.size <= size) {
				return memcmp(buffer + size - other.size, other.buffer, other.MemoryOf(other.size)) == 0;
			}
			else {
				return false;
			}
		}

		// Returns the previous buffer (the size is also updated)
		ECS_INLINE void* Expand(AllocatorPolymorphic allocator, size_t new_elements, bool deallocate_old = false, DebugInfo debug_info = ECS_DEBUG_INFO) {
			return Resize(allocator, size + new_elements, true, deallocate_old, debug_info);
		}

		// Returns the previous buffer (the size is also updated)
		void* Expand(AllocatorPolymorphic allocator, Stream<T> new_elements, bool deallocate_old = false, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t old_size = size;
			void* old_buffer = buffer;
			if (deallocate_old) {
				Reallocate(allocator, size + new_elements.size, debug_info);
			}
			else {
				Resize(allocator, size + new_elements.size, true, deallocate_old, debug_info);
			}
			new_elements.CopyTo(buffer + old_size);
			return old_buffer;
		}

		// Returns the previous buffer (the size is also updated)
		void* Expand(void* new_buffer, size_t new_size) {
			void* old_buffer = buffer;
			size_t old_size = size;
			InitializeFromBuffer(new_buffer, new_size);
			memcpy(new_buffer, old_buffer, MemoryOf(old_size));
			return old_buffer;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		ECS_INLINE size_t Find(Value value, ProjectionFunctor&& functor) const {
			for (size_t index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		ECS_INLINE size_t Find(const Value* value, ProjectionFunctor&& functor) const {
			for (size_t index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		ECS_INLINE void Insert(size_t index, T value) {
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
		}

		// Set the count to the count of elements that you want to be removed
		ECS_INLINE void Remove(size_t index, size_t count = 1) {
			for (size_t copy_index = index + count; copy_index < size; copy_index++) {
				buffer[copy_index - count] = buffer[copy_index];
			}
			size -= count;
		}

		ECS_INLINE void RemoveSwapBack(size_t index) {
			size--;
			buffer[index] = buffer[size];
		}

		// Returns the previous buffer (the size is also updated)
		void* Resize(AllocatorPolymorphic allocator, size_t new_size, bool copy_old_elements = true, bool deallocate_old = false, DebugInfo debug_info = ECS_DEBUG_INFO) {
			void* old_buffer = buffer;

			void* allocation = nullptr;
			if (new_size > 0) {
				allocation = Allocate(allocator, MemoryOf(new_size), alignof(T), debug_info);
			}
			if (copy_old_elements) {
				size_t copy_size = new_size > size ? size : new_size;
				memcpy(allocation, buffer, sizeof(T) * copy_size);
			}

			if (deallocate_old) {
				Deallocate(allocator, debug_info);
			}

			InitializeFromBuffer(allocation, new_size);
			return old_buffer;
		}

		void Reallocate(AllocatorPolymorphic allocator, size_t new_size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			void* new_allocation = ECSEngine::Reallocate(allocator, buffer, MemoryOf(new_size), alignof(T), debug_info);
			if (new_allocation != buffer) {
				size_t copy_size = new_size > size ? size : new_size;
				memcpy(new_allocation, buffer, MemoryOf(copy_size));
			}
			InitializeFromBuffer(new_allocation, new_size);
		}

		ECS_INLINE void Swap(size_t first, size_t second) {
			T copy = buffer[first];
			buffer[first] = buffer[second];
			buffer[second] = copy;
		}

		void SwapContents() {
			for (size_t index = 0; index < size >> 1; index++) {
				Swap(index, size - index - 1);
			}
		}

		size_t Search(T element) const {
			for (size_t index = 0; index < size; index++) {
				if (element == buffer[index]) {
					return index;
				}
			}

			return -1;
		}

		ECS_INLINE bool StartsWith(Stream<T> other) const {
			if (other.size <= size) {
				return memcmp(buffer, other.buffer, other.MemoryOf(other.size)) == 0;
			}
			else {
				return false;
			}
		}

		ECS_INLINE T& operator [](size_t index) {
			return buffer[index];
		}

		ECS_INLINE const T& operator [](size_t index) const {
			return buffer[index];
		}

		ECS_INLINE const void* GetAllocatedBuffer() const {
			return buffer;
		}

		ECS_INLINE static size_t MemoryOf(size_t number) {
			return sizeof(T) * number;
		}

		void InitializeFromBuffer(void* _buffer, size_t _size) {
			buffer = (T*)_buffer;
			size = _size;
		}

		void InitializeFromBuffer(uintptr_t& _buffer, size_t _size) {
			buffer = (T*)_buffer;
			_buffer += sizeof(T) * _size;
			size = _size;
		}

		void InitializeAndCopy(uintptr_t& buffer, Stream<T> other) {
			InitializeFromBuffer(buffer, other.size);
			Copy(other);
		}

		Stream<T> InitializeAndCopy(AllocatorPolymorphic allocator, Stream<T> other, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Initialize(allocator, other.size, debug_info);
			Copy(other);

			return *this;
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, size_t _size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(_size);
			if (memory_size > 0) {
				void* allocation = allocator->Allocate(memory_size, alignof(T), debug_info);
				buffer = (T*)allocation;
			}
			else {
				buffer = nullptr;
			}
			size = _size;
		}

		void Initialize(AllocatorPolymorphic allocator, size_t _size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(_size);
			if (memory_size > 0) {
				void* allocation = Allocate(allocator, memory_size, alignof(T), debug_info);
				buffer = (T*)allocation;
			}
			else {
				buffer = nullptr;
			}
			size = _size;
		}

		T* buffer;
		size_t size;
	};

	template<typename T>
	struct CapacityStream {
		typedef T T;

		ECS_INLINE CapacityStream() : buffer(nullptr), size(0), capacity(0) {}
		ECS_INLINE CapacityStream(const void* buffer, unsigned int size) : buffer((T*)buffer), size(size), capacity(size) {}
		ECS_INLINE CapacityStream(const void* buffer, unsigned int size, unsigned int capacity) : buffer((T*)buffer), size(size), capacity(capacity) {}
		ECS_INLINE CapacityStream(Stream<T> stream) : buffer(stream.buffer), size(stream.size), capacity(stream.size) {}

		template<typename = std::enable_if_t<std::is_same_v<char, T>>>
		ECS_INLINE CapacityStream(const char* string) : buffer((char*)string) {
			ECS_ASSERT(string != nullptr);
			size = strlen(string);
			capacity = size;
		}

		template<typename = std::enable_if_t<std::is_same_v<wchar_t, T>>>
		ECS_INLINE CapacityStream(const wchar_t* string) : buffer((wchar_t*)string) {
			ECS_ASSERT(string != nullptr);
			size = wcslen(string);
			capacity = size;
		}

		ECS_INLINE CapacityStream(const CapacityStream& other) = default;
		ECS_INLINE CapacityStream<T>& operator = (const CapacityStream<T>& other) = default;

		ECS_INLINE unsigned int Add(T element) {
			buffer[size++] = element;
			return size - 1;
		}

		ECS_INLINE unsigned int Add(const T* element) {
			buffer[size++] = *element;
			return size - 1;
		}

		// Returns the first index
		unsigned int AddStream(Stream<T> other) {
			CopySlice(size, other.buffer, other.size);
			unsigned int previous_size = size;
			size += other.size;
			return previous_size;
		}

		ECS_INLINE bool AddSafe(T element) {
			if (size == capacity) {
				return false;
			}
			Add(element);
			return true;
		}

		ECS_INLINE bool AddSafe(const T* element) {
			if (size == capacity) {
				return false;
			}
			Add(element);
			return true;
		}

		ECS_INLINE unsigned int AddAssert(T element) {
			ECS_ASSERT(size < capacity);
			return Add(element);
		}

		ECS_INLINE unsigned int AddAssert(const T* element) {
			ECS_ASSERT(size < capacity);
			return Add(element);
		}

		ECS_INLINE bool AddStreamSafe(Stream<T> other) {
			if (size + other.size > capacity) {
				return false;
			}
			AddStream(other);
			return true;
		}

		ECS_INLINE unsigned int AddStreamAssert(Stream<T> other) {
			ECS_ASSERT(size + other.size <= capacity);
			return AddStream(other);
		}

		// Returns the index at which it was added. Can select the amount by which it grows with the growth_count
		// (the total growth count is necessary_elements + growth_count)
		unsigned int AddResize(T element, AllocatorPolymorphic allocator, unsigned int growth_count = 0, bool deallocate_previous = true) {
			Expand(allocator, 1, growth_count, deallocate_previous);
			return Add(element);
		}

		// Returns the index at which they were added. Can select the amount by which it grows with the growth_count
		// (the total growth count is necessary_elements + growth_count)
		unsigned int AddResizeStream(Stream<T> elements, AllocatorPolymorphic allocator, unsigned int growth_count = 0, bool deallocate_previous = true) {
			Expand(allocator, elements.size, growth_count, deallocate_previous);
			return AddStream(elements);
		}

		ECS_INLINE void AssertCapacity() const {
			ECS_ASSERT(size <= capacity);
		}

		// Increments the pointer and decrements the size and the capacity
		ECS_INLINE void Advance(int64_t amount = 1) {
			size_t unsigned_amount = (size_t)amount;
			buffer += unsigned_amount;
			size -= unsigned_amount;
			capacity -= unsigned_amount;
		}

		// Sets the size to 0
		ECS_INLINE void Clear() {
			size = 0;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned int count) {
			ECS_ASSERT(count <= capacity);
			memcpy(buffer, memory, sizeof(T) * count);
			size = count;
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<T> other) {
			Copy(other.buffer, other.size);
		}

		ECS_INLINE CapacityStream<T> Copy(AllocatorPolymorphic allocator) const {
			CapacityStream<T> result;
			result.InitializeAndCopy(allocator, ToStream());
			return result;
		}

		// it will not set the size and will not do a check
		ECS_INLINE void CopySlice(unsigned int starting_index, const void* memory, unsigned int count) {
			memcpy(buffer + starting_index, memory, sizeof(T) * count);
		}

		// it will not set the size and will not do a check
		ECS_INLINE void CopySlice(unsigned int starting_index, Stream<T> other) {
			CopySlice(starting_index, other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, sizeof(T) * size);
		}

		// Added return type to be able to be used with StreamCoalescedDeepCopy
		ECS_INLINE CapacityStream<T> CopyTo(uintptr_t& memory) const {
			return CopyTo(memory, size);
		}

		// Can select how many elements to copy
		ECS_INLINE CapacityStream<T> CopyTo(uintptr_t& memory, unsigned int count) const {
			void* current_buffer = (void*)memory;

			memcpy(current_buffer, buffer, sizeof(T) * count);
			memory += sizeof(T) * count;

			return CapacityStream<T>(current_buffer, count, count);
		}
		
		ECS_INLINE size_t CopySize() const {
			return sizeof(T) * size;
		}

		// Does not change the size or the pointer
		// It only deallocates if the size is greater than 0
		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) const {
			if (size > 0 && buffer != nullptr) {
				ECSEngine::Deallocate(allocator, buffer, debug_info);
			}
		}

		// Does not change the size or the pointer
		// It only deallocates if the size is greater than 0
		template<typename Allocator>
		ECS_INLINE void Deallocate(Allocator* allocator, DebugInfo debug_info = ECS_DEBUG_INFO) const {
			if (size > 0) {
				allocator->Deallocate(buffer);
			}
		}

		ECS_INLINE bool DeallocateIfBelongs(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) const {
			if (size > 0 && buffer != nullptr) {
				return ECSEngine::DeallocateIfBelongs(allocator, buffer, debug_info);
			}
			return false;
		}

		// Does not update the size
		void DisplaceElements(unsigned int starting_index, int32_t displacement) {
			// If the displacement is negative, start from the bottom up
			// Else start from the up downwards
			if (displacement < 0) {
				for (int32_t index = (int32_t)starting_index; index < (int32_t)size; index++) {
					buffer[index + displacement] = buffer[index];
				}
			}
			else {
				for (int64_t index = (int32_t)size - 1; index >= (int32_t)starting_index; index--) {
					buffer[index + displacement] = buffer[index];
				}
			}
		}

		ECS_INLINE bool EndsWith(Stream<T> other) const {
			if (other.size <= size) {
				return memcmp(buffer + size - other.size, other.buffer, other.MemoryOf(other.size)) == 0;
			}
			else {
				return false;
			}
		}

		// Returns the old buffer if a resize was performed, else nullptr. Can select the amount by which it grows with the growth_count
		// (the capacity will be necessary_elements + growth_count). 
		void* Expand(
			AllocatorPolymorphic allocator, 
			unsigned int expand_count, 
			unsigned int growth_count = 0, 
			bool deallocate_previous = true, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		) {
			if (size + expand_count > capacity) {
				void* old_buffer = buffer;
				unsigned int needed_elements = expand_count + size + growth_count;
				if (!deallocate_previous) {
					Initialize(allocator, size, needed_elements, debug_info);
					memcpy(buffer, old_buffer, MemoryOf(size));
				}
				else {
					void* new_buffer = ECSEngine::Reallocate(allocator, buffer, needed_elements, alignof(T), debug_info);
					if (new_buffer != buffer) {
						memcpy(new_buffer, buffer, MemoryOf(size));
					}
					InitializeFromBuffer(new_buffer, size, needed_elements);
				}
				return old_buffer;
			}
			return nullptr;
		}

		// Returns the previous buffer
		void* Expand(void* new_buffer, unsigned int new_capacity) {
			void* old_buffer = buffer;
			InitializeFromBuffer(new_buffer, size, new_capacity);
			memcpy(new_buffer, old_buffer, MemoryOf(size));
			return old_buffer;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		ECS_INLINE unsigned int Find(Value value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		ECS_INLINE unsigned int Find(const Value* value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		ECS_INLINE bool IsFull() const {
			return size == capacity;
		}

		ECS_INLINE void Insert(unsigned int index, T value) {
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
			AssertCapacity();
		}

		// The new_elements value will be added to the capacity, not to the current size
		/*void Resize(AllocatorPolymorphic allocator, unsigned int new_elements) {
			void* old_buffer = buffer;

			void* allocation = Allocate(allocator, MemoryOf(capacity + new_elements));
			CopyTo(allocation);
			InitializeFromBuffer(allocation, size, capacity + new_elements);
		
			return old_buffer;
		}*/

		ECS_INLINE void Reset() {
			InitializeFromBuffer(nullptr, 0, 0);
		}

		// Set the count to the number of elements that you want to be removed
		ECS_INLINE void Remove(unsigned int index, unsigned int count = 1) {
			for (unsigned int copy_index = index + count; copy_index < size; copy_index++) {
				buffer[copy_index - count] = buffer[copy_index];
			}
			size -= count;
		}

		ECS_INLINE void RemoveSwapBack(unsigned int index) {
			size--;
			buffer[index] = buffer[size];
		}

		ECS_INLINE void Swap(unsigned int first, unsigned int second) {
			T copy = buffer[first];
			buffer[first] = buffer[second];
			buffer[second] = copy;
		}

		ECS_INLINE void SwapContents() {
			for (unsigned int index = 0; index < size >> 1; index++) {
				Swap(index, size - index - 1);
			}
		}

		unsigned int Search(T element) const {
			for (unsigned int index = 0; index < size; index++) {
				if (buffer[index] == element) {
					return index;
				}
			}

			return -1;
		}

		ECS_INLINE bool StartsWith(Stream<T> other) const {
			if (other.size <= size) {
				return memcmp(buffer, other.buffer, other.MemoryOf(other.size)) == 0;
			}
			else {
				return false;
			}
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return buffer[index];
		}

		ECS_INLINE const T& operator [](unsigned int index) const {
			return buffer[index];
		}

		ECS_INLINE const void* GetAllocatedBuffer() const {
			return buffer;
		}

		ECS_INLINE static size_t MemoryOf(unsigned int number) {
			return sizeof(T) * number;
		}

		ECS_INLINE Stream<T> ToStream() const {
			return { buffer, size };
		}

		void InitializeFromBuffer(void* _buffer, unsigned int _size, unsigned int _capacity) {
			buffer = (T*)_buffer;
			size = _size;
			capacity = _capacity;
		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _size, unsigned int _capacity) {
			buffer = (T*)_buffer;
			_buffer += sizeof(T) * _capacity;
			size = _size;
			capacity = _capacity;
		}

		// Helpful for temp memory copy and initialization
		CapacityStream<T> InitializeAndCopy(uintptr_t& buffer, Stream<T> other) {
			InitializeFromBuffer(buffer, other.size, other.size);
			Copy(other);

			return *this;
		}

		// It will make a copy with the capacity the same as the stream's size
		CapacityStream<T> InitializeAndCopy(AllocatorPolymorphic allocator, Stream<T> other, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Initialize(allocator, other.size, other.size, debug_info);
			Copy(other);

			return *this;
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _size, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(_capacity);
			if (memory_size > 0) {
				void* allocation = allocator->Allocate(memory_size, alignof(T), debug_info);
				InitializeFromBuffer(allocation, _size, _capacity);
			}
			else {
				buffer = nullptr;
				size = _size;
				capacity = _capacity;
			}
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _size, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(_capacity);
			if (memory_size > 0) {
				void* allocation = Allocate(allocator, memory_size, alignof(T), debug_info);
				InitializeFromBuffer(allocation, _size, _capacity);
			}
			else {
				buffer = nullptr;
				size = _size;
				capacity = _capacity;
			}
		}

		T* buffer;
		unsigned int size;
		unsigned int capacity;
	};

	template<typename T>
	struct ResizableStream {
		typedef T T;

		ECS_INLINE ResizableStream() : buffer(nullptr), allocator({nullptr}), capacity(0), size(0) {}
		ResizableStream(AllocatorPolymorphic _allocator, unsigned int _capacity) : allocator(_allocator), capacity(_capacity), size(0) {
			if (_capacity != 0) {
				ResizeNoCopy(_capacity);
			}
			else {
				buffer = nullptr;
			}
		}

		ResizableStream(const ResizableStream& other) = default;
		ResizableStream<T>& operator = (const ResizableStream<T>& other) = default;

		ECS_INLINE operator Stream<T>() const {
			return { buffer, size };
		}

		ECS_INLINE unsigned int Add(T element) {
			if (size == capacity) {
				Resize((unsigned int)(ECS_RESIZABLE_STREAM_FACTOR * capacity + 1));
			}
			buffer[size++] = element;
			return size - 1;
		}

		ECS_INLINE unsigned int Add(const T* element) {
			if (size == capacity) {
				Resize((unsigned int)(ECS_RESIZABLE_STREAM_FACTOR * capacity + 1));
			}
			buffer[size++] = *element;
			return size - 1;
		}

		// Returns the first index
		unsigned int AddStream(Stream<T> other) {
			ReserveNewElements(other.size);
			CopySlice(size, other);
			unsigned int previous_size = size;
			size += other.size;
			return previous_size;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned int count) {
			ResizeNoCopy(count);
			memcpy(buffer, memory, sizeof(T) * count);
			size = count;
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<T> other) {
			Copy(other.buffer, other.size);
		}

		ECS_INLINE ResizableStream<T> Copy(AllocatorPolymorphic allocator) const {
			ResizableStream<T> result;
			result.InitializeAndCopy(allocator, ToStream());
			return result;
		}

		// it will not set the size and will not do a check
		ECS_INLINE void CopySlice(unsigned int starting_index, const void* memory, unsigned int count) {
			memcpy(buffer + starting_index, memory, sizeof(T) * count);
		}

		// it will not set the size and will not do a check
		ECS_INLINE void CopySlice(unsigned int starting_index, Stream<T> other) {
			CopySlice(starting_index, other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, sizeof(T) * size);
		}

		// If it is desirable to be used with StreamCoalescedDeepCopy, the problem is that
		// it cannot use coallesced allocation. Wait for a use case to decide
		ECS_INLINE void CopyTo(uintptr_t& memory) const {
			CopyTo(memory, size);
		}

		// Can choose how many elements to copy
		ECS_INLINE void CopyTo(uintptr_t& memory, unsigned int count) const {
			memcpy((void*)memory, buffer, sizeof(T) * count);
			memory += sizeof(T) * count;
		}

		ECS_INLINE size_t CopySize() const {
			return sizeof(T) * size;
		}

		// Does not update the size
		// Make sure you have enough space before doing this!
		void DisplaceElements(unsigned int starting_index, int32_t displacement) {
			// If the displacement is negative, start from the bottom up
			// Else start from the up downwards
			if (displacement < 0) {
				for (int32_t index = (int32_t)starting_index; index < (int32_t)size; index++) {
					buffer[index + displacement] = buffer[index];
				}
			}
			else {
				for (int64_t index = (int32_t)size - 1; index >= (int32_t)starting_index; index--) {
					buffer[index + displacement] = buffer[index];
				}
			}
		}

		void FreeBuffer(DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (buffer != nullptr) {
				DeallocateEx(allocator, buffer, debug_info);
				buffer = nullptr;
				size = 0;
				capacity = 0;
			}
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		ECS_INLINE unsigned int Find(Value value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		ECS_INLINE unsigned int Find(const Value* value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		ECS_INLINE void Insert(unsigned int index, T value) {
			ReserveNewElement();
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
		}

		ECS_INLINE void Reset() {
			size = 0;	
		}

		// Set the count to the number of elements that you want to be removed
		ECS_INLINE void Remove(unsigned int index, unsigned int count = 1) {
			for (unsigned int copy_index = index + count; copy_index < size; copy_index++) {
				buffer[copy_index - count] = buffer[copy_index];
			}
			size -= count;
		}

		ECS_INLINE void RemoveSwapBack(unsigned int index) {
			size--;
			buffer[index] = buffer[size];
		}

		// makes sure there is enough space for extra count elements
		ECS_INLINE void ReserveNewElements(unsigned int count) {
			if (size + count > capacity) {
				unsigned int new_capacity = ECS_RESIZABLE_STREAM_FACTOR * capacity + 1;
				unsigned int resize_count = new_capacity < capacity + count ? capacity + count : new_capacity;
				Resize(resize_count);
			}
		}

		unsigned int ReserveNewElement() {
			unsigned int index = size;
			ReserveNewElements(1);
			return index;
		}

		private:
			template<bool copy_old_data>
			void ResizeImpl(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
				void* new_buffer = nullptr;

				if (new_capacity != 0) {
					if (buffer != nullptr && size > 0) {
						unsigned int copy_size = size < new_capacity ? size : new_capacity;
						new_buffer = ECSEngine::ReallocateEx(allocator, buffer, MemoryOf(new_capacity), debug_info);
						ECS_ASSERT(new_buffer != nullptr);
						if constexpr (copy_old_data) {
							if (new_buffer != buffer) {
								memcpy(new_buffer, buffer, MemoryOf(copy_size));
							}
						}
					}
					else {
						new_buffer = ECSEngine::AllocateEx(allocator, MemoryOf(new_capacity), debug_info);
					}
				}
				else {
					FreeBuffer(debug_info);
				}

				buffer = (T*)new_buffer;
				capacity = new_capacity;
			}

		public:

		void Resize(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ResizeImpl<true>(new_capacity, debug_info);
		}

		void ResizeNoCopy(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ResizeImpl<false>(new_capacity);
		}

		ECS_INLINE void Swap(unsigned int first, unsigned int second) {
			T copy = buffer[first];
			buffer[first] = buffer[second];
			buffer[second] = copy;
		}

		ECS_INLINE void SwapContents() {
			for (unsigned int index = 0; index < size >> 1; index++) {
				Swap(index, size - index - 1);
			}
		}

		unsigned int Search(T element) const {
			for (unsigned int index = 0; index < size; index++) {
				if (buffer[index] == element) {
					return index;
				}
			}

			return -1;
		}
		
		// It will leave another additional_elements over the current size
		// Returns true if a resizing was performed, else false
		bool Trim(unsigned int additional_elements, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (additional_elements < capacity - size) {
				unsigned int elements_to_copy = size + additional_elements;

				void* allocation = nullptr;
				if (buffer != nullptr && elements_to_copy > 0) {
					allocation = ReallocateEx(allocator, buffer, MemoryOf(elements_to_copy), debug_info);
					ECS_ASSERT(allocation != nullptr);

					if (allocation != buffer) {
						memcpy(allocation, buffer, sizeof(T) * elements_to_copy);
					}
				}

				buffer = (T*)allocation;
				capacity = elements_to_copy;
				return true;
			}
			return false;
		}

		ECS_INLINE void Trim() {
			Trim(0);
		}

		ECS_INLINE Stream<T> ToStream() const {
			return { buffer, size };
		}
		
		ECS_INLINE T& operator [](unsigned int index) {
			return buffer[index];
		}

		ECS_INLINE const T& operator [](unsigned int index) const {
			return buffer[index];
		}

		ECS_INLINE static size_t MemoryOf(unsigned int number) {
			return sizeof(T) * number;
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			allocator = _allocator;
			if (_capacity > 0) {
				ResizeNoCopy(_capacity, debug_info);
			}
			else {
				buffer = nullptr;
				capacity = 0;
			}
			size = 0;
		}

		ResizableStream<T> InitializeAndCopy(AllocatorPolymorphic _allocator, Stream<T> stream, DebugInfo debug_info = ECS_DEBUG_INFO) {
			allocator = _allocator;
			if (stream.size > 0) {
				ResizeNoCopy(stream.size, debug_info);
				Copy(stream);
			}
			else {
				buffer = nullptr;
				capacity = 0;
				size = 0;
			}

			return *this;
		}

		T* buffer;
		unsigned int size;
		unsigned int capacity;
		AllocatorPolymorphic allocator;
	};

	template <>
	struct ECSENGINE_API Stream<void>
	{
		typedef void T;

		ECS_INLINE Stream() : buffer(nullptr), size(0) {}
		ECS_INLINE Stream(const void* _buffer, size_t _size) : buffer((void*)_buffer), size(_size) {}

		template<typename T>
		Stream(const T* type) : buffer((void*)type), size(sizeof(T)) {
			static_assert(!std::is_same_v<T, void>, "Stream<void> constructor given a void* pointer to construct from!");
		}

		template<typename T>
		Stream(Stream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}
		
		template<typename T, typename = std::enable_if_t<!std::is_same_v<void, T>>>
		Stream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}

		Stream(const Stream& other) = default;
		Stream<void>& operator = (const Stream<void>& other) = default;

		ECS_INLINE void Add(Stream<void> other) {
			memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
			size += other.size;
		}

		// If using the buffer for untyped data
		ECS_INLINE void AddElement(const void* data, size_t byte_size) {
			SetElement(size, data, byte_size);
			size++;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, size_t memory_size) {
			memcpy(buffer, memory, memory_size);
			size = memory_size;
		}

		ECS_INLINE Stream<void> Copy(AllocatorPolymorphic allocator) const {
			Stream<void> result;
			result.Initialize(allocator, size);
			result.Copy(buffer, size);
			return result;
		}

		// it will not set the size
		ECS_INLINE void CopySlice(size_t offset, const void* memory, size_t memory_size) {
			memcpy((void*)((size_t)buffer + offset), memory, memory_size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		// It considers this stream to contain untyped data with the size being the count, not
		// the byte size such that it can copy to other buffers
		ECS_INLINE void CopyTo(void* memory, size_t byte_size) const {
			memcpy(memory, buffer, size * byte_size);
		}

		ECS_INLINE bool Equals(Stream<void> other) const {
			return size == other.size && memcmp(buffer, other.buffer, size) == 0;
		}

		// If the size is used as an element count instead of byte size
		ECS_INLINE bool Equals(Stream<void> other, size_t element_size) const {
			return size == other.size && memcmp(buffer, other.buffer, size * element_size) == 0;
		}

		ECS_INLINE void* Get(size_t index, size_t byte_size) {
			return (void*)((uintptr_t)buffer + index * byte_size);
		}

		ECS_INLINE const void* Get(size_t index, size_t byte_size) const {
			return (const void*)((uintptr_t)buffer + index * byte_size);
		}

		// If used polymorphically
		ECS_INLINE void SetElement(size_t index, const void* data, size_t byte_size) {
			CopySlice(index * byte_size, data, byte_size);
		}

		// If used polymorphically
		ECS_INLINE void RemoveSwapBack(size_t index, size_t byte_size) {
			size--;
			CopySlice(index * byte_size, (void*)((uintptr_t)buffer + size * byte_size), byte_size);
		}

		void Swap(size_t first, size_t second, size_t byte_size) {
			void* temporary_storage = ECS_STACK_ALLOC(byte_size);
			// Copy the first one into temporary storage
			memcpy(temporary_storage, Get(first, byte_size), byte_size);
			SetElement(first, Get(second, byte_size), byte_size);
			SetElement(second, temporary_storage, byte_size);
		}

		// The size is divided by the sizeof T
		template<typename T>
		ECS_INLINE Stream<T> As() const {
			return { buffer, size / sizeof(T) };
		}

		// Returns this as a Stream<T>, without modifying the size
		template<typename T>
		ECS_INLINE Stream<T> AsIs() const {
			return { buffer, size };
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, size_t _size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			buffer = allocator->Allocate(_size, debug_info);
			size = _size;
		}

		void Initialize(AllocatorPolymorphic allocator, size_t _size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			buffer = Allocate(allocator, size, alignof(void*), debug_info);
			size = _size;
		}

		void InitializeFromBuffer(void* _buffer, size_t _size) {
			buffer = _buffer;
			size = _size;
		}

		void InitializeFromBuffer(uintptr_t& _buffer, size_t _size) {
			buffer = (void*)_buffer;
			size = _size;
			_buffer += size;
		}

		void* buffer;
		size_t size;
	};

	template<>
	struct ECSENGINE_API CapacityStream<void>
	{
		typedef void T;

		ECS_INLINE CapacityStream() : buffer(nullptr), size(0), capacity(0) {}
		ECS_INLINE CapacityStream(const void* _buffer, unsigned int _size, unsigned int _capacity) : buffer((void*)_buffer), size(_size), capacity(_capacity) {}

		template<typename T>
		CapacityStream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)), capacity(other.capacity * sizeof(T)) {}

		operator Stream<void>() const {
			return { buffer, size };
		}

		CapacityStream(const CapacityStream& other) = default;
		CapacityStream<void>& operator = (const CapacityStream<void>& other) = default;

		ECS_INLINE void Add(Stream<void> other) {
			ECS_ASSERT(size + other.size < capacity);
			memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
			size += other.size;
		}

		ECS_INLINE void Add(CapacityStream<void> other) {
			ECS_ASSERT(size + other.size < capacity);
			memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
			size += other.size;
		}

		// If using the buffer for untyped data
		ECS_INLINE void AddElement(const void* data, unsigned int byte_size) {
			ECS_ASSERT(size < capacity);
			SetElement(size, data, byte_size);
			size++;
		}

		ECS_INLINE void AssertCapacity() const {
			ECS_ASSERT(size <= capacity);
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned int memory_size) {
			ECS_ASSERT(memory_size <= capacity);
			memcpy(buffer, memory, memory_size);
			size = memory_size;
		}
		
		ECS_INLINE void Copy(Stream<void> other, unsigned int element_byte_size) {
			ECS_ASSERT(other.size <= capacity);
			memcpy(buffer, other.buffer, other.size * element_byte_size);
			size = other.size;
		}

		ECS_INLINE CapacityStream<void> Copy(AllocatorPolymorphic allocator) const {
			CapacityStream<void> result;
			result.Initialize(allocator, size);
			result.Copy(buffer, size);
			return result;
		}

		// it will not set the size
		ECS_INLINE void CopySlice(unsigned int offset, const void* memory, unsigned int memory_size) {
			memcpy((void*)((uintptr_t)buffer + offset), memory, memory_size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		ECS_INLINE bool Equals(Stream<void> other) const {
			return size == other.size && memcmp(buffer, other.buffer, size) == 0;
		}

		// If the size is used as an element count instead of byte size
		ECS_INLINE bool Equals(Stream<void> other, size_t element_size) const {
			return size == other.size && memcmp(buffer, other.buffer, (size_t)size * element_size) == 0;
		}

		ECS_INLINE void* Get(unsigned int index, unsigned int byte_size) {
			return (void*)((uintptr_t)buffer + index * byte_size);
		}

		ECS_INLINE const void* Get(unsigned int index, unsigned int byte_size) const {
			return (const void*)((uintptr_t)buffer + index * byte_size);
		}

		ECS_INLINE void Remove(unsigned int index, unsigned int byte_size) {
			auto offset_pointer = [byte_size](void* buffer, unsigned int subindex) {
				return (void*)((uintptr_t)buffer + subindex * byte_size);
			};

			for (unsigned int subindex = index; subindex < size - 1; subindex++) {
				memcpy(
					offset_pointer(buffer, subindex),
					offset_pointer(buffer, (subindex + 1)),
					byte_size
				);
			}
		}

		// If used polymorphically
		ECS_INLINE void RemoveSwapBack(unsigned int index, unsigned int byte_size) {
			size--;
			CopySlice(index * byte_size, (void*)((uintptr_t)buffer + size * byte_size), byte_size);
		}

		// If used untyped
		ECS_INLINE void SetElement(unsigned int index, const void* data, unsigned int byte_size) {
			CopySlice(index * byte_size, data, byte_size);
		}

		void Swap(unsigned int first, unsigned int second, unsigned int byte_size) {
			void* temporary_storage = ECS_STACK_ALLOC(byte_size);
			// Copy the first one into temporary storage
			memcpy(temporary_storage, Get(first, byte_size), byte_size);
			SetElement(first, Get(second, byte_size), byte_size);
			SetElement(second, temporary_storage, byte_size);
		}

		// Divides the size and the capacity by the sizeof(T)
		template<typename T>
		ECS_INLINE CapacityStream<T> As() const {
			return { buffer, size / sizeof(T), capacity / sizeof(T) };
		}

		// Does not divide the size and capacity by the sizeof(T)
		template<typename T>
		ECS_INLINE CapacityStream<T> AsIs() const {
			return { buffer, size, capacity };
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			buffer = allocator->Allocate(_capacity, 8, debug_info);
			size = 0;
			capacity = _capacity;
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			buffer = Allocate(allocator, _capacity, 8, debug_info);
			size = 0;
			capacity = _capacity;
		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _size, unsigned int _capacity) {
			buffer = (void*)_buffer;
			size = _size;
			capacity = _capacity;

			_buffer += _capacity;
		}

		void* buffer;
		unsigned int size;
		unsigned int capacity;
	};

	template<>
	struct ResizableStream<void> {
		typedef void T;

		ECS_INLINE ResizableStream() : buffer(nullptr), allocator({ nullptr }), capacity(0), size(0) {}
		ResizableStream(AllocatorPolymorphic _allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) : allocator(_allocator), 
			capacity(_capacity), size(0) {
			if (_capacity != 0) {
				buffer = Allocate(allocator, _capacity, alignof(void*), debug_info);
			}
			else {
				buffer = nullptr;
			}
		}

		ResizableStream(const ResizableStream& other) = default;
		ResizableStream<void>& operator = (const ResizableStream<void>& other) = default;

		unsigned int Add(Stream<void> data) {
			if (size + data.size >= capacity) {
				Resize((unsigned int)((float)capacity * ECS_RESIZABLE_STREAM_FACTOR + 2));
			}
			memcpy((void*)((uintptr_t)buffer + size), data.buffer, data.size);
			unsigned int offset = size;
			size += data.size;
			return offset;
		}

		unsigned int Add(Stream<void> data, unsigned int element_byte_size) {
			if (size + data.size >= capacity) {
				Resize((unsigned int)((float)capacity * ECS_RESIZABLE_STREAM_FACTOR + 2), element_byte_size);
			}
			memcpy((void*)((uintptr_t)buffer + size * element_byte_size), data.buffer, data.size * element_byte_size);
			unsigned int offset = size;
			size += data.size;
			return offset;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned int count) {
			if (count != capacity) {
				ResizeNoCopy(count);
			}
			memcpy(buffer, memory, count);
			size = count;
		}

		ECS_INLINE void Copy(const void* memory, unsigned int count, unsigned int element_byte_size) {
			if (count != capacity) {
				ResizeNoCopy(count, element_byte_size);
			}
			memcpy(buffer, memory, (size_t)count * (size_t)element_byte_size);
			size = count;
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<void> other) {
			Copy(other.buffer, other.size);
		}

		ECS_INLINE void Copy(Stream<void> other, unsigned int element_byte_size) {
			Copy(other.buffer, other.size, element_byte_size);
		}

		ECS_INLINE ResizableStream<void> Copy(AllocatorPolymorphic allocator) const {
			ResizableStream<void> result;
			result.Initialize(allocator, size);
			result.Copy(buffer, size);
			return result;
		}

		// it will not set the size
		ECS_INLINE void CopySlice(unsigned int offset, const void* memory, unsigned int memory_size) {
			memcpy((void*)((uintptr_t)buffer + offset), memory, memory_size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		ECS_INLINE void CopyTo(uintptr_t& memory) const {
			memcpy((void*)memory, buffer, size);
			memory += size;
		}

		ECS_INLINE bool Equals(Stream<void> other) const {
			return size == other.size && memcmp(buffer, other.buffer, size) == 0;
		}

		// If the size is used as an element count instead of byte size
		ECS_INLINE bool Equals(Stream<void> other, unsigned int element_size) const {
			return size == other.size && memcmp(buffer, other.buffer, size * element_size) == 0;
		}

		void FreeBuffer(DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (buffer != nullptr) {
				DeallocateEx(allocator, buffer, debug_info);
				buffer = nullptr;
				size = 0;
				capacity = 0;
			}
		}

		ECS_INLINE void* Get(unsigned int index, unsigned int byte_size) {
			return (void*)((uintptr_t)buffer + index * byte_size);
		}

		ECS_INLINE const void* Get(unsigned int index, unsigned int byte_size) const {
			return (const void*)((uintptr_t)buffer + index * byte_size);
		}

		ECS_INLINE void Reset() {
			size = 0;
		}

		// makes sure there is enough space for extra count elements
		void Reserve(unsigned int byte_count) {
			unsigned int new_size = size + byte_count;
			if (new_size > capacity) {
				unsigned int resize_capacity = (unsigned int)(ECS_RESIZABLE_STREAM_FACTOR * capacity + 3);
				if (resize_capacity < new_size) {
					resize_capacity = new_size * ECS_RESIZABLE_STREAM_FACTOR;
				}
				Resize(resize_capacity);
			}
		}

		ECS_INLINE void Resize(unsigned int new_capacity) {
			Resize(new_capacity, 1);
		}

		void Resize(unsigned int new_capacity, unsigned int element_byte_size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (new_capacity > 0) {
				void* new_buffer = ReallocateEx(allocator, buffer, new_capacity * element_byte_size, debug_info);
				ECS_ASSERT(new_buffer != nullptr);

				unsigned int size_to_copy = size < new_capacity ? size : new_capacity;
				if (new_buffer != buffer) {
					memcpy(new_buffer, buffer, size_to_copy * element_byte_size);
					buffer = new_buffer;
				}
			}
			else {
				if (buffer != nullptr) {
					DeallocateEx(allocator, buffer, debug_info);
					buffer = nullptr;
				}
			}
			capacity = new_capacity;
		}

		ECS_INLINE void ResizeNoCopy(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ResizeNoCopy(new_capacity, 1, debug_info);
		}

		void ResizeNoCopy(unsigned int new_capacity, unsigned int element_byte_size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			void* new_buffer = nullptr;
			if (new_capacity > 0 && size > 0) {
				new_buffer = ReallocateEx(allocator, buffer, new_capacity * element_byte_size, debug_info);
				ECS_ASSERT(new_buffer != nullptr);
			}
			else if (size > 0) {
				DeallocateEx(allocator, buffer, debug_info);
			}
			else if (new_capacity > 0) {
				new_buffer = AllocateEx(allocator, new_capacity * element_byte_size, debug_info);
				ECS_ASSERT(new_buffer != nullptr);
			}

			buffer = new_buffer;
			capacity = new_capacity;
		}

		// If used polymorphically
		ECS_INLINE void SetElement(unsigned int index, const void* data, unsigned int byte_size) {
			CopySlice(index * byte_size, data, byte_size);
		}

		void Swap(unsigned int first, unsigned int second, unsigned int byte_size) {
			void* temporary_storage = ECS_STACK_ALLOC(byte_size);
			// Copy the first one into temporary storage
			memcpy(temporary_storage, Get(first, byte_size), byte_size);
			SetElement(first, Get(second, byte_size), byte_size);
			SetElement(second, temporary_storage, byte_size);
		}

		ECS_INLINE Stream<void> ToStream() const {
			return { buffer, size };
		}

		ECS_INLINE CapacityStream<void> ToCapacityStream() const {
			return { buffer, size, capacity };
		}

		// Divides the size and the capacity by the sizeof(T)
		template<typename T>
		ECS_INLINE ResizableStream<T> As() const {
			return { buffer, size / sizeof(T), capacity / sizeof(T), allocator };
		}

		// Does not divide the size and capacity by the sizeof(T)
		template<typename T>
		ECS_INLINE ResizableStream<T> AsIs() const {
			return { buffer, size, capacity, allocator };
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			allocator = _allocator;
			if (_capacity > 0) {
				ResizeNoCopy(_capacity, debug_info);
			}
			else {
				buffer = nullptr;
				capacity = 0;
			}
			size = 0;
		}

		void* buffer;
		unsigned int size;
		unsigned int capacity;
		AllocatorPolymorphic allocator;
	};

	// The template parameter of the stream must have as functions
	// Type Copy(AllocatorPolymorphic allocator) const;
	template<typename Stream>
	Stream StreamDeepCopy(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Stream result;

		result.Initialize(allocator, input.size, debug_info);
		for (size_t index = 0; index < (size_t)input.size; index++) {
			result[index] = input[index].Copy(allocator);
		}

		return result;
	}

	// The template parameter of the stream must have as functions
	// Type CopyTo(uintptr_t& ptr) const;
	// size_t CopySize() const;
	template<typename Stream>
	Stream StreamDeepCopyTo(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Stream result;

		result.Initialize(allocator, input.size, debug_info);
		for (size_t index = 0; index < (size_t)input.size; index++) {
			size_t copy_size = input[index].CopySize();
			void* allocation = AllocateEx(allocator, copy_size, debug_info);
			uintptr_t ptr = (uintptr_t)allocation;
			result[index] = input[index].CopyTo(ptr);
		}

		return result;
	}

	// The template parameter of the stream must have as functions
	// Type Copy(AllocatorPolymorphic allocator) const;
	template<typename Stream>
	void StreamInPlaceDeepCopy(Stream input, AllocatorPolymorphic allocator) {
		for (size_t index = 0; index < (size_t)input.size; index++) {
			input[index] = input[index].Copy(allocator);
		}
	}

	// The template parameter of the stream must have as functions
	// Type CopyTo(uintptr_t& ptr) const;
	// size_t CopySize() const;
	template<typename Stream>
	Stream StreamInPlaceDeepCopyTo(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info) {
		for (size_t index = 0; index < (size_t)input.size; index++) {
			size_t copy_size = input[index].CopySize();
			void* allocation = AllocateEx(allocator, copy_size, debug_info);
			uintptr_t ptr = (uintptr_t)allocation;
			input[index] = input[index].CopyTo(ptr);
		}
	}

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	// void Copy(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename Stream>
	Stream StreamCoalescedDeepCopy(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Stream new_stream;
		
		ECS_STACK_CAPACITY_STREAM(size_t, copy_sizes, 1024);
		size_t total_size = input.MemoryOf(input.size);

		if (input.size < 1024) {
			for (size_t index = 0; index < (size_t)input.size; index++) {
				size_t copy_size = input[index].CopySize();
				total_size += copy_size;

				copy_sizes[index] = copy_size;
			}
		}
		else {
			for (size_t index = 0; index < (size_t)input.size; index++) {
				total_size += input[index].CopySize();
			}
		}

		void* allocation = AllocateEx(allocator, total_size, debug_info);
		uintptr_t ptr = (uintptr_t)allocation;
		new_stream.InitializeAndCopy(ptr, input);

		if (input.size < 1024) {
			for (size_t index = 0; index < (size_t)input.size; index++) {
				if (copy_sizes[index] > 0) {
					new_stream[index] = input[index].CopyTo(ptr);
				}
			}
		}
		else {
			for (size_t index = 0; index < (size_t)input.size; index++) {
				size_t copy_size = input[index].CopySize();
				if (copy_size > 0) {
					new_stream[index] = input[index].CopyTo(ptr);
				}
			}
		}

		return new_stream;
	}

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	// T CopyTo(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename Stream>
	Stream StreamCoalescedDeepCopy(Stream input, uintptr_t& ptr) {
		Stream new_stream;
		new_stream.InitializeAndCopy(ptr, input);

		for (size_t index = 0; index < (size_t)input.size; index++) {
			size_t copy_size = input[index].CopySize();
			if (copy_size > 0) {
				new_stream[index] = input[index].CopyTo(ptr);
			}
		}

		return new_stream;
	}

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	template<typename Stream>
	size_t StreamCoallescedDeepCopySize(Stream input) {
		size_t total_size = input.MemoryOf(input.size);

		for (size_t index = 0; index < (size_t)input.size; index++) {
			total_size += input[index].CopySize();
		}

		return total_size;
	}

	// It will write the elements in place
	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	// T CopyTo(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename Stream>
	void StreamCoallescedInplaceDeepCopy(Stream input, uintptr_t& ptr) {
		for (size_t index = 0; index < (size_t)input.size; index++) {
			size_t copy_size = input[index].CopySize();
			if (copy_size > 0) {
				input[index] = input[index].CopyTo(ptr);
			}
		}
	}

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	template<typename Stream>
	size_t StreamCoallescedInplaceDeepCopySize(Stream input) {
		size_t total_size = 0;

		for (size_t index = 0; index < (size_t)input.size; index++) {
			total_size += input[index].CopySize();
		}

		return total_size;
	}

	// It will write the elements in place
	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	// T CopyTo(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename Stream>
	void StreamCoallescedInplaceDeepCopy(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		size_t allocation_size = StreamCoallescedInplaceDeepCopySize(input);
		void* allocation = AllocateEx(allocator, allocation_size, debug_info);
		uintptr_t ptr = (uintptr_t)allocation;
		return StreamCoallescedInplaceDeepCopy(input, ptr);
	}

	ECS_INLINE bool operator == (Stream<char> left, Stream<char> right) {
		return left.size == right.size && memcmp(left.buffer, right.buffer, left.size * sizeof(char)) == 0;
	}

	ECS_INLINE bool operator == (Stream<wchar_t> left, Stream<wchar_t> right) {
		return left.size == right.size && memcmp(left.buffer, right.buffer, left.size * sizeof(wchar_t)) == 0;
	}

	ECS_INLINE bool operator != (Stream<char> left, Stream<char> right) {
		return !(left == right);
	}

	ECS_INLINE bool operator != (Stream<wchar_t> left, Stream<wchar_t> right) {
		return !(left == right);
	}

	// The stream element must have the function void ToString(CapacityStream<char>& string) const;
	template<typename StreamType>
	ECS_INLINE void StreamToString(StreamType input, CapacityStream<char>& string, Stream<char> separator = ", ") {
		for (size_t index = 0; index < input.size; index++) {
			input[index].ToString(string);
			if (index < input.size - 1) {
				string.AddStreamSafe(separator);
			}
		}
	}

	template<typename T>
	struct AdditionStream {
		ECS_INLINE AdditionStream() {}

		ECS_INLINE unsigned int Add(T element) {
			if (is_capacity) {
				return capacity_stream.AddAssert(element);
			}
			else {
				return resizable_stream.Add(element);
			}
		}

		ECS_INLINE unsigned int Size() const {
			// Both of them have the same layout, we can return the size directly
			return capacity_stream.size;
		}
		
		// This sets the size directly, it does not perform a resize for the resizable stream
		ECS_INLINE void SetSize(unsigned int value) {
			// We can alias the 2 streams since they have the same layout
			capacity_stream.size = value;
		}

		ECS_INLINE bool IsCapacity() const {
			return is_capacity;
		}

		ECS_INLINE Stream<T> ToStream() const {
			if (is_capacity) {
				return capacity_stream;
			}
			else {
				return resizable_stream.ToStream();
			}
		}

		ECS_INLINE bool IsInitialized() const {
			return is_capacity ? capacity_stream.capacity > 0 : resizable_stream.allocator.allocator != nullptr;
		}
		
		ECS_INLINE T& operator[](unsigned int index) {
			// Both have the same layout, we can index into the buffer
			return capacity_stream.buffer[index];
		}

		ECS_INLINE const T& operator[](unsigned int index) const {
			// Both have the same layout, we can index into the buffer
			return capacity_stream.buffer[index];
		}

		bool is_capacity;
		union {
			CapacityStream<T> capacity_stream;
			ResizableStream<T> resizable_stream;
		};
	};

}
