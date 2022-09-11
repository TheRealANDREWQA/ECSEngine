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

#define ECS_TEMP_ASCII_STRING(name, size) char name##_temp_memory[size]; \
ECSEngine::CapacityStream<char> name(name##_temp_memory, 0, size);

#define ECS_TEMP_STRING(name, size) wchar_t name##_temp_memory[size]; \
ECSEngine::CapacityStream<wchar_t> name(name##_temp_memory, 0, size);

	template<typename T>
	struct CapacityStream;

	template <typename T>
	struct Stream
	{
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

		// it will set the size
		ECS_INLINE void Copy(const void* memory, size_t count) {
			memcpy(buffer, memory, count * sizeof(T));
			size = count;
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<T> other) {
			Copy(other.buffer, other.size);
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

		// Added return type in order to be compatible with StreamDeepCopy
		ECS_INLINE Stream<T> CopyTo(uintptr_t& memory) const {
			void* current_buffer = (void*)memory;

			memcpy(current_buffer, buffer, sizeof(T) * size);
			memory += sizeof(T) * size;

			return Stream<T>(current_buffer, size);
		}

		ECS_INLINE size_t CopySize() const {
			return sizeof(T) * size;
		}

		ECS_INLINE void Insert(size_t index, T value) {
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
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

		/*template<typename Functor>
		size_t SearchFunctor(Functor&& functor) const {
			for (size_t index = 0; index < size; index++) {
				if (functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}*/

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

		void InitializeAndCopy(AllocatorPolymorphic allocator, Stream<T> other) {
			Initialize(allocator, other.size);
			Copy(other);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, size_t _size) {
			size_t memory_size = MemoryOf(_size);
			void* allocation = allocator->Allocate(memory_size, alignof(T));
			buffer = (T*)allocation;
			size = _size;
		}

		void Initialize(AllocatorPolymorphic allocator, size_t _size) {
			size_t memory_size = MemoryOf(_size);
			void* allocation = Allocate(allocator, memory_size, alignof(T));
			buffer = (T*)allocation;
			size = _size;
		}

		T* buffer;
		size_t size;
	};

	template<typename T>
	struct CapacityStream {
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

		ECS_INLINE bool AddStreamSafe(Stream<T> other) {
			if (size + other.size > capacity) {
				return false;
			}
			AddStream(other);
			return true;
		}

		ECS_INLINE void AssertCapacity() const {
			ECS_ASSERT(size <= capacity);
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

		// Added return type to be able to be used with StreamDeepCopy
		ECS_INLINE CapacityStream<T> CopyTo(uintptr_t& memory) const {
			void* current_buffer = (void*)memory;
			memcpy(current_buffer, buffer, sizeof(T) * size);
			memory += sizeof(T) * size;

			return CapacityStream<T>(current_buffer, size, size);
		}
		
		ECS_INLINE size_t CopySize() const {
			return sizeof(T) * size;
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

		/*template<typename Functor>
		unsigned int SearchFunctor(Functor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}*/

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
		void InitializeAndCopy(uintptr_t& buffer, Stream<T> other) {
			InitializeFromBuffer(buffer, other.size, other.size);
			Copy(other);
		}

		// It will make a copy with the capacity the same as the stream's size
		void InitializeAndCopy(AllocatorPolymorphic allocator, Stream<T> other) {
			Initialize(allocator, other.size, other.size);
			Copy(other);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _size, unsigned int _capacity) {
			size_t memory_size = MemoryOf(_capacity);
			void* allocation = allocator->Allocate(memory_size, alignof(T));
			InitializeFromBuffer(allocation, _size, _capacity);
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _size, unsigned int _capacity) {
			size_t memory_size = MemoryOf(_capacity);
			void* allocation = Allocate(allocator, memory_size, alignof(T));
			InitializeFromBuffer(allocation, _size, _capacity);
		}

		T* buffer;
		unsigned int size;
		unsigned int capacity;
	};

	template<typename T>
	struct ResizableStream {
		ECS_INLINE ResizableStream() : buffer(nullptr), allocator({nullptr}), capacity(0), size(0) {}
		ResizableStream(AllocatorPolymorphic _allocator, unsigned int _capacity) : allocator(_allocator), capacity(_capacity), size(0) {
			if (_capacity != 0) {
				buffer = (T*)Allocate(allocator, sizeof(T) * _capacity, alignof(T));
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
				Resize(static_cast<unsigned int>(ECS_RESIZABLE_STREAM_FACTOR * capacity + 1));
			}
			buffer[size++] = element;
			return size - 1;
		}

		ECS_INLINE unsigned int Add(const T* element) {
			if (size == capacity) {
				Resize(static_cast<unsigned int>(ECS_RESIZABLE_STREAM_FACTOR * capacity + 1));
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

		// If it is desirable to be used with StreamDeepCopy, the problem is that
		// it cannot use coallesced allocation. Wait for a use case to decide
		ECS_INLINE void CopyTo(uintptr_t& memory) const {
			memcpy((void*)memory, buffer, sizeof(T) * size);
			memory += sizeof(T) * size;
		}

		ECS_INLINE size_t CopySize() const {
			return sizeof(T) * size;
		}

		void FreeBuffer() {
			if (buffer != nullptr) {
				Deallocate(allocator, buffer);
				buffer = nullptr;
				size = 0;
				capacity = 0;
			}
		}

		ECS_INLINE void Insert(unsigned int index, T value) {
			ReserveNewElement();
			DisplaceElements(index, 1);
			buffer[index] = value;
			size++;
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

		void Resize(unsigned int new_capacity) {
			T* new_buffer = (T*)Allocate(allocator, new_capacity * sizeof(T), alignof(T));
			ECS_ASSERT(new_buffer != nullptr);

			memcpy(new_buffer, buffer, sizeof(T) * (size < new_capacity ? size : new_capacity));

			if (buffer != nullptr)
				Deallocate(allocator, buffer);

			buffer = new_buffer;
			capacity = new_capacity;
		}

		void ResizeNoCopy(unsigned int new_capacity) {
			T* new_buffer = (T*)Allocate(allocator, new_capacity * sizeof(T), alignof(T));
			ECS_ASSERT(new_buffer != nullptr);

			if (buffer != nullptr)
				Deallocate(allocator, buffer);

			buffer = new_buffer;
			capacity = new_capacity;
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

		/*template<typename Functor>
		unsigned int SearchFunctor(Functor&& functor) const {
			for (unsigned int index = 0; index < size; index++) {
				if (functor(buffer[index])) {
					return index;
				}
			}
			return -1;
		}*/

		void Trim() {
			T* allocation = (T*)Allocate(allocator, sizeof(T) * size, alignof(T));
			ECS_ASSERT(allocation != nullptr);

			memcpy(allocation, buffer, sizeof(T) * size);
			Deallocate(allocator, buffer);
			buffer = allocation;
			capacity = size;
		}

		// it will leave another additional_elements over the current size
		void Trim(unsigned int additional_elements) {
			if (additional_elements < capacity - size) {
				unsigned int elements_to_copy = size + additional_elements;
				T* allocation = (T*)Allocate(allocator, sizeof(T) * elements_to_copy, alignof(T));
				ECS_ASSERT(allocation != nullptr);

				memcpy(allocation, buffer, elements_to_copy * sizeof(T));
				Deallocate(allocator, buffer);
				buffer = allocation;
				capacity = size + additional_elements;
			}
		}

		// It will trim the container if the difference between the capacity and the current size is greater or equal to
		// the threshold. Returns whether or not it trimmed
		bool TrimThreshold(unsigned int threshold) {
			if (capacity - size >= threshold) {
				Trim();
				return true;
			}
			return false;
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

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity) {
			allocator = _allocator;
			if (_capacity > 0) {
				ResizeNoCopy(_capacity);
			}
			else {
				buffer = nullptr;
				capacity = 0;
			}
			size = 0;
		}

		void InitializeAndCopy(AllocatorPolymorphic _allocator, Stream<T> stream) {
			allocator = _allocator;
			if (stream.size > 0) {
				ResizeNoCopy(stream.size);
				Copy(stream);
			}
			else {
				buffer = nullptr;
				capacity = 0;
				size = 0;
			}
		}

		T* buffer;
		unsigned int size;
		unsigned int capacity;
		AllocatorPolymorphic allocator;
	};

	template <>
	struct ECSENGINE_API Stream<void>
	{
		ECS_INLINE Stream() : buffer(nullptr), size(0) {}
		ECS_INLINE Stream(const void* _buffer, size_t _size) : buffer((void*)_buffer), size(_size) {}

		template<typename T>
		Stream(Stream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}

		template<typename T>
		Stream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}

		Stream(const Stream& other) = default;
		Stream<void>& operator = (const Stream<void>& other) = default;

		ECS_INLINE void Add(Stream<void> other) {
			memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
			size += other.size;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned long long memory_size) {
			memcpy(buffer, memory, memory_size);
			size = memory_size;
		}

		// it will not set the size
		ECS_INLINE void CopySlice(size_t offset, const void* memory, unsigned long long memory_size) {
			memcpy((void*)((unsigned long long)buffer + offset), memory, memory_size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, size_t _size) {
			buffer = allocator->Allocate(_size);
			size = _size;
		}

		void Initialize(AllocatorPolymorphic allocator, size_t _size) {
			buffer = Allocate(allocator, size);
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
		ECS_INLINE CapacityStream() : buffer(nullptr), size(0), capacity(0) {}
		ECS_INLINE CapacityStream(const void* _buffer, unsigned int _size, unsigned int _capacity) : buffer((void*)_buffer), size(_size), capacity(_capacity) {}

		template<typename T>
		CapacityStream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)), capacity(other.capacity * sizeof(T)) {}

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

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned long long memory_size) {
			ECS_ASSERT(memory_size <= capacity);
			memcpy(buffer, memory, memory_size);
			size = memory_size;
		}

		// it will not set the size
		ECS_INLINE void CopySlice(size_t offset, const void* memory, unsigned long long memory_size) {
			memcpy((void*)((unsigned long long)buffer + offset), memory, memory_size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _capacity) {
			buffer = allocator->Allocate(_capacity);
			size = 0;
			capacity = _capacity;
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity) {
			buffer = Allocate(allocator, _capacity);
			size = 0;
			capacity = _capacity;
		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _size, unsigned int _capacity) {
			buffer = (void*)_buffer;
			size = _size;
			capacity = _capacity;
		}

		void* buffer;
		unsigned int size;
		unsigned int capacity;
	};

	template<>
	struct ResizableStream<void> {
		ECS_INLINE ResizableStream() : buffer(nullptr), allocator({ nullptr }), capacity(0), size(0) {}
		ResizableStream(AllocatorPolymorphic _allocator, unsigned int _capacity) : allocator(_allocator), capacity(_capacity), size(0) {
			if (_capacity != 0) {
				buffer = Allocate(allocator, _capacity);
			}
			else {
				buffer = nullptr;
			}
		}

		ResizableStream(const ResizableStream& other) = default;
		ResizableStream<void>& operator = (const ResizableStream<void>& other) = default;

		unsigned int Add(Stream<void> data) {
			if (size + data.size >= capacity) {
				
			}
			memcpy((void*)((uintptr_t)buffer + size), data.buffer, data.size);
			unsigned int offset = size;
			size += data.size;
			return offset;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned int count) {
			ResizeNoCopy(count);
			memcpy(buffer, memory, count);
			size = count;
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<void> other) {
			Copy(other.buffer, other.size);
		}

		// it will set the size
		ECS_INLINE void Copy(CapacityStream<void> other) {
			Copy(other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, size);
		}

		ECS_INLINE void CopyTo(uintptr_t& memory) const {
			memcpy((void*)memory, buffer, size);
			memory += size;
		}

		void FreeBuffer() {
			if (buffer != nullptr) {
				Deallocate(allocator, buffer);
				buffer = nullptr;
				size = 0;
				capacity = 0;
			}
		}

		ECS_INLINE void Reset() {
			size = 0;
		}

		// makes sure there is enough space for extra count elements
		void Reserve(unsigned int byte_count) {
			unsigned int new_size = size + byte_count;
			if (new_size > capacity) {
				unsigned int resize_capacity = ECS_RESIZABLE_STREAM_FACTOR * capacity + 1;
				if (resize_capacity < new_size) {
					resize_capacity = new_size * ECS_RESIZABLE_STREAM_FACTOR;
				}
				Resize(resize_capacity);
			}
		}

		void Resize(unsigned int new_capacity) {
			void* new_buffer = Allocate(allocator, new_capacity);
			ECS_ASSERT(new_buffer != nullptr);

			memcpy(new_buffer, buffer, size < new_capacity ? size : new_capacity);

			if (buffer != nullptr)
				Deallocate(allocator, buffer);

			buffer = new_buffer;
			capacity = new_capacity;
		}

		void ResizeNoCopy(unsigned int new_capacity) {
			void* new_buffer = Allocate(allocator, new_capacity);
			ECS_ASSERT(new_buffer != nullptr);

			if (buffer != nullptr)
				Deallocate(allocator, buffer);

			buffer = new_buffer;
			capacity = new_capacity;
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity) {
			allocator = _allocator;
			if (_capacity > 0) {
				ResizeNoCopy(_capacity);
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

	/*ECS_INLINE Stream<char> const char* string {
		return Stream<char>(string, strlen(string));
	}

	ECS_INLINE Stream<wchar_t> const wchar_t* string {
		return Stream<wchar_t>(string, wcslen(string));
	}*/

	// The template parameter of the stream must have as functions
	// size_t CopySize() const;
	// void Copy(uintptr_t& ptr);
	// If copy size returns 0, it assumes it needs no buffers and does not call
	// the copy function.
	template<typename Stream>
	Stream StreamDeepCopy(Stream input, AllocatorPolymorphic allocator) {
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

		void* allocation = AllocateEx(allocator, total_size);
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
	Stream StreamDeepCopy(Stream input, uintptr_t& ptr) {
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

	template<typename Stream>
	size_t StreamDeepCopySize(Stream input) {
		size_t total_size = input.MemoryOf(input.size);

		for (size_t index = 0; index < (size_t)input.size; index++) {
			total_size += input[index].CopySize();
		}

		return total_size;
	}

}
