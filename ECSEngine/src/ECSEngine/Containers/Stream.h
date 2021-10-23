#pragma once
#include "../Core.h"
#include "../Utilities/Assert.h"
#include <stdint.h>
#include <string.h>

constexpr float ECS_RESIZABLE_STREAM_FACTOR = 1.5f;

namespace ECSEngine {

	namespace containers {

		template<typename T>
		struct CapacityStream;

		template <typename T>
		struct Stream
		{
			Stream() : buffer(nullptr), size(0) {}
			Stream(const void* buffer, size_t size) : buffer((T*)buffer), size(size) {}
			Stream(CapacityStream<T> other) : buffer(other.buffer), size(other.size) {}

			Stream(const Stream& other) = default;
			Stream<T>& operator = (const Stream<T>& other) = default;

			unsigned int Add(T element) {
				buffer[size++] = element;
				return size - 1;
			}

			unsigned int Add(const T* element) {
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

			// Returns the first index
			unsigned int AddStream(CapacityStream<T> other) {
				CopySlice(size, other);
				unsigned int previous_size = size;
				size += other.size;
				return previous_size;
			}

			// it will set the size
			void Copy(const void* memory, size_t count) {
				memcpy(buffer, memory, count * sizeof(T));
				size = count;
			}

			// it will set the size
			void Copy(Stream<T> other) {
				Copy(other.buffer, other.size);
			}

			// it will set the size
			void Copy(CapacityStream<T> other) {
				Copy(other.buffer, other.size);
			}

			// it will not set the size
			void CopySlice(size_t starting_index, const void* memory, size_t count) {
				memcpy(buffer + starting_index, memory, sizeof(T) * count);
			}

			// it will not set the size
			void CopySlice(size_t starting_index, Stream<T> other) {
				CopySlice(starting_index, other.buffer, other.size);
			}
			
			// it will not set the size
			void CopySlice(size_t starting_index, CapacityStream<T> other) {
				CopySlice(starting_index, other.buffer, other.size);
			}

			void CopyTo(void* memory) const {
				memcpy(memory, buffer, sizeof(T) * size);
			}

			void CopyTo(uintptr_t& memory) const {
				memcpy((void*)memory, buffer, sizeof(T) * size);
				memory += sizeof(T) * size;
			}

			void PushDownElements(size_t starting_index, unsigned int count) {
				for (int64_t index = size - 1; index >= (int64_t)starting_index; index--) {
					buffer[index + count] = buffer[index];
				}
			}

			void Remove(unsigned int index) {
				for (size_t copy_index = index + 1; copy_index < size; copy_index++) {
					buffer[copy_index - 1] = buffer[copy_index];
				}
				size--;
			}

			void RemoveSwapBack(unsigned int index) {
				size--;
				buffer[index] = buffer[size];
			}

			void Swap(unsigned int first, unsigned int second) {
				T copy = buffer[first];
				buffer[first] = buffer[second];
				buffer[second] = copy;
			}

			void SwapContents() {
				for (size_t index = 0; index < size >> 1; index++) {
					Swap(index, size - index - 1);
				}
			}

			ECS_INLINE T& operator [](size_t index) {
				return buffer[index];
			}

			ECS_INLINE const T& operator [](size_t index) const {
				return buffer[index];
			}

			const void* GetAllocatedBuffer() const {
				return buffer;
			}

			static size_t MemoryOf(size_t number) {
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

			template<typename Allocator>
			void Initialize(Allocator* allocator, size_t _size) {
				size_t memory_size = MemoryOf(_size);
				void* allocation = allocator->Allocate(memory_size, alignof(T));
				buffer = (T*)allocation;
				size = _size;
			}

			T* buffer;
			size_t size;
		};

		template<typename T>
		struct CapacityStream {
			CapacityStream() : buffer(nullptr), size(0), capacity(0) {}
			CapacityStream(const void* buffer, unsigned int size) : buffer((T*)buffer), size(size), capacity(size) {}
			CapacityStream(const void* buffer, unsigned int size, unsigned int capacity) : buffer((T*)buffer), size(size), capacity(capacity) {}
			CapacityStream(Stream<T> stream) : buffer(stream.buffer), size(stream.size), capacity(stream.size) {}

			CapacityStream(const CapacityStream& other) = default;
			CapacityStream<T>& operator = (const CapacityStream<T>& other) = default;

			unsigned int Add(T element) {
				buffer[size++] = element;
				return size - 1;
			}

			unsigned int Add(const T* element) {
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

			// Returns the first index
			unsigned int AddStream(CapacityStream<T> other) {
				CopySlice(size, other.buffer, other.size);
				unsigned int previous_size = size;
				size += other.size;
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

			bool AddStreamSafe(Stream<T> other) {
				if (size + other.size > capacity) {
					return false;
				}
				AddStream(other);
				return true;
			}

			bool AddStreamSafe(CapacityStream<T> other) {
				if (size + other.size > capacity) {
					return false;
				}
				AddStream(other);
				return true;
			}

			void AssertCapacity() const {
				ECS_ASSERT(size <= capacity);
			}

			// it will set the size
			void Copy(const void* memory, size_t count) {
				ECS_ASSERT(count <= capacity);
				memcpy(buffer, memory, sizeof(T) * count);
				size = count;
			}

			// it will set the size
			void Copy(Stream<T> other) {
				Copy(other.buffer, other.size);
			}

			// it will set the size
			void Copy(CapacityStream<T> other) {
				Copy(other.buffer, other.size);
			}

			// it will not set the size and will not do a check
			void CopySlice(unsigned int starting_index, const void* memory, size_t count) {
				memcpy(buffer + starting_index, memory, sizeof(T) * count);
			}

			// it will not set the size and will not do a check
			void CopySlice(unsigned int starting_index, Stream<T> other) {
				CopySlice(starting_index, other.buffer, other.size);
			}

			// it will not set the size and will not do a check
			void CopySlice(unsigned int starting_index, CapacityStream<T> other) {
				CopySlice(starting_index, other.buffer, other.size);
			}

			void CopyTo(void* memory) const {
				memcpy(memory, buffer, sizeof(T) * size);
			}

			void CopyTo(uintptr_t& memory) const {
				memcpy((void*)memory, buffer, sizeof(T) * size);
				memory += sizeof(T) * size;
			}

			void PushDownElements(unsigned int starting_index, unsigned int count) {
				//ECS_ASSERT(count + size <= starting_index);
				for (int32_t index = (int32_t)size - 1; index >= (int32_t)starting_index; index--) {
					buffer[index + count] = buffer[index];
				}
			}

			void Reset() {
				size = 0;
			}

			void Remove(unsigned int index) {
				for (size_t copy_index = index + 1; copy_index < size; copy_index++) {
					buffer[copy_index - 1] = buffer[copy_index];
				}
				size--;
			}

			void RemoveSwapBack(unsigned int index) {
				size--;
				buffer[index] = buffer[size];
			}

			void Swap(unsigned int first, unsigned int second) {
				T copy = buffer[first];
				buffer[first] = buffer[second];
				buffer[second] = copy;
			}

			void SwapContents() {
				for (size_t index = 0; index < size >> 1; index++) {
					Swap(index, size - index - 1);
				}
			}

			ECS_INLINE T& operator [](size_t index) {
				return buffer[index];
			}

			ECS_INLINE const T& operator [](size_t index) const {
				return buffer[index];
			}

			const void* GetAllocatedBuffer() const {
				return buffer;
			}

			static size_t MemoryOf(size_t number) {
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
			void InitializeAndCopy(uintptr_t& buffer, CapacityStream<T> other) {
				InitializeFromBuffer(buffer, other.size);
				Copy(other);
			}

			template<typename Allocator>
			void Initialize(Allocator* allocator, unsigned int _size, unsigned int _capacity) {
				size_t memory_size = MemoryOf(_capacity);
				void* allocation = allocator->Allocate(memory_size, alignof(T));
				InitializeFromBuffer(allocation, _size, _capacity);
			}

			T* buffer;
			unsigned int size;
			unsigned int capacity;
		};

		template<typename T, typename Allocator, bool zero_memory = false>
		struct ResizableStream {
			ResizableStream() : buffer(nullptr), allocator(nullptr), capacity(0), size(0) {}
			ResizableStream(Allocator* _allocator, unsigned int _capacity) : allocator(_allocator), capacity(_capacity), size(0) {
				if (_capacity != 0) {
					buffer = (T*)allocator->Allocate(sizeof(T) * _capacity, alignof(T));

					if constexpr (zero_memory) {
						memset(buffer, 0, sizeof(T) * _capacity);
					}
				}
				else {
					buffer = nullptr;
				}
			}

			ResizableStream(const ResizableStream<T, Allocator, zero_memory>& other) = default;
			ResizableStream<T, Allocator, zero_memory>& operator = (const ResizableStream<T, Allocator, zero_memory>& other) = default;

			unsigned int Add(T element) {
				if (size == capacity) {
					Resize(static_cast<unsigned int>(ECS_RESIZABLE_STREAM_FACTOR * capacity + 1));
				}
				buffer[size++] = element;
				return size - 1;
			}

			unsigned int Add(const T* element) {
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

			// Returns the first index
			unsigned int AddStream(CapacityStream<T> other) {
				ReserveNewElements(other.size);
				CopySlice(size, other);
				unsigned int previous_size = size;
				size += other.size;
				return previous_size;
			}

			// it will set the size
			void Copy(const void* memory, size_t count) {
				ResizeNoCopy(count);
				memcpy(buffer, memory, sizeof(T) * count);
				size = count;
			}

			// it will set the size
			void Copy(Stream<T> other) {
				Copy(other.buffer, other.size);
			}

			// it will set the size
			void Copy(CapacityStream<T> other) {
				Copy(other.buffer, other.size);
			}

			// it will not set the size and will not do a check
			void CopySlice(unsigned int starting_index, const void* memory, size_t count) {
				memcpy(buffer + starting_index, memory, sizeof(T) * count);
			}

			// it will not set the size and will not do a check
			void CopySlice(unsigned int starting_index, Stream<T> other) {
				CopySlice(starting_index, other.buffer, other.size);
			}

			// it will not set the size and will not do a check
			void CopySlice(unsigned int starting_index, CapacityStream<T> other) {
				CopySlice(starting_index, other.buffer, other.size);
			}

			void CopyTo(void* memory) const {
				memcpy(memory, buffer, sizeof(T) * size);
			}

			void CopyTo(uintptr_t& memory) const {
				memcpy((void*)memory, buffer, sizeof(T) * size);
				memory += sizeof(T) * size;
			}

			void FreeBuffer() {
				if (buffer != nullptr) {
					allocator->Deallocate(buffer);
					buffer = nullptr;
					size = 0;
					capacity = 0;
				}
			}

			void PushDownElements(unsigned int starting_index, unsigned int count) {
				for (int32_t index = size - 1; index >= (int32_t)starting_index; index--) {
					buffer[index + count] = buffer[index];
				}
			}

			void Reset() {
				size = 0;
			}

			void Remove(size_t index) {
				for (size_t copy_index = index + 1; copy_index < size; copy_index++) {
					buffer[copy_index - 1] = buffer[copy_index];
				}
				size--;
			}

			void RemoveSwapBack(size_t index) {
				size--;
				buffer[index] = buffer[size];
			}

			// makes sure there is enough space for extra count elements
			void ReserveNewElements(size_t count) {
				if (size + count > capacity) {
					Resize(static_cast<unsigned int>(ECS_RESIZABLE_STREAM_FACTOR * capacity + 1));
				}
			}

			unsigned int ReserveNewElement() {
				unsigned int index = size;
				ReserveNewElements(1);
				return index;
			}

			void Resize(size_t new_capacity) {
				T* new_buffer = (T*)allocator->Allocate(new_capacity * sizeof(T), alignof(T));
				ECS_ASSERT(new_buffer != nullptr);

				if constexpr (zero_memory) {
					memset(new_buffer, 0, sizeof(T) * new_capacity);
				}

				memcpy(new_buffer, buffer, sizeof(T) * size);

				if (buffer != nullptr)
					allocator->Deallocate(buffer);

				buffer = new_buffer;
				capacity = new_capacity;
			}

			void ResizeNoCopy(size_t new_capacity) {
				T* new_buffer = (T*)allocator->Allocate(new_capacity * sizeof(T), alignof(T));
				ECS_ASSERT(new_buffer != nullptr);

				if constexpr (zero_memory) {
					memset(new_buffer, 0, sizeof(T) * new_capacity);
				}
				if (buffer != nullptr)
					allocator->Deallocate(buffer);

				buffer = new_buffer;
				capacity = new_capacity;
			}

			void Swap(unsigned int first, unsigned int second) {
				T copy = buffer[first];
				buffer[first] = buffer[second];
				buffer[second] = copy;
			}

			void SwapContents() {
				for (size_t index = 0; index < size >> 1; index++) {
					Swap(index, size - index - 1);
				}
			}

			void Trim() {
				T* allocation = (T*)allocator->Allocate(sizeof(T) * size, alignof(T));
				ECS_ASSERT(allocation != nullptr);

				memcpy(allocation, buffer, sizeof(T) * size);
				allocator->Deallocate(buffer);
				buffer = allocation;
				capacity = size;
			}

			// it will leave another additional_elements over the current size
			void Trim(unsigned int additional_elements) {
				if (additional_elements < capacity - size) {
					size_t elements_to_copy = size + additional_elements;
					T* allocation = (T*)allocator->Allocate(sizeof(T) * elements_to_copy, alignof(T));
					ECS_ASSERT(allocation != nullptr);

					if constexpr (zero_memory) {
						memset(allocation + size, 0, sizeof(T) * additional_elements);
					}

					memcpy(allocation, buffer, elements_to_copy * sizeof(T));
					allocator->Deallocate(buffer);
					buffer = allocation;
					capacity = size + additional_elements;
				}
			}

			ECS_INLINE T& operator [](size_t index) {
				return buffer[index];
			}

			ECS_INLINE const T& operator [](size_t index) const {
				return buffer[index];
			}

			static size_t MemoryOf(size_t number) {
				return sizeof(T) * number;
			}

			void Initialize(Allocator* _allocator, size_t _capacity) {
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

			T* buffer;
			Allocator* allocator;
			unsigned int size;
			unsigned int capacity;
		};

		template <>
		struct ECSENGINE_API Stream<void>
		{
			Stream() : buffer(nullptr), size(0) {}
			Stream(const void* _buffer, size_t _size) : buffer((void*)_buffer), size(_size) {}

			template<typename T>
			Stream(Stream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}

			template<typename T>
			Stream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)) {}

			Stream(const Stream& other) = default;
			Stream<void>& operator = (const Stream<void>& other) = default;

			void Add(Stream<void> other) {
				memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
				size += other.size;
			}

			// it will set the size
			void Copy(const void* memory, unsigned long long memory_size) {
				memcpy(buffer, memory, memory_size);
				size = memory_size;
			}

			// it will not set the size
			void CopySlice(size_t offset, const void* memory, unsigned long long memory_size) {
				memcpy((void*)((unsigned long long)buffer + offset), memory, memory_size);
			}

			void CopyTo(void* memory) const {
				memcpy(memory, buffer, size);
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
			CapacityStream() : buffer(nullptr), size(0), capacity(0) {}
			CapacityStream(const void* _buffer, unsigned int _size, unsigned int _capacity) : buffer((void*)_buffer), size(_size), capacity(_capacity) {}

			template<typename T>
			CapacityStream(CapacityStream<T> other) : buffer(other.buffer), size(other.size * sizeof(T)), capacity(other.capacity * sizeof(T)) {}

			CapacityStream(const CapacityStream& other) = default;
			CapacityStream<void>& operator = (const CapacityStream<void>& other) = default;

			void Add(Stream<void> other) {
				ECS_ASSERT(size + other.size < capacity);
				memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
				size += other.size;
			}

			void Add(CapacityStream<void> other) {
				ECS_ASSERT(size + other.size < capacity);
				memcpy((void*)((uintptr_t)buffer + size), other.buffer, other.size);
				size += other.size;
			}

			// it will set the size
			void Copy(const void* memory, unsigned long long memory_size) {
				ECS_ASSERT(memory_size <= capacity);
				memcpy(buffer, memory, memory_size);
				size = memory_size;
			}

			// it will not set the size
			void CopySlice(size_t offset, const void* memory, unsigned long long memory_size) {
				memcpy((void*)((unsigned long long)buffer + offset), memory, memory_size);
			}

			void CopyTo(void* memory) const {
				memcpy(memory, buffer, size);
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

		template<typename T>
		struct StableReferenceStream {
			StableReferenceStream() {}
			StableReferenceStream(void* buffer, unsigned int _capacity) {
				InitializeFromBuffer(buffer, _capacity);
			}

			StableReferenceStream(const StableReferenceStream& other) = default;
			StableReferenceStream<T>& operator = (const StableReferenceStream<T>& other) = default;

			// returns a handle that can be used for the lifetime of the object to index it
			unsigned int Add(T element) {
				unsigned int remapping_value = remapping[size];
				buffer[remapping_value] = element;
				size++;
				return remapping_value;
			}

			// returns a handle that can be used for the lifetime of the object to index it
			unsigned int Add(const T* element) {
				unsigned int remapping_value = remapping[size];
				buffer[remapping_value] = *element;
				size++;
				return remapping_value;
			}

			// returns a handle that can be used for the lifetime of the object to index it
			bool AddSafe(T element, unsigned int& handle_value) {
				if (size == capacity) {
					return false;
				}
				handle_value = Add(element);
				return true;
			}

			unsigned int RemapValue(unsigned int index) const {
				return remapping[index];
			}

			// it will reset the mapping
			void Reset() {
				size = 0;
				for (size_t index = 0; index < capacity; index++) {
					remapping[index] = index;
				}
			}

			void Remove(unsigned int index) {
				for (size_t copy_index = index + 1; copy_index < size; copy_index++) {
					buffer[copy_index - 1] = buffer[copy_index];
					remapping[copy_index]--;
				}
				size--;
			}

			void RemoveSwapBack(unsigned int index) {
				size--;
				buffer[remapping[index]] = buffer[size];
				unsigned int index_remap = remapping[index];
				remapping[index] = remapping[size];
				remapping[size] = index_remap;
			}

			void Swap(unsigned int first, unsigned int second) {
				T copy = buffer[remapping[first]];
				buffer[remapping[first]] = buffer[remapping[second]];
				buffer[remapping[second]] = copy;
				unsigned int remap_value = remapping[first];
				remapping[first] = remapping[second];
				remapping[second] = remap_value;
			}

			ECS_INLINE const T& operator [](size_t index) const {
				return buffer[index];
			}

			ECS_INLINE T& operator [](size_t index) {
				return buffer[index];
			}

			static size_t MemoryOf(size_t count) {
				return sizeof(T) * count;
			}

			void InitializeFromBuffer(void* _buffer, unsigned int _capacity) {
				buffer = (T*)_buffer;
				capacity = _capacity;
				uintptr_t ptr = (uintptr_t)_buffer;
				ptr += sizeof(T) * capacity;
				remapping = (unsigned int*)ptr;
				for (size_t index = 0; index < capacity; index++) {
					remapping[index] = index;
				}
				size = 0;
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

			T* buffer;
			unsigned int* remapping;
			unsigned int size;
			unsigned int capacity;
		};

	}

	inline containers::Stream<char> ToStream(const char* string) {
		return containers::Stream<char>(string, strlen(string));
	}

	inline containers::Stream<wchar_t> ToStream(const wchar_t* string) {
		return containers::Stream<wchar_t>(string, wcslen(string));
	}

}

