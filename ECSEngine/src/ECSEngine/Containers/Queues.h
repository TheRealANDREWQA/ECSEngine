#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "Stream.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/FunctionInterfaces.h"

#define ECS_CIRCULAR_QUEUE_RESIZE_FACTOR 1.5f

namespace ECSEngine {

	namespace containers {
		
		// does not check to see if it goes out of boundaries
		template<typename T>
		struct Queue {
		public:
			Queue() : m_queue(nullptr, 0, 0), m_first_item(0) {}
			Queue(void* buffer, size_t capacity) {
				InitializeFromBuffer(buffer, capacity);
			}

			Queue(const Queue& other) = default;

			Queue& operator = (const Queue& other) = default;

			unsigned int GetSize() const {
				return m_queue.size;
			}

			unsigned int GetCapacity() const {
				return m_queue.capacity;
			}

			CapacityStream<T>* GetQueue() const {
				return &m_queue;
			}

			bool Peek(T& element) const {
				if (m_queue.size > 0) {
					element = m_queue[m_first_item];
					return true;
				}
				else {
					return false;
				}
			}

			bool Pop(T& element) {
				if (m_queue.size > 0) {
					unsigned int element_index = m_first_item;
					m_first_item = function::PredicateValue<unsigned int>(m_first_item == m_queue.capacity - 1, 0, m_first_item + 1);
					m_queue.size--;
					element = m_queue[element_index];
					return true;
				}
				return false;
			}

			void Push(T element) {
				if (m_queue.size == m_queue.capacity) {
					unsigned int index = m_first_item + m_queue.size;
					bool is_greater = index >= m_queue.capacity;
					m_queue[function::PredicateValue(is_greater, index - m_queue.capacity, index)] = element;
					m_first_item = function::PredicateValue<unsigned int>(m_first_item == m_queue.capacity - 1, 0, m_first_item + 1);
				}
				else {
					unsigned int index = m_first_item + m_queue.size;
					bool is_greater = index >= m_queue.capacity;
					m_queue[function::PredicateValue(is_greater, index - m_queue.capacity, index)] = element;
					m_queue.size++;
				}
			}

			void Push(const T* element) {
				if (m_queue.size == m_queue.capacity) {
					unsigned int index = m_first_item + m_queue.size;
					bool is_greater = index >= m_queue.capacity;
					m_queue[function::PredicateValue(is_greater, index - m_queue.capacity, index)] = *element;
					m_first_item = function::PredicateValue<unsigned int>(m_first_item == m_queue.capacity - 1, 0, m_first_item + 1);
				}
				else {
					unsigned int index = m_first_item + m_queue.size;
					bool is_greater = index >= m_queue.capacity;
					m_queue[function::PredicateValue(is_greater, index - m_queue.capacity, index)] = *element;
					m_queue.size++;
				}
			}

			void Reset() {
				m_first_item = 0;
				m_queue.Reset();
			}

			void* GetAllocatedBuffer() const {
				return m_queue.buffer;
			}

			static size_t MemoryOf(size_t count) {
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

		//private:
			CapacityStream<T> m_queue;
			unsigned int m_first_item;
		};

		template<typename T, typename Allocator, bool zero_memory = false>
		struct ResizableQueue {
		public:
			ResizableQueue() : m_first_item(0) {}
			ResizableQueue(Allocator* allocator, size_t capacity) : m_queue(allocator, capacity), m_first_item(0) {}

			ResizableQueue(const ResizableQueue& other) = default;

			ResizableQueue& operator = (const ResizableQueue& other) = default;

			void FreeBuffer() {
				m_queue.FreeBuffer();
			}

			unsigned int GetSize() const {
				return m_queue.size;
			}

			unsigned int GetCapacity() const {
				return m_queue.capacity;
			}

			ResizableStream<T, Allocator, zero_memory>* GetQueue() const {
				return &m_queue;
			}

			bool Pop(T& element) {
				if (m_queue.size > 0) {
					unsigned int element_index = m_first_item;
					m_first_item = (m_first_item + 1) % m_queue.capacity;
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
					m_queue.FreeBuffer();
					m_queue.ResizeNoCopy(static_cast<size_t>((m_queue.capacity + 1) * ECS_CIRCULAR_QUEUE_RESIZE_FACTOR));
					memcpy(m_queue.buffer, old_buffer + m_first_item, sizeof(T) * (old_capacity - m_first_item));
					memcpy(m_queue.buffer + old_capacity - m_first_item, old_buffer, sizeof(T) * m_first_item);
					m_first_item = 0;
					m_queue[m_queue.size++] = element;
				}
				else {
					unsigned int index = m_first_item + m_queue.size;
					bool is_greater = index >= m_queue.capacity;
					m_queue[function::PredicateValue(is_greater, index - m_queue.capacity, index)] = element;
					m_queue.size++;
				}
			}

			void Push(const T* element) {
				if (m_queue.size == m_queue.capacity) {
					unsigned int old_capacity = m_queue.capacity;
					T* old_buffer = m_queue.buffer;
					m_queue.FreeBufferUnguarded();
					m_queue.ResizeNoCopy(static_cast<size_t>((m_queue.capacity + 1) * ECS_CIRCULAR_QUEUE_RESIZE_FACTOR));
					memcpy(m_queue.buffer, old_buffer + m_first_item, sizeof(T) * (old_capacity - m_first_item));
					memcpy(m_queue.buffer + old_capacity - m_first_item, old_buffer, sizeof(T) * m_first_item);
					m_first_item = 0;
					m_queue[m_queue.size++] = *element;
				}
				else {
					unsigned int index = m_first_item + m_queue.size;
					bool is_greater = index >= m_queue.capacity;
					m_queue[function::PredicateValue(is_greater, index - m_queue.capacity, index)] = *element;
					m_queue.size++;
				}
			}

			void Reset() {
				m_first_item = 0;
				m_queue.Reset();
			}

			const void* GetAllocatedBuffer() const {
				return m_queue.buffer;
			}

		//private:
			ResizableStream<T, Allocator, zero_memory> m_queue;
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
			ThreadSafeQueue(ThreadSafeQueue&& other) = default;

			ThreadSafeQueue& operator = (const ThreadSafeQueue& other) = default;
			ThreadSafeQueue& operator = (ThreadSafeQueue&& other) = default;

			void Reset() {
				m_queue.Reset();
			}

			unsigned int GetSize() const {
				return m_queue.GetSize();
			}

			const Queue<T>* GetQueue() const {
				return &m_queue;
			}

			size_t GetCapacity() const {
				return m_queue.GetCapacity();
			}

			void Lock() {
				m_lock.lock();
			}

			bool Pop(T& element) {
				m_lock.lock();
				bool succeded = PopNonAtomic(element);
				m_lock.unlock();
				return succeded;
			}

			bool PopNonAtomic(T& element) {
				return m_queue.Pop(element);
			}

			void Push(T element) {
				m_lock.lock();
				PushNonAtomic(element);
				m_lock.unlock();
			}

			void Push(const T* element) {
				m_lock.lock();
				PushNonAtomic(element);
				m_lock.unlock();
			}

			void PushNonAtomic(T element) {
				ECS_ASSERT(m_queue.GetSize() < m_queue.GetCapacity());
				m_queue.Push(element);
			}

			void PushNonAtomic(const T* element) {
				ECS_ASSERT(m_queue.GetSize() < m_queue.GetCapacity());
				m_queue.Push(element);
			}

			void Unlock() {
				m_lock.unlock();
			}

			bool TryLock() {
				return m_lock.try_lock();
			}

			static size_t MemoryOf(size_t size) {
				return Queue<T>::MemoryOf(size);
			}

			void InitializeFromBuffer(void* buffer, size_t capacity) {
				m_queue.InitializeFromBuffer(buffer, capacity);
				m_lock.value.store(false);
			}

			void InitializeFromBuffer(uintptr_t& buffer, size_t capacity) {
				InitializeFromBuffer((void*)buffer, capacity);
				buffer += MemoryOf(capacity);
			}

			template<typename Allocator>
			void Initialize(Allocator* allocator, size_t capacity) {
				m_queue.Initialize(allocator, capacity);
				m_lock.value.store(false);
			}

		//private:
			Queue<T> m_queue;
			SpinLock m_lock;
		};
	}
}

