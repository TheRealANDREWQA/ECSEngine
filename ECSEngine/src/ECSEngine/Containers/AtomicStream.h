#pragma once
#include "../Utilities/Assert.h"
#include "ecspch.h"
#include "Stream.h"

namespace ECSEngine {

	namespace containers {

		// Atomic adds and requests are atomic, the rest of operations are not - The equivalent of the CapacityStream<T>
		// Cannot create atomic streams with void as template argument - use char to express byte streams
		template<typename T>
		struct AtomicStream {
			AtomicStream() : buffer(nullptr), size(0) {}
			AtomicStream(const void* buffer, unsigned int size) : buffer((T*)buffer), size(size), capacity(size) {}
			AtomicStream(const void* buffer, unsigned int size, unsigned int capacity) : buffer((T*)buffer), size(size), capacity(capacity) {}
			AtomicStream(Stream<T> other) : buffer(other.buffer), size(other.size), capacity(other.size) {}

			AtomicStream(const AtomicStream& other) = default;
			AtomicStream<T>& operator = (const AtomicStream<T>&other) = default;

			unsigned int Add(T element) {
				unsigned int index = size.fetch_add(1, std::memory_order_acq_rel);
				ECS_ASSERT(index <= capacity);
				buffer[index] = element;
				return index;
			}

			unsigned int Add(const T* element) {
				unsigned int index = size.fetch_add(1, std::memory_order_acq_rel);
				ECS_ASSERT(index <= capacity);
				buffer[index] = *element;
				return index;
			}

			// Returns the first index
			unsigned int AddStream(Stream<T> other) {
				unsigned int previous_size = size.fetch_add(other.size, std::memory_order_acq_rel);
				CopySlice(previous_size, other);
				return previous_size;
			}

			// it will set the size
			void Copy(const void* memory, size_t count) {
				memcpy(buffer, memory, count * sizeof(T));
				size.store(count, std::memory_order_release);
			}

			// it will set the size
			void Copy(Stream<T> other) {
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

			void CopyTo(void* memory) const {
				memcpy(memory, buffer, sizeof(T) * size);
			}

			void CopyTo(uintptr_t& memory) const {
				unsigned int current_size = size.load(std::memory_order_acquire);
				memcpy((void*)memory, buffer, sizeof(T) * current_size);
				memory += sizeof(T) * current_size;
			}

			Stream<T> Request(unsigned int count) {
				unsigned int current_size = size.fetch_add(count, std::memory_order_acq_rel);
				return { buffer + current_size, count };
			}

			unsigned int RequestInt(unsigned int count){
				unsigned int current_size = size.fetch_add(count, std::memory_order_acq_rel);
				return current_size;
			}

			void Remove(unsigned int index) {
				unsigned int current_size = size.load(std::memory_order_acquire);
				for (size_t copy_index = index + 1; copy_index < current_size; copy_index++) {
					buffer[copy_index - 1] = buffer[copy_index];
				}
				size.fetch_sub(1, std::memory_order_relaxed);
			}

			void RemoveSwapBack(unsigned int index) {
				unsigned int current_size = size.fetch_sub(1, std::memory_order_relaxed);
				buffer[index] = buffer[current_size - 1];
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
				_buffer += sizeof(T) * _size;
				size = _size;
				capacity = _capacity;
			}

			void InitializeAndCopy(uintptr_t& buffer, Stream<T> other) {
				InitializeFromBuffer(buffer, other.size, other.size);
				Copy(other);
			}

			template<typename Allocator>
			void Initialize(Allocator * allocator, unsigned int _capacity) {
				size_t memory_size = MemoryOf(_capacity);
				void* allocation = allocator->Allocate(memory_size, alignof(T));
				buffer = (T*)allocation;
				size = 0;
				capacity = _capacity;
			}

			T* buffer;
			std::atomic<unsigned int> size;
			unsigned int capacity;
		};

	}

}