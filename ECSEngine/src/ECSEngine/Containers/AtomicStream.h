#pragma once
#include "../Utilities/Assert.h"
#include "Stream.h"
#include <atomic>

namespace ECSEngine {

#define ECS_STACK_ATOMIC_STREAM(type, name, capacity) type _##name[capacity]; AtomicStream<type> name(_##name, 0, capacity)

	// Atomic adds and requests are atomic, the rest of operations are not - The equivalent of the CapacityStream<T>
	// It has 2 atomic uints, one for the size, telling how many items have been written, and another one where allocations
	// for are being made
	// Cannot create atomic streams with void as template argument - use char to express byte streams
	template<typename T>
	struct AtomicStream {
		ECS_INLINE AtomicStream() : buffer(nullptr), size(0), capacity(0), write_index(0) {}
		ECS_INLINE AtomicStream(const void* buffer, unsigned int size) : buffer((T*)buffer), size(size), capacity(size), write_index(size) {}
		ECS_INLINE AtomicStream(const void* buffer, unsigned int size, unsigned int capacity) : buffer((T*)buffer), size(size), capacity(capacity), write_index(size) {}
		ECS_INLINE AtomicStream(Stream<T> other) : buffer(other.buffer), size(other.size), capacity(other.size), write_index(other.size) {}

		ECS_INLINE AtomicStream(const AtomicStream<T>& other) {
			memcpy(this, &other, sizeof(*this));
		}

		ECS_INLINE AtomicStream<T>& operator = (const AtomicStream<T>& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}

		unsigned int Add(T element) {
			unsigned int index = write_index.fetch_add(1, std::memory_order_acquire);
			ECS_ASSERT(index <= capacity);
			buffer[index] = element;
			size.fetch_add(1, std::memory_order_release);
			return index;
		}

		unsigned int Add(const T* element) {
			unsigned int index = write_index.fetch_add(1, std::memory_order_acquire);
			ECS_ASSERT(index <= capacity);
			buffer[index] = *element;
			size.fetch_add(1, std::memory_order_release);
			return index;
		}

		// Returns the first index
		unsigned int AddStream(Stream<T> other) {
			unsigned int index = write_index.fetch_add(other.size, std::memory_order_acquire);
			ECS_ASSERT(index + other.size <= capacity);
			CopySlice(index, other);
			size.fetch_add(other.size, std::memory_order_release);
			return index;
		}

		unsigned int AddStreamClamped(Stream<T> other) {
			unsigned int index = write_index.fetch_add(other.size, std::memory_order_acquire);
			unsigned int copy_count = index + other.size > capacity ? capacity - index : other.size;
			CopySlice(index, other.buffer, copy_count);
			size.fetch_add(copy_count, std::memory_order_release);
			return index;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned int count) {
			memcpy(buffer, memory, count * sizeof(T));
			size.store(count, std::memory_order_relaxed);
			write_index.store(count, std::memory_order_relaxed);
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<T> other) {
			Copy(other.buffer, other.size);
		}

		// it will not set the size
		ECS_INLINE void CopySlice(unsigned int starting_index, const void* memory, unsigned int count) {
			memcpy(buffer + starting_index, memory, sizeof(T) * count);
		}

		// it will not set the size
		ECS_INLINE void CopySlice(unsigned int starting_index, Stream<T> other) {
			CopySlice(starting_index, other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			memcpy(memory, buffer, sizeof(T) * size.load(std::memory_order_relaxed));
		}

		ECS_INLINE void CopyTo(uintptr_t& memory) const {
			unsigned int current_size = size.load(std::memory_order_relaxed);
			memcpy((void*)memory, buffer, sizeof(T) * current_size);
			memory += sizeof(T) * current_size;
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
			if (buffer != nullptr && capacity > 0) {
				ECSEngine::Deallocate(allocator, buffer);
			}
		}

		ECS_INLINE void FinishRequest(unsigned int count) {
			size.fetch_add(count, std::memory_order_release);
		}

		// This only increments the write index, it does not advance the size value!!
		// Must be paired with a FinishRequest call with the number of values written
		ECS_INLINE Stream<T> Request(unsigned int count) {
			unsigned int index = write_index.fetch_add(count, std::memory_order_acquire);
			return { buffer + index, count };
		}

		// This only increments the write index, it does not advance the size value!!
		// Must be paired with a FinishRequest call with the number of values written
		// It will set the boolean flag to true if there is not enough space for the elements
		ECS_INLINE Stream<T> Request(unsigned int count, bool& not_enough_capacity) {
			Stream<T> request = Request(count);
			if (request.buffer + request.size > buffer + capacity) {
				not_enough_capacity = true;
			}
			else {
				not_enough_capacity = false;
			}
			return request;
		}

		// This only increments the write index, it does not advance the size value!!
		// Must be paired with a FinishRequest call with the number of values written
		ECS_INLINE unsigned int RequestInt(unsigned int count) {
			return write_index.fetch_add(count, std::memory_order_acquire);
		}

		ECS_INLINE void Reset() {
			size.store(0, std::memory_order_relaxed);
			write_index.store(0, std::memory_order_relaxed);
		}

		void Remove(unsigned int index) {
			unsigned int current_size = size.load(std::memory_order_acquire);
			for (size_t copy_index = index + 1; copy_index < current_size; copy_index++) {
				buffer[copy_index - 1] = buffer[copy_index];
			}
			size.fetch_sub(1, std::memory_order_relaxed);
			write_index.fetch_sub(1, std::memory_order_relaxed);
		}

		ECS_INLINE void RemoveSwapBack(unsigned int index) {
			unsigned int current_size = size.fetch_sub(1, std::memory_order_relaxed);
			write_index.fetch_sub(1, std::memory_order_relaxed);
			buffer[index] = buffer[current_size - 1];
		}

		ECS_INLINE void SetSize(unsigned int new_size) {
			size.store(new_size, std::memory_order_relaxed);
			write_index.store(new_size, std::memory_order_release);
		}

		// It will wait until all writes are commited into the stream
		// (i.e. size becomes equal to write index). Returns the size at that point
		ECS_INLINE unsigned int SpinWaitWrites() const {
			unsigned int write_count = write_index.load(std::memory_order_relaxed);
			SpinWait<'<'>(size, write_count);
			return write_count;
		}

		// It will wait until all writes up to the min between capacity and write index 
		// have been written to the stream. Returns the size at that point
		ECS_INLINE unsigned int SpinWaitCapacity() const {
			unsigned int write_count = write_index.load(std::memory_order_relaxed);
			unsigned int wait_count = min(write_count, capacity);
			SpinWait<'<'>(size, wait_count);
			return wait_count;
		}

		void Swap(unsigned int first, unsigned int second) {
			T copy = buffer[first];
			buffer[first] = buffer[second];
			buffer[second] = copy;
		}

		void SwapContents() {
			unsigned int current_size = size.load(std::memory_order_acquire);
			for (size_t index = 0; index < current_size >> 1; index++) {
				Swap(index, current_size - index - 1);
			}
		}
		
		Stream<T> ToStream() const {
			unsigned int current_size = SpinWaitWrites();
			return { buffer, current_size };
		}

		// It will atomically increment the size and if the size is still
		// in the bounds it will read that element. This only works if all 
		// the threads using this stream use this function to advance the size
		bool TryGet(T& type) {
			// Do a read first in order to avoid a fetch_add when not necessary
			unsigned int current_size = size.load(std::memory_order_relaxed);
			if (current_size < capacity) {
				// Do an increment now
				unsigned int index = size.fetch_add(1, std::memory_order_relaxed);
				// If the index is still in bounds, read the element
				if (index < capacity) {
					type = buffer[index];
					return true;
				}
			}
			return false;
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

		// Returns the size at the current moment - not the write index
		ECS_INLINE unsigned int Size() const {
			return size.load(ECS_RELAXED);
		}

		ECS_INLINE static size_t MemoryOf(size_t number) {
			return sizeof(T) * number;
		}

		void InitializeFromBuffer(void* _buffer, unsigned int _size, unsigned int _capacity) {
			buffer = (T*)_buffer;
			size.store(_size, std::memory_order_relaxed);
			write_index.store(_size, std::memory_order_relaxed);
			capacity = _capacity;
		}

		void InitializeFromBuffer(uintptr_t& _buffer, unsigned int _size, unsigned int _capacity) {
			buffer = (T*)_buffer;
			_buffer += sizeof(T) * _capacity;
			size.store(_size, std::memory_order_relaxed);
			write_index.store(_size, std::memory_order_relaxed);
			capacity = _capacity;
		}

		void InitializeAndCopy(uintptr_t& buffer, Stream<T> other) {
			InitializeFromBuffer(buffer, other.size, other.size);
			Copy(other);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(_capacity);
			void* allocation = allocator->Allocate(memory_size, alignof(T), debug_info);
			InitializeFromBuffer(allocation, 0, _capacity);
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			void* allocation = Allocate(allocator, MemoryOf(_capacity), alignof(T), debug_info);
			InitializeFromBuffer(allocation, 0, _capacity);
		}

		T* buffer;
		std::atomic<unsigned int> write_index;
		std::atomic<unsigned int> size;
		unsigned int capacity;
	};

	template<typename T>
	struct AdditionStreamAtomic {
		ECS_INLINE AdditionStreamAtomic() {}

		enum TYPE : unsigned char {
			CAPACITY,
			RESIZABLE,
			ATOMIC
		};

		ECS_INLINE unsigned int Add(T element) {
			if (IsCapacity()) {
				return capacity_stream.AddAssert(element);
			}
			else if (IsResizable()) {
				return resizable_stream.Add(element);
			}
			else {
				return atomic_stream.Add(element);
			}
		}

		ECS_INLINE unsigned int AddStream(Stream<T> elements) {
			if (IsCapacity()) {
				return capacity_stream.AddStreamAssert(elements);
			}
			else if (IsResizable()) {
				return resizable_stream.AddStream(elements);
			}
			else {
				return atomic_stream.AddStream(elements);
			}
		}

		ECS_INLINE bool IsCapacity() const {
			return tag == CAPACITY;
		}

		ECS_INLINE bool IsResizable() const {
			return tag == RESIZABLE;
		}

		ECS_INLINE bool IsAtomic() const {
			return tag == ATOMIC;
		}

		ECS_INLINE Stream<T> ToStream() const {
			if (IsCapacity()) {
				return capacity_stream.ToStream();
			}
			else if (IsResizable()) {
				return resizable_stream.ToStream();
			}
			else {
				return atomic_stream.ToStream();
			}
		}

		ECS_INLINE bool IsInitialized() const {
			if (IsCapacity()) {
				return capacity_stream.capacity > 0;
			}
			else if (IsResizable()) {
				return resizable_stream.allocator.allocator != nullptr;
			}
			else {
				return atomic_stream.capacity > 0;
			}
		}

		TYPE tag;
		union {
			CapacityStream<T> capacity_stream;
			ResizableStream<T> resizable_stream;
			AtomicStream<T> atomic_stream;
		};
	};

}