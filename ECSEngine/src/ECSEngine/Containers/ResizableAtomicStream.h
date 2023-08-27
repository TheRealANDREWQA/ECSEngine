#pragma once
#include "ecspch.h"
#include "Stream.h"
#include "../Utilities/Assert.h"
#include "../Multithreading/ConcurrentPrimitives.h"

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
		unsigned int AddStream(Stream<T> elements) {
			lock.lock();
			unsigned int index = stream.AddStream(elements);
			lock.unlock();
			return index;
		}

		ECS_INLINE unsigned int AddNonAtomic(T element) {
			return stream.Add(element);
		}

		ECS_INLINE unsigned int AddNonAtomic(const T* element) {
			return stream.Add(element);
		}

		ECS_INLINE unsigned int AddStreamNonAtomic(Stream<T> elements) {
			return stream.AddStream(elements);
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

		// It does not lock
		ECS_INLINE void Reset() {
			stream.Reset();
		}

		// It does not lock
		ECS_INLINE void Resize(unsigned int new_capacity) {
			stream.Resize(new_capacity);
		}

		// It does not lock
		ECS_INLINE void Remove(unsigned int index) {
			stream.Remove(index);
		}

		// It does not lock
		ECS_INLINE void RemoveSwapBack(unsigned int index) {
			stream.RemoveSwapBack(index);
		}

		// It does not lock
		ECS_INLINE void Swap(unsigned int first, unsigned int second) {
			stream.Swap(first, second);
		}

		// It does not lock
		ECS_INLINE void SwapContents() {
			stream.SwapContents();
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return stream[index];
		}

		ECS_INLINE const T& operator [](unsigned int index) const {
			return stream[index];
		}

		static size_t MemoryOf(size_t number) {
			return sizeof(T) * number;
		}

		ECS_INLINE void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity) {
			stream.Initialize(_allocator, _capacity);
			lock.unlock();
		}

		ResizableStream<T> stream;
		SpinLock lock;
	};

}