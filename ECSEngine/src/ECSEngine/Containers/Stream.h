#pragma once
#include "../Core.h"
#include "../Utilities/Assert.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include <stdint.h>
#include <string.h>
#include <type_traits>
#include "../Utilities/Iterator.h"

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

#define ECS_STACK_ADDITION_STREAM(type, name, capacity) ECS_STACK_CAPACITY_STREAM(type, name, capacity); AdditionStream<type> name##_addition = &name

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
			size_t previous_size = size;
			size += other.size;
			return previous_size;
		}

		// It assumes that the stream has enough space for the iterator to be added. If you are not sure if it fits,
		// Use a capacity stream or a resizable stream.
		unsigned int AddIterator(IteratorInterface<T>* iterator) {
			struct AddInterface {
				AddInterface(Stream<T>& _stream) : stream(_stream) {}

				// This type doesn't have an assert call
				ECS_INLINE void AddAssert(const T& value) {
					stream.Add(value);
				}

				ECS_INLINE void Add(const T& value) {
					stream.Add(value);
				}

				// Nothing to be done here
				ECS_INLINE void Reserve(size_t size) {}

				ECS_INLINE void AddStream(Stream<T> elements) {
					stream.AddStream(elements);
				}

				Stream<T>& stream;
			};
			
			size_t previous_size = size;
			AddInterface add_interface{ *this };
			AddIteratorToContainer(iterator, add_interface);
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
			size_t old_size = size;
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
		// And returns the stream with those values. It does not modify this one
		Stream<T> AdvanceReturn(int64_t amount = 1) const {
			Stream<T> copy = *this;
			copy.Advance(amount);
			return copy;
		}

		ECS_INLINE Stream<T> Copy(AllocatorPolymorphic allocator) const {
			Stream<T> result;
			result.InitializeAndCopy(allocator, *this);
			return result;
		}

		// It will set the size
		ECS_INLINE void CopyOther(const void* memory, size_t count) {
			memcpy(buffer, memory, count * sizeof(T));
			size = count;
		}

		// It will set the size
		ECS_INLINE void CopyOther(Stream<T> other) {
			CopyOther(other.buffer, other.size);
		}

		// It will not set the size
		ECS_INLINE void CopySlice(size_t starting_index, const void* memory, size_t count) {
			memcpy(buffer + starting_index, memory, sizeof(T) * count);
		}

		// It will not set the size
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
		Stream<T> CopyTo(uintptr_t& memory, size_t element_count) const {
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

		ECS_INLINE void DeallocateWithReset(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Deallocate(allocator, debug_info);
			buffer = nullptr;
			size = 0;
		}

		// Returns the stream starting from the buffer up until the start of the subregion (even when the size of the subregion
		// Does not cover the end of the stream)
		ECS_INLINE Stream<T> StartDifference(Stream<T> subregion) const {
			return { buffer, (size_t)(subregion.buffer - buffer) };
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
		size_t Find(Value value, ProjectionFunctor&& functor) const {
			for (size_t index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		size_t Find(const Value* value, ProjectionFunctor&& functor) const {
			for (size_t index = 0; index < size; index++) {
				if (*value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		size_t Find(T value) const {
			return Find(value, [](T current_value) {
				return current_value;
			});
		}

		ECS_INLINE void Insert(size_t index, T value) {
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
		}

		ECS_INLINE T& Last() {
			return buffer[size - 1];
		}

		ECS_INLINE const T& Last() const {
			return buffer[size - 1];
		}

		// Set the count to the count of elements that you want to be removed
		ECS_INLINE void Remove(size_t index, size_t count = 1) {
			memmove(buffer + index, buffer + index + count, (size - index - count) * sizeof(T));
			size -= count;
		}

		ECS_INLINE void RemoveSwapBack(size_t index) {
			size--;
			buffer[index] = buffer[size];
		}

		// It will assert that the value exists
		ECS_INLINE void RemoveSwapBackByValue(T value, const char* assert_message) {
			size_t index = Find(value);
			ECS_ASSERT(index != -1, assert_message);
			RemoveSwapBack(index);
		}

		// It will assert that the value exists
		ECS_INLINE void ReplaceByValue(T old_value, T new_value, const char* assert_message) {
			size_t index = Find(old_value);
			ECS_ASSERT(index != -1, assert_message);
			buffer[index] = new_value;
		}


		// Returns true if the previous value existed, else false
		ECS_INLINE bool TryReplaceByValue(T old_value, T new_value) {
			size_t index = Find(old_value);
			if (index != -1) {
				buffer[index] = new_value;
				return true;
			}
			return false;
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
			void* new_allocation = ECSEngine::ReallocateWithCopyNonNull(allocator, buffer, size, CopySize(), MemoryOf(new_size), alignof(T), debug_info);
			ECS_ASSERT(new_allocation != nullptr || new_size == 0);
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
		
		// Returns the stream starting at the given offset until the end
		ECS_INLINE Stream<T> SliceAt(size_t offset) const {
			return { buffer + offset, size - offset };
		}

		// Returns the stream starting at the given offset for a given count of elements
		ECS_INLINE Stream<T> SliceAt(size_t offset, size_t count) const {
			return { buffer + offset, count };
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

		// Returns the alignment of the underlying element
		ECS_INLINE static size_t AlignOf() {
			return alignof(T);
		}

		ECS_INLINE void InitializeFromBuffer(void* _buffer, size_t _size) {
			buffer = (T*)_buffer;
			size = _size;
		}

		ECS_INLINE void InitializeFromBuffer(uintptr_t& _buffer, size_t _size) {
			buffer = (T*)_buffer;
			_buffer += sizeof(T) * _size;
			size = _size;
		}

		Stream<T> InitializeAndCopy(uintptr_t& _buffer, Stream<T> other) {
			InitializeFromBuffer(_buffer, other.size);
			CopyOther(other);

			return *this;
		}

		Stream<T> InitializeFromBufferAndCopy(void* _buffer, Stream<T> other) {
			InitializeFromBuffer(_buffer, other.size);
			CopyOther(other);

			return *this;
		}

		Stream<T> InitializeAndCopy(AllocatorPolymorphic allocator, Stream<T> other, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Initialize(allocator, other.size, debug_info);
			CopyOther(other);

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

		ECS_INLINE StreamIterator<const T> ConstIterator(size_t starting_index = 0) const {
			return { buffer, size, starting_index };
		}

		ECS_INLINE StreamIterator<T> MutableIterator(size_t starting_index = 0) {
			return { buffer, size, starting_index };
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

		unsigned int AddIterator(IteratorInterface<T>* iterator) {
			unsigned int previous_size = size;
			// The CapacityStream<T> implements the static interface of a fixed size container for the add
			AddIteratorToContainerFixed(iterator, *this);
			return previous_size;
		}

		bool AddSafe(T element) {
			if (size == capacity) {
				return false;
			}
			Add(element);
			return true;
		}

		bool AddSafe(const T* element) {
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

		bool AddStreamSafe(Stream<T> other) {
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

		// The same as the overload, but it determines the growth count based on the current capacity
		ECS_INLINE unsigned int AddResizePercentage(T element, AllocatorPolymorphic allocator, float growth_percentage = ECS_RESIZABLE_STREAM_FACTOR, bool deallocate_previous = true) {
			return AddResize(element, allocator, (unsigned int)(capacity * (growth_percentage - 1.0f)) + 1, deallocate_previous);
		}

		// Returns the index at which they were added. Can select the amount by which it grows with the growth_count
		// (the total growth count is necessary_elements + growth_count)
		unsigned int AddResizeStream(Stream<T> elements, AllocatorPolymorphic allocator, unsigned int growth_count = 0, bool deallocate_previous = true) {
			Expand(allocator, elements.size, growth_count, deallocate_previous);
			return AddStream(elements);
		}

		// The same as the overload, but it determines the growth count based on the current capacity
		ECS_INLINE unsigned int AddResizeStreamPercentage(Stream<T> element, AllocatorPolymorphic allocator, float growth_percentage = ECS_RESIZABLE_STREAM_FACTOR, bool deallocate_previous = true) {
			return AddResizeStream(element, allocator, (unsigned int)(capacity * (growth_percentage - 1.0f)) + 1, deallocate_previous);
		}

		ECS_INLINE void AssertCapacity() const {
			ECS_ASSERT(size <= capacity);
		}

		ECS_INLINE void AssertCapacity(unsigned int elements) const {
			ECS_ASSERT(size + elements <= capacity);
		}

		ECS_INLINE void AssertSpace(unsigned int count) const {
			ECS_ASSERT(size + count <= capacity, "Not enough capacity for CapacityStream");
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

		ECS_INLINE CapacityStream<T> Copy(AllocatorPolymorphic allocator) const {
			CapacityStream<T> result;
			result.InitializeAndCopy(allocator, ToStream());
			return result;
		}

		// It will set the size
		ECS_INLINE void CopyOther(const void* memory, unsigned int count) {
			ECS_ASSERT(count <= capacity);
			memcpy(buffer, memory, sizeof(T) * count);
			size = count;
		}

		// It will set the size
		ECS_INLINE void CopyOther(Stream<T> other) {
			CopyOther(other.buffer, other.size);
		}

		// It will not set the size and will not do a check
		ECS_INLINE void CopySlice(unsigned int starting_index, const void* memory, unsigned int count) {
			memcpy(buffer + starting_index, memory, sizeof(T) * count);
		}

		// It will not set the size and will not do a check
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
		CapacityStream<T> CopyTo(uintptr_t& memory, unsigned int count) const {
			void* current_buffer = (void*)memory;

			memcpy(current_buffer, buffer, sizeof(T) * count);
			memory += sizeof(T) * count;

			return CapacityStream<T>(current_buffer, count, count);
		}
		
		ECS_INLINE size_t CopySize() const {
			return sizeof(T) * size;
		}

		// Does not change the size or the pointer
		// It only deallocates if the size is greater than 0 and the pointer is not nullptr
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
					void* new_buffer = ECSEngine::ReallocateWithCopyNonNull(allocator, buffer, size, CopySize(), MemoryOf(needed_elements), alignof(T), debug_info);
					ECS_ASSERT(new_buffer != nullptr);
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
		unsigned int Find(Value value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		unsigned int Find(const Value* value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		unsigned int Find(T value) const {
			return Find(value, [](T current_value) {
				return current_value;
			});
		}

		ECS_INLINE bool IsFull() const {
			return size == capacity;
		}

		void Insert(unsigned int index, T value) {
			AssertCapacity(1);
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
		}

		void Insert(unsigned int index, Stream<T> values) {
			AssertCapacity(values.size);
			DisplaceElements(index, values.size);
			values.CopyTo(buffer + index);
			size += values.size;
		}

		ECS_INLINE T& Last() {
			return buffer[size - 1];
		}

		ECS_INLINE const T& Last() const {
			return buffer[size - 1];
		}

		void Resize(AllocatorPolymorphic allocator, unsigned int new_capacity) {
			buffer = (T*)ECSEngine::ReallocateWithCopyNonNull(allocator, buffer, capacity, CopySize(), MemoryOf(new_capacity), alignof(T));
			size = min(size, new_capacity);
			capacity = new_capacity;
		}

		// Asserts that there is enough space and increments the size with the given count
		// Returns the index of the first entry
		ECS_INLINE unsigned int ReserveRange(unsigned int count = 1) {
			ECS_ASSERT(size + count <= capacity);
			unsigned int index = size;
			size += count;
			return index;
		}

		ECS_INLINE void Reserve(AllocatorPolymorphic allocator, unsigned int count = 1) {
			if (size + count > capacity) {
				unsigned int new_capacity = (float)capacity * 1.5f + 4;
				// Clamp the new_capacity to cover the necessary count
				new_capacity = new_capacity < size + count ? size + count : new_capacity;
				Resize(allocator, new_capacity);
			}
		}

		ECS_INLINE void Reset() {
			InitializeFromBuffer(nullptr, 0, 0);
		}

		// Set the count to the number of elements that you want to be removed
		ECS_INLINE void Remove(unsigned int index, unsigned int count = 1) {
			memmove(buffer + index, buffer + index + count, (size - index - count) * sizeof(T));
			size -= count;
		}

		ECS_INLINE void RemoveSwapBack(unsigned int index) {
			size--;
			buffer[index] = buffer[size];
		}

		// It will assert that the value exists
		ECS_INLINE void RemoveSwapBackByValue(T value, const char* assert_message) {
			unsigned int index = Find(value);
			ECS_ASSERT(index != -1, assert_message);
			RemoveSwapBack(index);
		}

		// It will assert that the value exists
		ECS_INLINE void ReplaceByValue(T old_value, T new_value, const char* assert_message) {
			unsigned int index = Find(old_value);
			ECS_ASSERT(index != -1, assert_message);
			buffer[index] = new_value;
		}

		// Returns true if the previous value existed, else false
		ECS_INLINE bool TryReplaceByValue(T old_value, T new_value) {
			unsigned int index = Find(old_value);
			if (index != -1) {
				buffer[index] = new_value;
				return true;
			}
			return false;
		}

		// Returns the stream starting from the buffer up until the start of the subregion (even when the size of the subregion
		// Does not cover the end of the capacity stream)
		ECS_INLINE CapacityStream<T> StartDifference(Stream<T> subregion) const {
			return { buffer, (unsigned int)(subregion.buffer - buffer), capacity };
		}

		ECS_INLINE void Swap(unsigned int first, unsigned int second) {
			T copy = buffer[first];
			buffer[first] = buffer[second];
			buffer[second] = copy;
		}

		void SwapContents() {
			for (unsigned int index = 0; index < size >> 1; index++) {
				Swap(index, size - index - 1);
			}
		}

		// Returns the capacity stream at the given offset until the end of the stream
		ECS_INLINE CapacityStream<T> SliceAt(unsigned int offset) const {
			return { buffer + offset, size - offset, capacity - offset };
		}

		// Returns a stream at the given offset for a given count of elements
		ECS_INLINE Stream<T> SliceAt(unsigned int offset, unsigned int count) const {
			return { buffer + offset, count };
		}

		// Returns a stream that contains the last elements
		ECS_INLINE Stream<T> GetLastElements(unsigned int count) const {
			return SliceAt(size - count);
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

		// Returns the alignment of the underlying element
		ECS_INLINE static size_t AlignOf() {
			return alignof(T);
		}

		ECS_INLINE Stream<T> ToStream() const {
			return { buffer, size };
		}

		ECS_INLINE void InitializeFromBuffer(void* _buffer, unsigned int _size, unsigned int _capacity) {
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
		CapacityStream<T> InitializeAndCopy(uintptr_t& _buffer, Stream<T> other) {
			InitializeFromBuffer(_buffer, other.size, other.size);
			CopyOther(other);

			return *this;
		}

		CapacityStream<T> InitializeFromBufferAndCopy(void* _buffer, Stream<T> other) {
			InitializeFromBuffer(_buffer, other.size, other.size);
			CopyOther(other);
			
			return *this;
		}

		// It will make a copy with the capacity the same as the stream's size
		CapacityStream<T> InitializeAndCopy(AllocatorPolymorphic allocator, Stream<T> other, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Initialize(allocator, other.size, other.size, debug_info);
			CopyOther(other);

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

		ECS_INLINE StreamIterator<const T> ConstIterator(unsigned int starting_index = 0) const {
			return { buffer, size, starting_index };
		}

		ECS_INLINE StreamIterator<T> MutableIterator(unsigned int starting_index = 0) {
			return { buffer, size, starting_index };
		}

		T* buffer;
		unsigned int size;
		unsigned int capacity;
	};

	template<typename T>
	struct ResizableStream {
		typedef T T;

		ECS_INLINE ResizableStream() : buffer(nullptr), allocator({nullptr}), capacity(0), size(0) {}
		ResizableStream(AllocatorPolymorphic _allocator, unsigned int _capacity) : buffer(nullptr), capacity(0), size(0) {
			Initialize(_allocator, _capacity);
		}

		ECS_INLINE ResizableStream(const ResizableStream& other) = default;
		ECS_INLINE ResizableStream<T>& operator = (const ResizableStream<T>& other) = default;

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
			unsigned int write_index = ReserveRange(other.size);
			CopySlice(write_index, other);
			return write_index;
		}

		unsigned int AddIterator(IteratorInterface<T>* iterator) {
			unsigned int previous_size = size;
			// The ResizableStream<T> implements the necessary functions for the resizable static interface
			AddIteratorToContainerResizable(iterator, *this);
			return previous_size;
		}

		ECS_INLINE void Clear() {
			size = 0;
		}

		ECS_INLINE ResizableStream<T> Copy(AllocatorPolymorphic allocator) const {
			ResizableStream<T> result;
			result.InitializeAndCopy(allocator, ToStream());
			return result;
		}

		// It will set the size
		ECS_INLINE void CopyOther(const void* memory, unsigned int count) {
			if (count != size) {
				ResizeNoCopy(count);
			}
			memcpy(buffer, memory, sizeof(T) * count);
			size = count;
		}

		// It will set the size
		ECS_INLINE void CopyOther(Stream<T> other) {
			CopyOther(other.buffer, other.size);
		}

		// It will not set the size and will not do a check
		ECS_INLINE void CopySlice(unsigned int starting_index, const void* memory, unsigned int count) {
			memcpy(buffer + starting_index, memory, sizeof(T) * count);
		}

		// It will not set the size and will not do a check
		ECS_INLINE void CopySlice(unsigned int starting_index, Stream<T> other) {
			CopySlice(starting_index, other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, sizeof(T) * size);
		}

		// If it is desirable to be used with StreamCoalescedDeepCopy, the problem is that
		// it cannot use coalesced allocation. Wait for a use case to decide
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
				Deallocate(allocator, buffer, debug_info);
				buffer = nullptr;
				size = 0;
				capacity = 0;
			}
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		unsigned int Find(Value value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		// Returns -1 if it doesn't find it
		template<typename Value, typename ProjectionFunctor>
		unsigned int Find(const Value* value, ProjectionFunctor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (*value == functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}

		unsigned int Find(T value) const {
			return Find(value, [](T current_value) {
				return current_value;
			});
		}

		void Insert(unsigned int index, T value) {
			// We need to call the variant that doesn't update the size, since that it incorrectly move some elements
			// If the size is incremented. Increment the size manually afterwards
			Reserve();
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
		}

		void Insert(unsigned int index, Stream<T> values) {
			// We need to call the variant that doesn't update the size, since that it incorrectly move some elements
			// If the size is incremented. Increment the size manually afterwards
			Reserve(values.size);
			DisplaceElements(index, values.size);
			values.CopyTo(buffer + index);
			size += values.size;
		}

		ECS_INLINE T& Last() {
			return buffer[size - 1];
		}

		ECS_INLINE const T& Last() const {
			return buffer[size - 1];
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

		// It will assert that the value exists
		ECS_INLINE void RemoveSwapBackByValue(T value, const char* assert_message) {
			unsigned int index = Find(value);
			ECS_ASSERT(index != -1, assert_message);
			RemoveSwapBack(index);
		}

		// It will assert that the value exists
		ECS_INLINE void ReplaceByValue(T old_value, T new_value, const char* assert_message) {
			unsigned int index = Find(old_value);
			ECS_ASSERT(index != -1, assert_message);
			buffer[index] = new_value;
		}

		// Returns true if the previous value existed, else false
		ECS_INLINE bool TryReplaceByValue(T old_value, T new_value) {
			unsigned int index = Find(old_value);
			if (index != -1) {
				buffer[index] = new_value;
				return true;
			}
			return false;
		}

		// Makes sure there is enough space for extra count elements
		// Does not increment the size
		ECS_INLINE void Reserve(unsigned int count = 1) {
			if (size + count > capacity) {
				unsigned int new_capacity = ECS_RESIZABLE_STREAM_FACTOR * capacity + 4;
				unsigned int resize_count = new_capacity < capacity + count ? capacity + count : new_capacity;
				Resize(resize_count);
			}
		}

		// Makes sure there is enough space for extra count elements
		// And increases the size with that count
		unsigned int ReserveRange(unsigned int count = 1) {
			unsigned int initial_size = size;
			Reserve(count);
			size += count;
			return initial_size;
		}

		private:
			template<bool copy_old_data>
			void ResizeImpl(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
				void* new_buffer = nullptr;

				if (new_capacity != 0) {
					if (buffer != nullptr && capacity > 0) {
						unsigned int copy_size = size < new_capacity ? size : new_capacity;
						if constexpr (copy_old_data) {
							new_buffer = ECSEngine::ReallocateWithCopy(allocator, buffer, CopySize(), MemoryOf(new_capacity), alignof(T), debug_info);
						}
						else {
							new_buffer = ECSEngine::Reallocate(allocator, buffer, MemoryOf(new_capacity), alignof(T), debug_info);
						}
						ECS_ASSERT(new_buffer != nullptr);
					}
					else {
						new_buffer = ECSEngine::Allocate(allocator, MemoryOf(new_capacity), alignof(T), debug_info);
					}
				}
				else {
					FreeBuffer(debug_info);
				}

				buffer = (T*)new_buffer;
				capacity = new_capacity;
			}

		public:

		// It does not set the size, it will only allocate the given capacity
		ECS_INLINE void Resize(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ResizeImpl<true>(new_capacity, debug_info);
		}

		// It does not set the size, it will only allocate the given capacity
		ECS_INLINE void ResizeNoCopy(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ResizeImpl<false>(new_capacity, debug_info);
		}

		// Returns the stream starting at the given offset until the end of the stream
		ECS_INLINE Stream<T> SliceAt(unsigned int offset) const {
			return { buffer + offset, size - offset };
		}

		// Returns the stream starting at the given offset for a given count of elements
		ECS_INLINE Stream<T> SliceAt(unsigned int offset, unsigned int count) const {
			return { buffer + offset, count };
		}

		ECS_INLINE void Swap(unsigned int first, unsigned int second) {
			swap(buffer[first], buffer[second]);
		}

		void SwapContents() {
			for (unsigned int index = 0; index < size >> 1; index++) {
				Swap(index, size - index - 1);
			}
		}
		
		// It performs a trim if the difference between capacity and size is greater than additional_elements.
		// If that is the case, it will resize the stream to contain size + additional_elements entries.
		// Returns true if a resizing was performed, else false
		bool Trim(unsigned int additional_elements = 0, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (additional_elements < capacity - size) {
				unsigned int elements_to_copy = size + additional_elements;
				Resize(size + additional_elements, debug_info);
				return true;
			}
			return false;
		}

		// It is the same as Trim, the difference being that it uses relative values, percentages in the range [0-1]
		// Instead of flat values. The first parameter describes the minimum capacity for which this should be performed.
		// If the capacity is less than that value, it won't perform a trim. Returns true if a resizing was performed, else false
		bool TrimPercentage(unsigned int minimum_capacity, float threshold_percentage, float additional_percentage = 0.0f, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (capacity > minimum_capacity) {
				float current_percentage = (float)size / (float)capacity;
				if (current_percentage < threshold_percentage) {
					unsigned int additional_elements = (unsigned int)((float)capacity * additional_percentage);
					Resize(size + additional_elements, debug_info);
					return true;
				}
			}
			return false;
		}

		// Performs a trim percentage with default values - 16 for minimum capacity, 0.5f for the threshold and 0.1f for additional elements
		ECS_INLINE bool TrimPercentageDefault() {
			return TrimPercentage(16, 0.5f, 0.1f);
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

		// Returns the alignment of the underlying element
		ECS_INLINE static size_t AlignOf() {
			return alignof(T);
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			// Set the size to 0 first such that
			// The resize no copy won't trigger a reallocation
			size = 0;

			allocator = _allocator;
			// Set these to nullptr and 0 even when _capacity is larger than 0, in case it contains already some non null values
			buffer = nullptr;
			capacity = 0;
			if (_capacity > 0) {
				ResizeNoCopy(_capacity, debug_info);
			}
		}

		ResizableStream<T> InitializeAndCopy(AllocatorPolymorphic _allocator, Stream<T> stream, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Initialize(_allocator, stream.size);
			// Works for the 0 case as well
			CopyOther(stream);
			return *this;
		}

		ECS_INLINE StreamIterator<const T> ConstIterator(unsigned int starting_index = 0) const {
			return { buffer, size, starting_index };
		}

		ECS_INLINE StreamIterator<T> MutableIterator(unsigned int starting_index = 0) {
			return { buffer, size, starting_index };
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
		ECS_INLINE Stream(const T* type) : buffer((void*)type), size(sizeof(T)) {
			static_assert(!std::is_same_v<T, void>, "Stream<void> constructor given a void* pointer to construct from!");
		}

		template<typename T>
		ECS_INLINE Stream(Stream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}
		
		template<typename T, typename = std::enable_if_t<!std::is_same_v<void, T>>>
		ECS_INLINE Stream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}

		ECS_INLINE Stream(const Stream& other) = default;
		ECS_INLINE Stream<void>& operator = (const Stream<void>& other) = default;

		ECS_INLINE bool operator == (Stream<void> other) const {
			return buffer == other.buffer && size == other.size;
		}

		ECS_INLINE bool operator != (Stream<void> other) const {
			return !(*this == other);
		}

		// Returns the subrange in this entry
		ECS_INLINE Stream<void> Add(Stream<void> other) {
			size_t previous_size = size;
			memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
			size += other.size;
			return SliceAt(previous_size);
		}

		// If using the buffer for untyped data
		ECS_INLINE void AddElement(const void* data, size_t byte_size) {
			SetElement(size, data, byte_size);
			size++;
		}

		ECS_INLINE bool Compare(Stream<void> other) const {
			return size == other.size && memcmp(buffer, other.buffer, size) == 0;
		}

		// Returns true if the other stream is fully contained in this one
		bool Contains(Stream<void> other) const {
			uintptr_t ptr = (uintptr_t)buffer;
			uintptr_t other_ptr = (uintptr_t)other.buffer;
			uintptr_t final_ptr = (uintptr_t)buffer + size;
			uintptr_t other_final_ptr = (uintptr_t)other.buffer + other.size;
			return (ptr <= other_ptr && other_ptr < final_ptr) && (ptr <= other_final_ptr && other_final_ptr < final_ptr);
		}

		// It will set the size
		ECS_INLINE void CopyOther(const void* memory, size_t memory_size) {
			memcpy(buffer, memory, memory_size);
			size = memory_size;
		}

		Stream<void> Copy(AllocatorPolymorphic allocator) const {
			Stream<void> result;
			result.Initialize(allocator, size);
			result.CopyOther(buffer, size);
			return result;
		}

		Stream<void> Copy(AllocatorPolymorphic allocator, unsigned int element_byte_size) const {
			Stream<void> result;
			result.Initialize(allocator, size * element_byte_size);
			result.CopyOther(buffer, size * element_byte_size);
			result.size = size;
			return result;
		}

		// It will not set the size
		ECS_INLINE void CopySlice(size_t offset, const void* memory, size_t memory_size) {
			memcpy((void*)((size_t)buffer + offset), memory, memory_size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		// It considers this stream to contain untyped data with the size being the count, not
		// The byte size such that it can copy to other buffers
		ECS_INLINE void CopyTo(void* memory, size_t byte_size) const {
			memcpy(memory, buffer, size * byte_size);
		}

		ECS_INLINE Stream<void> CopyTo(uintptr_t& ptr) const {
			Stream<void> copy;
			copy.InitializeFromBuffer(ptr, size);
			copy.CopyOther(buffer, size);
			return copy;
		}

		ECS_INLINE size_t CopySize() const {
			return size;
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (size > 0 && buffer != nullptr) {
				ECSEngine::Deallocate(allocator, buffer, debug_info);
				buffer = nullptr;
				size = 0;
			}
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

		// Returns the subrange starting at the given offset until the end of this stream
		ECS_INLINE Stream<void> SliceAt(size_t offset) const {
			return { (void*)((uintptr_t)buffer + offset), size - offset };
		}

		// Returns the subrange starting at the given offset for a given count of bytes
		ECS_INLINE Stream<void> SliceAt(size_t offset, size_t byte_size) const {
			return { (void*)((uintptr_t)buffer + offset), byte_size };
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
		ECS_INLINE void Initialize(Allocator* allocator, size_t _size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (_size > 0) {
				buffer = allocator->Allocate(_size, alignof(void*), debug_info);
			}
			else {
				buffer = nullptr;
			}
			size = _size;
		}

		ECS_INLINE void Initialize(AllocatorPolymorphic allocator, size_t _size, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (_size > 0) {
				buffer = Allocate(allocator, _size, alignof(void*), debug_info);
			}
			else {
				buffer = nullptr;
			}
			size = _size;
		}

		ECS_INLINE void InitializeFromBuffer(void* _buffer, size_t _size) {
			buffer = _buffer;
			size = _size;
		}

		ECS_INLINE void InitializeFromBuffer(uintptr_t& _buffer, size_t _size) {
			buffer = (void*)_buffer;
			size = _size;
			_buffer += size;
		}

		ECS_INLINE void InitializeAndCopy(uintptr_t& _buffer, Stream<void> data) {
			*this = data.CopyTo(_buffer);
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
		ECS_INLINE CapacityStream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)), capacity(other.capacity * sizeof(T)) {}

		ECS_INLINE operator Stream<void>() const {
			return { buffer, size };
		}

		ECS_INLINE CapacityStream(const CapacityStream& other) = default;
		ECS_INLINE CapacityStream<void>& operator = (const CapacityStream<void>& other) = default;

		// Returns the subrange inside this entry
		ECS_INLINE Stream<void> Add(Stream<void> other) {
			ECS_ASSERT(other.size <= UINT_MAX, "Capacity stream integer overflow on stream addition.");
			AssertCapacity(other.size);
			unsigned int previous_size = size;
			memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
			size += (unsigned int)other.size;
			return SliceAt(previous_size);
		}

		// If using the buffer for untyped data
		ECS_INLINE void AddElement(const void* data, unsigned int byte_size) {
			ECS_ASSERT(size < capacity);
			SetElement(size, data, byte_size);
			size++;
		}

		// The size of the capacity stream must be the number of elements, not the byte size
		template<typename T>
		ECS_INLINE void AddElements(Stream<T> elements) {
			AssertCapacity(elements.size);
			CopySlice(size * sizeof(T), elements.buffer, elements.CopySize());
			size += elements.size;
		}

		ECS_INLINE void AssertCapacity(unsigned int extra_size) const {
			ECS_ASSERT(size + extra_size <= capacity);
		}

		ECS_INLINE void AssertCapacity(unsigned int extra_size, const char* message) const {
			ECS_ASSERT(size + extra_size <= capacity, message);
		}

		// It will set the size
		ECS_INLINE void CopyOther(const void* memory, unsigned int memory_size) {
			ECS_ASSERT(memory_size <= capacity);
			memcpy(buffer, memory, memory_size);
			size = memory_size;
		}
		
		ECS_INLINE void CopyOther(Stream<void> other, unsigned int element_byte_size) {
			ECS_ASSERT(other.size <= capacity);
			memcpy(buffer, other.buffer, other.size * element_byte_size);
			size = other.size;
		}

		CapacityStream<void> Copy(AllocatorPolymorphic allocator) const {
			CapacityStream<void> result;
			result.Initialize(allocator, size);
			result.CopyOther(buffer, size);
			return result;
		}

		// It will not set the size
		ECS_INLINE void CopySlice(unsigned int offset, const void* memory, unsigned int memory_size) {
			memcpy((void*)((uintptr_t)buffer + offset), memory, memory_size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (buffer != nullptr && capacity > 0) {
				ECSEngine::Deallocate(allocator, buffer, debug_info);
				buffer = nullptr;
				size = 0;
				capacity = 0;
			}
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

		ECS_INLINE void* Reserve(unsigned int reserve_size) {
			ECS_ASSERT(size + reserve_size <= capacity);
			void* value = (void*)((uintptr_t)buffer + size);
			size += reserve_size;
			return value;
		}

		template<typename T>
		ECS_INLINE T* Reserve() {
			return (T*)Reserve(sizeof(T));
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

		// Returns the subrange starting at the given offset until the end of the size
		ECS_INLINE CapacityStream<void> SliceAt(unsigned int offset) const {
			return { (void*)((uintptr_t)buffer + offset), size - offset, capacity - offset };
		}

		// Returns the subrange starting at the given offset for a given byte size
		ECS_INLINE Stream<void> SliceAt(unsigned int offset, unsigned int byte_size) const {
			return { (void*)((uintptr_t)buffer + offset), byte_size };
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
		ECS_INLINE void Initialize(Allocator* allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			buffer = _capacity == 0 ? nullptr : allocator->Allocate(_capacity, alignof(void*), debug_info);
			size = 0;
			capacity = _capacity;
		}

		ECS_INLINE void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			buffer = _capacity == 0 ? nullptr : Allocate(allocator, _capacity, alignof(void*), debug_info);
			size = 0;
			capacity = _capacity;
		}

		ECS_INLINE void InitializeFromBuffer(void* _buffer, unsigned int _size, unsigned int _capacity) {
			buffer = _buffer;
			size = _size;
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

		ECS_INLINE ResizableStream() : buffer(nullptr), allocator(nullptr), capacity(0), size(0) {}
		ResizableStream(AllocatorPolymorphic _allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) : allocator(_allocator), 
			capacity(_capacity), size(0) {
			if (_capacity != 0) {
				buffer = Allocate(allocator, _capacity, alignof(void*), debug_info);
			}
			else {
				buffer = nullptr;
			}
		}

		ECS_INLINE ResizableStream(const ResizableStream& other) = default;
		ECS_INLINE ResizableStream<void>& operator = (const ResizableStream<void>& other) = default;

		// Assumes that the element byte size and alignment is 1
		ECS_INLINE unsigned int Add(Stream<void> data) {
			return Add(data, 1, 1);
		}

		unsigned int Add(Stream<void> data, unsigned int element_byte_size, unsigned int element_alignment) {
			ECS_ASSERT(data.size <= UINT_MAX, "Resizable stream addition of stream overflows the integer range.");
			if (size + data.size >= capacity) {
				Resize((unsigned int)((float)capacity * ECS_RESIZABLE_STREAM_FACTOR + 2), element_byte_size, element_alignment);
			}
			memcpy((void*)((uintptr_t)buffer + (size_t)size * (size_t)element_byte_size), data.buffer, data.size * element_byte_size);
			unsigned int offset = size;
			size += (unsigned int)data.size;
			return offset;
		}

		// It will set the size. Assumes that the element byte size and alignment is 1
		ECS_INLINE void CopyOther(const void* memory, unsigned int count) {
			CopyOther(memory, count, 1, 1);
		}

		void CopyOther(const void* memory, unsigned int count, unsigned int element_byte_size, unsigned int element_alignment) {
			if (count != capacity) {
				ResizeNoCopy(count, element_byte_size, element_alignment);
			}
			memcpy(buffer, memory, (size_t)count * (size_t)element_byte_size);
			size = count;
		}

		// It will set the size
		ECS_INLINE void CopyOther(Stream<void> other) {
			CopyOther(other.buffer, other.size);
		}

		ECS_INLINE void CopyOther(Stream<void> other, unsigned int element_byte_size, unsigned int element_alignment) {
			CopyOther(other.buffer, other.size, element_byte_size, element_alignment);
		}

		// Assumes that the element byte size and alignment is 1
		ECS_INLINE ResizableStream<void> Copy(AllocatorPolymorphic _allocator) const {
			return Copy(_allocator, 1, 1);
		}

		ResizableStream<void> Copy(AllocatorPolymorphic _allocator, unsigned int element_byte_size, unsigned int element_alignment) const {
			ResizableStream<void> result;
			result.Initialize(_allocator, size, element_byte_size, element_alignment);
			result.CopyOther(buffer, size, element_byte_size, element_alignment);
			return result;
		}

		// It will not set the size
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
				Deallocate(allocator, buffer, debug_info);
				buffer = nullptr;
				size = 0;
				capacity = 0;
			}
		}

		ECS_INLINE void* Get(unsigned int index, unsigned int byte_size) {
			return (void*)((uintptr_t)buffer + (size_t)index * (size_t)byte_size);
		}

		ECS_INLINE const void* Get(unsigned int index, unsigned int byte_size) const {
			return (const void*)((uintptr_t)buffer + (size_t)index * (size_t)byte_size);
		}

		ECS_INLINE void Clear() {
			size = 0;
		}

		// Makes sure there is enough space for extra count bytes
		void Reserve(unsigned int byte_count) {
			unsigned int new_size = size + byte_count;
			if (new_size > capacity) {
				unsigned int resize_capacity = (unsigned int)(ECS_RESIZABLE_STREAM_FACTOR * capacity + 3);
				if (resize_capacity < new_size) {
					resize_capacity = (unsigned int)((float)new_size * ECS_RESIZABLE_STREAM_FACTOR);
				}
				Resize(resize_capacity);
			}
		}

		// Assumes that the element byte size and alignment is 1
		ECS_INLINE void Resize(unsigned int new_capacity) {
			Resize(new_capacity, 1, 1);
		}

		void Resize(unsigned int new_capacity, unsigned int element_byte_size, unsigned int element_alignment, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (new_capacity > 0) {
				// Use a default of max element alignment
				void* new_buffer = ReallocateWithCopyNonNull(allocator, buffer, capacity, size * element_byte_size, new_capacity * element_byte_size, element_alignment, debug_info);
				ECS_ASSERT(new_buffer != nullptr || new_capacity == 0);
				buffer = new_buffer;
			}
			else {
				if (buffer != nullptr) {
					Deallocate(allocator, buffer, debug_info);
					buffer = nullptr;
				}
			}
			capacity = new_capacity;
		}

		// Assumes the element byte size and alignment is 1
		ECS_INLINE void ResizeNoCopy(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ResizeNoCopy(new_capacity, 1, 1, debug_info);
		}

		// The element alignment is mandatory in case you use a strongly typed stream
		// That is used untyped in some other location due to the code nature, and if the
		// Alignment is not proper, it can result in deallocation errors
		void ResizeNoCopy(unsigned int new_capacity, unsigned int element_byte_size, unsigned int element_alignment, DebugInfo debug_info = ECS_DEBUG_INFO) {
			void* new_buffer = nullptr;
			if (new_capacity > 0 && size > 0) {
				new_buffer = Reallocate(allocator, buffer, new_capacity * element_byte_size, element_alignment, debug_info);
				ECS_ASSERT(new_buffer != nullptr);
			}
			else if (size > 0) {
				Deallocate(allocator, buffer, debug_info);
			}
			else if (new_capacity > 0) {
				new_buffer = Allocate(allocator, new_capacity * element_byte_size, element_alignment, debug_info);
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

		// Assumes that the element byte size and alignment is 1
		ECS_INLINE void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			Initialize(_allocator, _capacity, 1, 1, debug_info);
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity, unsigned int element_byte_size, unsigned int element_alignment, DebugInfo debug_info = ECS_DEBUG_INFO) {
			allocator = _allocator;
			size = 0;
			buffer = nullptr;
			capacity = 0;
			if (_capacity > 0) {
				ResizeNoCopy(_capacity, element_byte_size, element_alignment, debug_info);
			}		
		}

		void* buffer;
		unsigned int size;
		unsigned int capacity;
		AllocatorPolymorphic allocator;
	};

	/*
		All projection functions from here receive as argument T or const T&
	*/

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
	// Type Copy(AllocatorPolymorphic allocator) const;
	template<typename ProjectedType, typename StreamType, typename Projection>
	Stream<ProjectedType> StreamDeepCopyProject(StreamType input, AllocatorPolymorphic allocator, Projection&& projection, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Stream<ProjectedType> result;

		result.Initialize(allocator, input.size, debug_info);
		for (size_t index = 0; index < (size_t)input.size; index++) {
			result[index] = projection(input[index]).Copy(allocator);
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
			void* allocation = Allocate(allocator, copy_size, alignof(typename Stream::T), debug_info);
			uintptr_t ptr = (uintptr_t)allocation;
			result[index] = input[index].CopyTo(ptr);
		}

		return result;
	}

	// The template parameter of the stream must have as functions
	// Type CopyTo(uintptr_t& ptr) const;
	// size_t CopySize() const;
	template<typename ProjectedType, typename StreamType, typename Projection>
	Stream<ProjectedType> StreamDeepCopyToProject(StreamType input, AllocatorPolymorphic allocator, Projection&& projection, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Stream<ProjectedType> result;

		result.Initialize(allocator, input.size, debug_info);
		for (size_t index = 0; index < (size_t)input.size; index++) {
			auto index_projection = projection(input[index]);
			size_t copy_size = index_projection.CopySize();
			void* allocation = Allocate(allocator, copy_size, alignof(ProjectedType), debug_info);
			uintptr_t ptr = (uintptr_t)allocation;
			result[index] = index_projection.CopyTo(ptr);
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
	void StreamInPlaceDeepCopyTo(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		for (size_t index = 0; index < (size_t)input.size; index++) {
			size_t copy_size = input[index].CopySize();
			void* allocation = Allocate(allocator, copy_size, alignof(typename Stream::T), debug_info);
			uintptr_t ptr = (uintptr_t)allocation;
			input[index] = input[index].CopyTo(ptr);
		}
	}

	// The write stream must have enough space for the read elements
	// The template parameter of the streams must have as functions
	// Type Copy(AllocatorPolymorphic allocator) const;
	template<typename WriteStream, typename ReadStream>
	void StreamInPlaceDeepCopy(WriteStream write_stream, ReadStream read_stream, AllocatorPolymorphic allocator) {
		for (size_t index = 0; index < (size_t)read_stream.size; index++) {
			write_stream[index] = read_stream[index].Copy(allocator);
		}
	}

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	// Type Copy(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename Stream>
	Stream StreamCoalescedDeepCopy(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Stream new_stream;
		
		if (input.size > 0) {
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

			void* allocation = Allocate(allocator, total_size, alignof(typename Stream::T), debug_info);
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

			ECS_ASSERT(ptr - (uintptr_t)allocation <= total_size, "Invalid copy size function! Does not report the same amount as it uses!");
		}

		return new_stream;
	}

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	// Type Copy(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename ProjectedType, typename StreamType, typename Projection>
	Stream<ProjectedType> StreamCoalescedDeepCopyProjection(StreamType input, AllocatorPolymorphic allocator, Projection&& projection, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Stream<ProjectedType> new_stream;

		if (input.size > 0) {
			ECS_STACK_CAPACITY_STREAM(size_t, copy_sizes, 1024);
			size_t total_size = input.MemoryOf(input.size);

			if (input.size < 1024) {
				for (size_t index = 0; index < (size_t)input.size; index++) {
					size_t copy_size = projection(input[index]).CopySize();
					total_size += copy_size;

					copy_sizes[index] = copy_size;
				}
			}
			else {
				for (size_t index = 0; index < (size_t)input.size; index++) {
					total_size += projection(input[index]).CopySize();
				}
			}

			void* allocation = Allocate(allocator, total_size, alignof(ProjectedType), debug_info);
			uintptr_t ptr = (uintptr_t)allocation;
			new_stream.InitializeAndCopy(ptr, input);

			if (input.size < 1024) {
				for (size_t index = 0; index < (size_t)input.size; index++) {
					if (copy_sizes[index] > 0) {
						new_stream[index] = projection(input[index]).CopyTo(ptr);
					}
				}
			}
			else {
				for (size_t index = 0; index < (size_t)input.size; index++) {
					auto current_projection = projection(input[index]);
					size_t copy_size = current_projection.CopySize();
					if (copy_size > 0) {
						new_stream[index] = current_projection.CopyTo(ptr);
					}
				}
			}

			ECS_ASSERT(ptr - (uintptr_t)allocation <= total_size, "Invalid copy size function! Does not report the same amount as it uses!");
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
	// T CopyTo(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename ProjectedType, typename StreamType, typename Projection>
	Stream<ProjectedType> StreamCoalescedDeepCopyProjection(StreamType input, uintptr_t& ptr, Projection&& projection) {
		Stream<ProjectedType> new_stream;
		new_stream.InitializeAndCopy(ptr, input);

		for (size_t index = 0; index < (size_t)input.size; index++) {
			auto current_projection = projection(input[index]);
			size_t copy_size = current_projection.CopySize();
			if (copy_size > 0) {
				new_stream[index] = current_projection.CopyTo(ptr);
			}
		}

		return new_stream;
	}

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	template<typename Stream>
	size_t StreamCoalescedDeepCopySize(Stream input) {
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
	void StreamCoalescedInplaceDeepCopy(Stream input, uintptr_t& ptr) {
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
	size_t StreamCoalescedInplaceDeepCopySize(Stream input) {
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
	void StreamCoalescedInplaceDeepCopy(Stream input, AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		size_t allocation_size = StreamCoalescedInplaceDeepCopySize(input);
		void* allocation = allocation_size > 0 ? Allocate(allocator, allocation_size, alignof(typename Stream::T), debug_info) : nullptr;
		uintptr_t ptr = (uintptr_t)allocation;
		return StreamCoalescedInplaceDeepCopy(input, ptr);
	}

	// It will write the elements in place
	// For each entry it will create a temporary, deallocate the existing data
	// And then assigning the copy
	template<typename Stream>
	void StreamCoalescedInplaceDeepCopyWithDeallocate(
		Stream input,
		AllocatorPolymorphic allocation_allocator,
		AllocatorPolymorphic deallocation_allocator,
		DebugInfo debug_info = ECS_DEBUG_INFO
	) {
		size_t allocation_size = StreamCoalescedInplaceDeepCopySize(input);
		void* allocation = allocation_size > 0 ? Allocate(allocation_allocator, allocation_size, alignof(typename Stream::T), debug_info) : nullptr;
		uintptr_t ptr = (uintptr_t)allocation;
		for (size_t index = 0; index < (size_t)input.size; index++) {
			size_t copy_size = input[index].CopySize();
			if (copy_size > 0) {
				auto copy = input[index].CopyTo(ptr);
				input[index].Deallocate(deallocation_allocator);
				input[index] = copy;
			}
		}
	}

	// The elements must have a void Deallocate(AllocatorPolymorphic allocator) function
	template<typename Stream>
	void StreamDeallocateElements(Stream input, AllocatorPolymorphic allocator) {
		for (size_t index = 0; index < input.size; index++) {
			input[index].Deallocate(allocator);
		}
	}

	// The elements must have a void Deallocate(AllocatorPolymorphic allocator) function
	template<typename Stream, typename Projection>
	void StreamDeallocateElementsProject(Stream input, AllocatorPolymorphic allocator, Projection&& projection) {
		for (size_t index = 0; index < input.size; index++) {
			projection(input[index]).Deallocate(allocator);
		}
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
				string.AddStreamAssert(separator);
			}
		}
	}

	template<typename T>
	struct AdditionStream {
		ECS_INLINE AdditionStream() : is_capacity(true), capacity_stream(nullptr) {}
		ECS_INLINE AdditionStream(CapacityStream<T>* capacity) {
			is_capacity = true;
			capacity_stream = capacity;
		}
		ECS_INLINE AdditionStream(ResizableStream<T>* resizable) {
			is_capacity = false;
			resizable_stream = resizable;
		}

		ECS_INLINE unsigned int Add(T element) {
			if (is_capacity) {
				return capacity_stream->AddAssert(element);
			}
			else {
				return resizable_stream->Add(element);
			}
		}

		ECS_INLINE unsigned int AddStream(Stream<T> elements) {
			if (is_capacity) {
				return capacity_stream->AddStreamAssert(elements);
			}
			else {
				return resizable_stream->AddStream(elements);
			}
		}

		ECS_INLINE unsigned int AddIterator(IteratorInterface<T>* iterator) {
			if (is_capacity) {
				return capacity_stream->AddIterator(iterator);
			}
			else {
				return resizable_stream->AddIterator(iterator);
			}
		}

		ECS_INLINE bool IsCapacity() const {
			return is_capacity;
		}

		// If it is a capacity stream, asserts that the capacity
		// can hold these additional items. For resizable stream,
		// it will resize the stream to fit them in case it can't
		ECS_INLINE void AssertNewItems(unsigned int count) {
			if (is_capacity) {
				ECS_ASSERT(capacity_stream->size + count <= capacity_stream->capacity);
			}
			else {
				resizable_stream->Reserve(count);
			}
		}

		ECS_INLINE void Insert(unsigned int index, T element) {
			if (is_capacity) {
				capacity_stream->Insert(index, element);
			}
			else {
				resizable_stream->Insert(index, element);
			}
		}

		ECS_INLINE unsigned int Size() const {
			// Both of them have the same layout, we can return the size directly
			return capacity_stream->size;
		}
		
		// This sets the size directly, it does not perform a resize for the resizable stream
		ECS_INLINE void SetSize(unsigned int value) {
			// We can alias the 2 streams since they have the same layout
			capacity_stream->size = value;
		}

		// It increments the size and returns a pointer to the first entry
		ECS_INLINE T* Reserve(unsigned int count = 1) {
			if (is_capacity) {
				return capacity_stream->buffer + capacity_stream->ReserveRange(count);
			}
			else {
				return resizable_stream->buffer + resizable_stream->ReserveRange(count);
			}
		}

		ECS_INLINE void RemoveSwapBack(unsigned int index) {
			// We can alias the 2 streams
			capacity_stream->RemoveSwapBack(index);
		}

		ECS_INLINE void Remove(unsigned int index, unsigned int count = 1) {
			// We can alias the 2 streams
			capacity_stream->Remove(index, count);
		}

		ECS_INLINE Stream<T> ToStream() const {
			if (is_capacity) {
				return capacity_stream->ToStream();
			}
			else {
				return resizable_stream->ToStream();
			}
		}

		ECS_INLINE bool IsInitialized() const {
			return is_capacity ? capacity_stream && capacity_stream->capacity > 0 : resizable_stream && resizable_stream->allocator.allocator != nullptr;
		}

		ECS_INLINE T* GetBuffer() {
			// We can safely alias both structures
			return capacity_stream->buffer;
		}
		
		ECS_INLINE const T* GetBuffer() const {
			// We can safely alias both structures
			return capacity_stream->buffer;
		}
		
		ECS_INLINE T& operator[](unsigned int index) {
			// Both have the same layout, we can index into the buffer
			return capacity_stream->buffer[index];
		}

		ECS_INLINE const T& operator[](unsigned int index) const {
			// Both have the same layout, we can index into the buffer
			return capacity_stream->buffer[index];
		}

		bool is_capacity;
		union {
			CapacityStream<T>* capacity_stream;
			ResizableStream<T>* resizable_stream;
		};
	};

	// Returns true if it is stream, capacity stream or resizable stream
	template<typename ElementType, typename ContainerType>
	constexpr bool IsStreamType() {
		return std::is_same_v<ContainerType, Stream<ElementType>> ||
			std::is_same_v<ContainerType, CapacityStream<ElementType>> ||
			std::is_same_v<ContainerType, ResizableStream<ElementType>>;
	}

	// Returns the byte size of the structure, or 1 for void
	template<typename ElementType>
	constexpr size_t GetStructureByteSize() {
		if constexpr (std::is_same_v<ElementType, void>) {
			return 1;
		}
		else {
			return sizeof(ElementType);
		}
	}

	ECS_INLINE ECS_ALLOCATOR_TYPE AllocatorTypeFromString(Stream<char> string) {
		// Convenience function
		return AllocatorTypeFromString(string.buffer, string.size);
	}

	ECS_INLINE void WriteAllocatorTypeToString(ECS_ALLOCATOR_TYPE type, CapacityStream<char>& string) {
		// Convenience function
		WriteAllocatorTypeToString(type, string.buffer, string.size, string.capacity);
	}

}
