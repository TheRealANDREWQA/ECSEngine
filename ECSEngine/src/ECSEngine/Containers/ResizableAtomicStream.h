#pragma once
#include "AtomicStream.h"
#include "../Utilities/Assert.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Allocators/AllocatorTypes.h"

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
		ECS_INLINE ResizableAtomicStream() {}
		ECS_INLINE ResizableAtomicStream(AllocatorPolymorphic allocator, unsigned int initial_capacity) : stream(allocator, initial_capacity) {}

		ECS_INLINE ResizableAtomicStream(const ResizableAtomicStream<T>& other) {
			memcpy(this, &other, sizeof(*this));
		}
		ECS_INLINE ResizableAtomicStream<T>& operator = (const ResizableAtomicStream<T>& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}

		// Thread safe
		unsigned int Add(T element) {
			// Don't handle crashes here - it is too pedantic
			unsigned int write_position = stream.RequestInt(1);
			while (write_position > stream.capacity) {
				// Try to acquire the resize lock
				bool locked_by_us = lock.try_lock();
				if (locked_by_us) {
					// Wait for all writes to finish
					unsigned int capacity = stream.SpinWaitWrites();
					unsigned int new_capacity = (float)capacity * 1.5f + 16;
					void* allocation = AllocateEx(allocator, sizeof(T) * new_capacity);
					stream.CopyTo(allocation);
					stream.buffer = (T*)allocation;
					stream.capacity = new_capacity;
						
					// We must set this to false such that the outer __finally
					// Block won't unlock again
					locked_by_us = false;
					lock.unlock();
				}
				else {
					// Wait for the lock to be released
					lock.wait_locked();
				}
			}
			stream[write_position] = element;
			stream.FinishRequest(1);
			return write_position;
		}

		// Thread safe
		unsigned int AddStream(Stream<T> elements) {
			// Don't handle crashes here - it is too pedantic
			unsigned int write_position = write_position = stream.RequestInt(elements.size);
			while (write_position + elements.size > stream.capacity) {
				// Try to acquire the resize lock
				bool locked_by_us = lock.try_lock();
				if (locked_by_us) {
					// Wait for all writes to finish
					unsigned int capacity = stream.SpinWaitWrites();
					unsigned int new_capacity = (float)capacity * 1.5f + 16;
					new_capacity = new_capacity < capacity + elements.size ? capacity + elements.size + 16 : new_capacity;
					void* allocation = AllocateEx(allocator, sizeof(T) * new_capacity);
					ECS_HARD_ASSERT(allocation != nullptr, "Allocation failed for ResizableAtomicStream");
					stream.CopyTo(allocation);
					stream.buffer = (T*)allocation;
					stream.capacity = new_capacity;

					locked_by_us = false;
					// The unlock must be released in case of a crash
					lock.unlock();
				}
				else {
					// Wait for the lock to be released
					lock.wait_locked();
				}
			}
			elements.CopyTo(stream.buffer + write_position);
			stream.FinishRequest(elements.size);
			return write_position;
		}

		ECS_INLINE unsigned int AddNonAtomic(T element) {
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
		ECS_INLINE void Resize(unsigned int new_capacity, bool copy_old_data = true) {
			void* allocation = AllocateEx(new_capacity * sizeof(T));
			if (copy_old_data) {
				if (stream.capacity > 0) {
					stream.CopyTo(allocation);
				}
			}
			if (stream.capacity > 0) {
				DeallocateEx(allocator, stream.buffer);
			}
			stream.capacity = new_capacity;
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

		// Acquires the lock and waits for the writes to finish. You must release the lock
		// with WaitWritesExit
		ECS_INLINE unsigned int WaitWritesEnter() {
			lock.lock();
			// Check to see if we got the lock while a resize was attempted - in that case
			// We shouldn't wait for the writes to finish, instead read the entire contents as they are now
			if (stream.write_index.load(std::memory_order_relaxed) > stream.capacity) {
				// A resize is pending and we got the lock before them - we need to wait
				// until writes up to the capacity have been written
				return stream.SpinWaitCapacity();
			}
			// By default we should wait for all writes to finish now
			return stream.SpinWaitWrites();
		}

		ECS_INLINE void WaitWritesExit() {
			lock.unlock();
		}

		// You can use this in case you are sure that no writes are being made
		ECS_INLINE Stream<T> ToStream() const {
			return stream.ToStream();
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return stream[index];
		}

		ECS_INLINE const T& operator [](unsigned int index) const {
			return stream[index];
		}

		ECS_INLINE static size_t MemoryOf(size_t number) {
			return sizeof(T) * number;
		}

		ECS_INLINE void Initialize(AllocatorPolymorphic _allocator, unsigned int _capacity) {
			stream.Initialize(_allocator, _capacity);
			lock.unlock();
		}

		AtomicStream<T> stream;
		AllocatorPolymorphic allocator;
		// The lock is used to resize the stream
		// The actual additions are using atomic operations
		SpinLock lock;
	};

}