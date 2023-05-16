#pragma once
#include "ecspch.h"
#include "Stream.h"
#include "../Utilities/Assert.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

#define ECS_RESIZABLE_ATOMIC_STREAM_RESIZE_FACTOR (1.5f)

	// This is just a resizable stream with a lock on it. This is intended for low contention use cases.
	// We need a lock in place of an atomic variable because if a thread gets an index and then goes to sleep
	// and a resize is triggered immediately after, it can happen that the first thread is awaken in the middle 
	// of the resize and it might write into the old buffer instead of the new one being created. 
	// The only atomic operation is the addition!!! If you want other atomic operations you need to manually lock 
	// the lock and release when you are done.
	template<typename T>
	struct ResizableAtomicStream {
		ResizableAtomicStream() = default;
		ResizableAtomicStream(AllocatorPolymorphic allocator, unsigned int initial_capacity) : stream(allocator, initial_capacity) {}

		ResizableAtomicStream(const ResizableAtomicStream<T>& other) {
			memcpy(this, &other, sizeof(*this));
		}
		ResizableAtomicStream<T>& operator = (const ResizableAtomicStream<T>& other) {
			memcpy(this, &other, sizeof(*this));
		}

		// Thread safe
		unsigned int Add(T element) {
			lock.lock();
			unsigned int index = stream.Add(element);
			lock.unlock();
			return index;
		}

		// Thread safe
		unsigned int Add(const T* element) {
			lock.lock();
			unsigned int index = stream.Add(element);
			lock.unlock();
			return index;
		}

		// Thread safe
		unsigned int AddStream(Stream<T> other) {
			lock.lock();
			unsigned int index = stream.AddStream(other);
			lock.unlock();
			return index;
		}

		// it will set the size
		ECS_INLINE void Copy(const void* memory, unsigned int count) {
			stream.Copy(memory, count);
		}

		// it will set the size
		ECS_INLINE void Copy(Stream<T> other) {
			Copy(other.buffer, other.size);
		}

		// it will not set the size
		ECS_INLINE void CopySlice(unsigned int starting_index, const void* memory, unsigned int count) {
			stream.CopySlice(starting_index, memory, count);
		}

		// it will not set the size
		ECS_INLINE void CopySlice(unsigned int starting_index, Stream<T> other) {
			CopySlice(starting_index, other.buffer, other.size);
		}

		ECS_INLINE void CopyTo(void* memory) const {
			stream.CopyTo(memory);
		}

		ECS_INLINE void CopyTo(uintptr_t& memory) const {
			stream.CopyTo(memory);
		}

		ECS_INLINE void Reset() {
			stream.Reset();
		}

		void Resize(unsigned int new_capacity) {
			stream.Resize(new_capacity);
		}

		void Remove(unsigned int index) {
			stream.Remove(index);
		}

		ECS_INLINE void RemoveSwapBack(unsigned int index) {
			stream.RemoveSwapBack(index);
		}

		void Swap(unsigned int first, unsigned int second) {
			stream.Swap(first, second);
		}

		void SwapContents() {
			stream.SwapContents();
		}

		ECS_INLINE T& operator [](size_t index) {
			return stream[index];
		}

		ECS_INLINE const T& operator [](size_t index) const {
			return stream[index];
		}

		static size_t MemoryOf(size_t number) {
			return sizeof(T) * number;
		}

		void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity) {
			stream.Initialize(_allocator, _capacity);
			lock.unlock();
		}

		ResizableStream<T> stream;
		SpinLock lock;
	};

}