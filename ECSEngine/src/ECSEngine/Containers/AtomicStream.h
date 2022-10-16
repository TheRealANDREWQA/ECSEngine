#pragma once
#include "../Utilities/Assert.h"
#include "ecspch.h"
#include "Stream.h"

namespace ECSEngine {

	// Atomic adds and requests are atomic, the rest of operations are not - The equivalent of the CapacityStream<T>
	// It has 2 atomic uints, one for the size, telling how many items have been written, and another one where allocations
	// for are being made
	// Cannot create atomic streams with void as template argument - use char to express byte streams
	template<typename T>
	struct AtomicStream {
		AtomicStream() : buffer(nullptr), size(0), capacity(0), write_index(0) {}
		AtomicStream(const void* buffer, unsigned int size) : buffer((T*)buffer), size(size), capacity(size), write_index(size) {}
		AtomicStream(const void* buffer, unsigned int size, unsigned int capacity) : buffer((T*)buffer), size(size), capacity(capacity), write_index(size) {}
		AtomicStream(Stream<T> other) : buffer(other.buffer), size(other.size), capacity(other.size), write_index(other.size) {}

		AtomicStream(const AtomicStream<T>& other) {
			buffer = other.buffer;
			size.store(other.size.load(std::memory_order_relaxed), std::memory_order_relaxed);
			write_index.store(other.write_index.load(std::memory_order_relaxed), std::memory_order_relaxed);
			capacity = other.capacity;
		}

		AtomicStream<T>& operator = (const AtomicStream<T>& other) {
			buffer = other.buffer;
			size.store(other.size.load(std::memory_order_relaxed), std::memory_order_relaxed);
			write_index.store(other.write_index.load(std::memory_order_relaxed), std::memory_order_relaxed);
			capacity = other.capacity;

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

		void SetSize(unsigned int new_size) {
			size.store(new_size, std::memory_order_relaxed);
			write_index.store(new_size, std::memory_order_release);
		}

		// It will wait until all writes are commited into the stream
		// (i.e. size becomes equal to write index)
		void SpinWaitWrites() const {
			unsigned int write_count = write_index.load(ECS_RELAXED);
			SpinWait<'<'>(size, write_count);
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
			return { buffer, size.load(ECS_RELAXED) };
		}

		// It will atomically increment the size and if the size is still
		// in the bounds it will read that element. This only works if all 
		// the threads using this stream use this function to advance the size
		bool TryGet(T& type) {
			// Do a read first in order to avoid a fetch_add when not necessary
			unsigned int current_size = size.load(ECS_RELAXED);
			if (current_size < capacity) {
				// Do an increment now
				unsigned int index = size.fetch_add(1, ECS_RELAXED);
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

		static size_t MemoryOf(size_t number) {
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
		void Initialize(Allocator* allocator, unsigned int _capacity) {
			size_t memory_size = MemoryOf(_capacity);
			void* allocation = allocator->Allocate(memory_size, alignof(T));
			InitializeFromBuffer(allocation, 0, _capacity);
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int _capacity) {
			void* allocation = Allocate(allocator, MemoryOf(_capacity), alignof(T));
			InitializeFromBuffer(allocation, 0, _capacity);
		}

		T* buffer;
		std::atomic<unsigned int> write_index;
		std::atomic<unsigned int> size;
		unsigned int capacity;
	};

}