#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "Stream.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/FunctionInterfaces.h"

#define ECS_CIRCULAR_QUEUE_RESIZE_FACTOR 1.5f

namespace ECSEngine {

	// Does not check to see if it goes out of boundaries
	// It is already circular
	template<typename T>
	struct Queue {
	public:
		Queue() : m_queue(nullptr, 0, 0), m_first_item(0) {}
		Queue(void* buffer, size_t capacity) {
			InitializeFromBuffer(buffer, capacity);
		}

		Queue(const Queue& other) = default;
		Queue& operator = (const Queue& other) = default;

		ECS_INLINE unsigned int GetSize() const {
			return m_queue.size;
		}

		ECS_INLINE unsigned int GetCapacity() const {
			return m_queue.capacity;
		}

		ECS_INLINE CapacityStream<T>* GetQueue() {
			return &m_queue;
		}

		ECS_INLINE void CopyTo(void* memory) const {
			if (m_first_item + GetSize() <= GetCapacity()) {
				// Can memcpy directly
				memcpy(memory, m_queue.buffer + m_first_item, MemoryOf(GetSize()));
			}
			else {
				unsigned int first_copy_count = GetCapacity() - m_first_item;
				memcpy(memory, m_queue.buffer + m_first_item, MemoryOf(first_copy_count));
				unsigned int second_copy_count = GetSize() - first_copy_count;
				memcpy(memory, m_queue.buffer, MemoryOf(second_copy_count));
			}
		}

		ECS_INLINE bool Peek(T& element) const {
			if (m_queue.size > 0) {
				element = m_queue[m_first_item];
				return true;
			}
			else {
				return false;
			}
		}

		// Returns a pointer to the first element from the queue
		// Such that it can be modified inside the queue
		// Make sure that the size is greater than 0
		ECS_INLINE T* PeekIntrusive() const {
			return m_queue.buffer + m_first_item;
		}

		bool Pop(T& element) {
			if (m_queue.size > 0) {
				unsigned int element_index = m_first_item;
				m_first_item = m_first_item + 1 == m_queue.capacity ? 0 : m_first_item + 1;
				m_queue.size--;
				element = m_queue[element_index];
				return true;
			}
			return false;
		}

		void Push(T element) {
			unsigned int index = m_first_item + m_queue.size;
			bool is_greater = index >= m_queue.capacity;
			m_queue[is_greater ? index - m_queue.capacity : index] = element;
			if (m_queue.size == m_queue.capacity) {
				m_first_item = m_first_item + 1 == m_queue.capacity ? 0 : m_first_item + 1;
			}
			else {
				m_queue.size++;
			}
		}

		void Push(const T* element) {
			unsigned int index = m_first_item + m_queue.size;
			bool is_greater = index >= m_queue.capacity;
			m_queue[is_greater ? index - m_queue.capacity : index] = *element;
			if (m_queue.size == m_queue.capacity) {
				m_first_item = m_first_item + 1 == m_queue.capacity ? 0 : m_first_item + 1;
			}
			else {
				m_queue.size++;
			}
		}

		ECS_INLINE void Reset() {
			m_first_item = 0;
			m_queue.Clear();
		}

		ECS_INLINE void* GetAllocatedBuffer() const {
			return m_queue.buffer;
		}

		ECS_INLINE static size_t MemoryOf(size_t count) {
			return CapacityStream<T>::MemoryOf(count);
		}

		void InitializeFromBuffer(void* buffer, size_t capacity) {
			m_queue = CapacityStream<T>(buffer, 0, capacity);
			m_first_item = 0;
		}

		void InitializeFromBuffer(uintptr_t& buffer, size_t capacity) {
			InitializeFromBuffer((void*)buffer, capacity);
			buffer += MemoryOf(capacity);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, size_t capacity) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = allocator->Allocate(memory_size, alignof(T));
			InitializeFromBuffer(allocation, capacity);
		}

		void Initialize(AllocatorPolymorphic allocator, size_t capacity) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = Allocate(allocator, memory_size, alignof(T));
			InitializeFromBuffer(allocation, capacity);
		}

		// Return true in the functor to early exit, if desired. The functor takes as parameter a T or T&
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			unsigned int size = GetSize();
			unsigned int capacity = GetCapacity();
			unsigned int current_index = m_first_item;
			for (unsigned int index = 0; index < size; index++) {
				if constexpr (early_exit) {
					if (functor(m_queue[current_index])) {
						return true;
					}
				}
				else {
					functor(m_queue[current_index]);
				}
				current_index = current_index == capacity - 1 ? 0 : current_index + 1;
			}
			return false;
		}

		// CONST VARIANT
		// Return true in the functor to early exit, if desired. The functor takes as parameter a T or const T&
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) const {
			unsigned int size = GetSize();
			unsigned int capacity = GetCapacity();
			unsigned int current_index = m_first_item;
			for (unsigned int index = 0; index < size; index++) {
				if constexpr (early_exit) {
					if (functor(m_queue[current_index])) {
						return true;
					}
				}
				else {
					functor(m_queue[current_index]);
				}
				current_index = current_index == capacity - 1 ? 0 : current_index + 1;
			}
			return false;
		}

		CapacityStream<T> m_queue;
		unsigned int m_first_item;
	};

	template<typename T>
	struct ResizableQueue {
	public:
		ResizableQueue() : m_first_item(0) {}
		ResizableQueue(AllocatorPolymorphic allocator, size_t capacity) : m_queue(allocator, capacity), m_first_item(0) {}

		ResizableQueue(const ResizableQueue& other) = default;
		ResizableQueue& operator = (const ResizableQueue& other) = default;

		ECS_INLINE void FreeBuffer() {
			m_queue.FreeBuffer();
		}

		void CopyTo(uintptr_t& buffer) const {
			unsigned int last_index = m_first_item + GetSize();
			if (last_index < GetCapacity()) {
				memcpy((void*)buffer, m_queue.buffer + m_first_item, GetSize() * sizeof(T));
				buffer += GetSize() * sizeof(T);
			}
			else {
				unsigned int copy_size = 2 * GetCapacity() - last_index;
				memcpy((void*)buffer, m_queue.buffer + m_first_item, copy_size * sizeof(T));
				buffer += copy_size * sizeof(T);
				copy_size = GetCapacity() - copy_size;
				memcpy((void*)buffer, m_queue.buffer, copy_size * sizeof(T));
				buffer += copy_size * sizeof(T);
			}
		}

		ECS_INLINE unsigned int GetSize() const {
			return m_queue.size;
		}

		ECS_INLINE unsigned int GetCapacity() const {
			return m_queue.capacity;
		}

		ECS_INLINE ResizableStream<T>* GetQueue() {
			return &m_queue;
		}

		ECS_INLINE bool Peek(T& element) const {
			if (m_queue.size > 0) {
				element = m_queue[m_first_item];
				return true;
			}
			else {
				return false;
			}
		}

		// Returns a pointer to the first element from the queue
		// Such that it can be modified inside the queue
		// Make sure that the size is greater than 0
		ECS_INLINE T* PeekIntrusive() {
			return m_queue.buffer + m_first_item;
		}

		bool Pop(T& element) {
			if (m_queue.size > 0) {
				unsigned int element_index = m_first_item;
				m_first_item = m_first_item + 1 == m_queue.capacity ? 0 : m_first_item + 1;
				m_queue.size--;
				element = m_queue[element_index];
				return true;
			}
			return false;
		}

		void Push(T element) {
			if (m_queue.size == m_queue.capacity) {
				unsigned int old_capacity = m_queue.capacity;
				T* old_buffer = m_queue.buffer;
				unsigned int new_capacity = (unsigned int)((m_queue.capacity + 1) * ECS_CIRCULAR_QUEUE_RESIZE_FACTOR);
				void* new_buffer = Allocate(m_queue.allocator, m_queue.MemoryOf(new_capacity));
				m_queue.buffer = (T*)new_buffer;

				memcpy(m_queue.buffer, old_buffer + m_first_item, sizeof(T) * (old_capacity - m_first_item));
				memcpy(m_queue.buffer + old_capacity - m_first_item, old_buffer, sizeof(T) * m_first_item);

				if (old_buffer != nullptr && m_queue.capacity > 0) {
					Deallocate(m_queue.allocator, old_buffer);
				}
				m_queue.capacity = new_capacity;

				m_first_item = 0;
				m_queue.Add(element);
			}
			else {
				unsigned int index = m_first_item + m_queue.size;
				bool is_greater = index >= m_queue.capacity;
				m_queue[is_greater ? index - m_queue.capacity : index] = element;
				m_queue.size++;
			}
		}

		void Push(const T* element) {
			if (m_queue.size == m_queue.capacity) {
				unsigned int old_capacity = m_queue.capacity;
				T* old_buffer = m_queue.buffer;
				unsigned int new_capacity = (unsigned int)((m_queue.capacity + 1) * ECS_CIRCULAR_QUEUE_RESIZE_FACTOR);
				void* new_buffer = Allocate(m_queue.allocator, m_queue.MemoryOf(new_capacity));
				m_queue.buffer = (T*)new_buffer;

				memcpy(m_queue.buffer, old_buffer + m_first_item, sizeof(T) * (old_capacity - m_first_item));
				memcpy(m_queue.buffer + old_capacity - m_first_item, old_buffer, sizeof(T) * m_first_item);

				if (old_buffer != nullptr && m_queue.capacity > 0) {
					Deallocate(m_queue.allocator, old_buffer);
				}
				m_queue.capacity = new_capacity;

				m_first_item = 0;
				m_queue.Add(element);
			}
			else {
				unsigned int index = m_first_item + m_queue.size;
				bool is_greater = index >= m_queue.capacity;
				m_queue[is_greater ? index - m_queue.capacity : index] = *element;
				m_queue.size++;
			}
		}

		ECS_INLINE void Reset() {
			m_first_item = 0;
			m_queue.Reset();
		}

		ECS_INLINE void ResizeNoCopy(unsigned int capacity) {
			m_first_item = 0;
			m_queue.ResizeNoCopy(capacity);
		}

		ECS_INLINE const void* GetAllocatedBuffer() const {
			return m_queue.buffer;
		}

		// Return true in the functor to early exit, if desired. The functor takes as parameter a T or T&
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			unsigned int size = GetSize();
			unsigned int capacity = GetCapacity();
			unsigned int current_index = m_first_item;
			for (unsigned int index = 0; index < size; index++) {
				if constexpr (early_exit) {
					if (functor(m_queue[current_index])) {
						return true;
					}
				}
				else {
					functor(m_queue[current_index]);
				}
				current_index = current_index == capacity - 1 ? 0 : current_index + 1;
			}
			return false;
		}

		// CONST VARIANT
		// Return true in the functor to early exit, if desired. The functor takes as parameter a T or const T&
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) const {
			unsigned int size = GetSize();
			unsigned int capacity = GetCapacity();
			unsigned int current_index = m_first_item;
			for (unsigned int index = 0; index < size; index++) {
				if constexpr (early_exit) {
					if (functor(m_queue[current_index])) {
						return true;
					}
				}
				else {
					functor(m_queue[current_index]);
				}
				current_index = current_index == capacity - 1 ? 0 : current_index + 1;
			}
			return false;
		}

		ResizableStream<T> m_queue;
		unsigned int m_first_item;
	};

	// Thread-safe queue, not suited to high contention
	template<typename T>
	struct ThreadSafeQueue
	{
	public:
		ThreadSafeQueue() : m_queue(nullptr, 0) {}

		ThreadSafeQueue(void* allocation, size_t capacity) : m_queue(allocation, capacity) {}

		ThreadSafeQueue(const ThreadSafeQueue& other) = default;
		ThreadSafeQueue& operator = (const ThreadSafeQueue& other) = default;

		ECS_INLINE void Reset() {
			m_queue.Reset();
		}

		ECS_INLINE unsigned int GetSize() const {
			return m_queue.GetSize();
		}

		ECS_INLINE const Queue<T>* GetQueue() const {
			return &m_queue;
		}

		ECS_INLINE size_t GetCapacity() const {
			return m_queue.GetCapacity();
		}

		ECS_INLINE void CopyTo(void* memory) const {
			m_queue.CopyTo(memory);
		}

		ECS_INLINE void Lock() {
			m_lock.lock();
		}

		bool Pop(T& element) {
			Lock();
			bool succeded = PopNonAtomic(element);
			Unlock();
			return succeded;
		}

		ECS_INLINE bool PopNonAtomic(T& element) {
			return m_queue.Pop(element);
		}

		void Push(T element) {
			Lock();
			PushNonAtomic(element);
			Unlock();
		}

		void Push(const T* element) {
			Lock();
			PushNonAtomic(element);
			Unlock();
		}

		// Returns true if it could peek an element
		bool Peek(T& element) {
			Lock();
			bool has_element = PeekNonAtomic(element);
			Unlock();
		}

		ECS_INLINE void PushNonAtomic(T element) {
			ECS_ASSERT(m_queue.GetSize() < m_queue.GetCapacity());
			m_queue.Push(element);
		}

		ECS_INLINE void PushNonAtomic(const T* element) {
			ECS_ASSERT(m_queue.GetSize() < m_queue.GetCapacity());
			m_queue.Push(element);
		}

		ECS_INLINE bool PeekNonAtomic(T& element) const {
			return m_queue.Peek(element);
		}

		ECS_INLINE void Unlock() {
			m_lock.unlock();
		}

		ECS_INLINE bool TryLock() {
			return m_lock.try_lock();
		}

		// It has good sleep behaviour on many missed loads
		ECS_INLINE void SpinWaitElements() {
			if (m_queue.GetSize() == 0) {
				m_lock.wait_signaled();
			}
		}

		ECS_INLINE static size_t MemoryOf(size_t size) {
			return Queue<T>::MemoryOf(size);
		}

		void InitializeFromBuffer(void* buffer, size_t capacity) {
			m_queue.InitializeFromBuffer(buffer, capacity);
			m_lock.clear();
		}

		void InitializeFromBuffer(uintptr_t& buffer, size_t capacity) {
			InitializeFromBuffer((void*)buffer, capacity);
			buffer += MemoryOf(capacity);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, size_t capacity) {
			m_queue.Initialize(allocator, capacity);
			m_lock.clear();
		}

		void Initialize(AllocatorPolymorphic allocator, size_t capacity) {
			m_queue.Initialize(allocator, capacity);
			m_lock.clear();
		}

		// This can be used when on single thread - it performs no locking
		// Return true in the functor to early exit, if desired. The functor takes as parameter a T or T&
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			return m_queue.ForEach<early_exit>(functor);
		}

		// This can be used when on single thread - it performs no locking
		// CONST VARIANT
		// Return true in the functor to early exit, if desired. The functor takes as parameter a T or const T&
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		ECS_INLINE bool ForEach(Functor&& functor) const {
			return m_queue.ForEach<early_exit>(functor);
		}

		Queue<T> m_queue;
		SpinLock m_lock;
	};

}

