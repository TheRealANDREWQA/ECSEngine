#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "Stream.h"
#include "../Multithreading/ConcurrentPrimitives.h"

#define ECS_CIRCULAR_QUEUE_RESIZE_FACTOR 1.5f

namespace ECSEngine {

	namespace Internal {
		template<typename QueueType>
		bool CompareQueue(const QueueType& first, const QueueType& second) {
			if (first.GetSize() != second.GetSize()) {
				return false;
			}

			const size_t element_size = sizeof(typename QueueType::T);

			unsigned int first_start_size = ClampMax(first.m_queue.size, first.m_queue.capacity - first.m_first_item);
			unsigned int first_end_size = ClampMax(first.m_queue.size - first_start_size, first.m_first_item);
			unsigned int second_start_size = ClampMax(second.m_queue.size, second.m_queue.capacity - second.m_first_item);
			unsigned int second_end_size = ClampMax(second.m_queue.size - second_start_size, second.m_first_item);

			unsigned int start_size_min = min(first_start_size, second_start_size);
			// Compare the start ranges
			if (memcmp(first.m_queue.buffer + first.m_first_item, second.m_queue.buffer + second.m_first_item, element_size * (size_t)start_size_min) != 0) {
				return false;
			}

			if (first_start_size < second_start_size) {
				// Must compare the start of this instance with still the first range
				unsigned int current_compare_count = second_start_size - first_start_size;
				if (memcmp(first.m_queue.buffer, second.m_queue.buffer + second.m_first_item + first_start_size, element_size * (size_t)current_compare_count) != 0) {
					return false;
				}

				// If there are still entries, compare the end ranges
				if (second_end_size > 0) {
					if (memcmp(first.m_queue.buffer + current_compare_count, second.m_queue.buffer, element_size * (size_t)second_end_size) != 0) {
						return false;
					}
				}
			}
			else if (first_start_size > second_start_size) {
				unsigned int compare_count = first_start_size - second_start_size;
				if (memcmp(first.m_queue.buffer + first.m_first_item + second_start_size, second.m_queue.buffer, element_size * (size_t)compare_count) != 0) {
					return false;
				}

				// If there are still entries, compare the end ranges
				if (first_end_size > 0) {
					if (memcmp(first.m_queue.buffer, second.m_queue.buffer + compare_count, element_size * (size_t)first_end_size) != 0) {
						return false;
					}
				}
			}
			else {
				// Both are equal, compare the end ranges
				if (memcmp(first.m_queue.buffer, second.m_queue.buffer, element_size * (size_t)second_end_size) != 0) {
					return false;
				}
			}

			return true;
		}
	}

	// Does not check to see if it goes out of boundaries
	// It is already circular
	template<typename T>
	struct Queue {
		typedef T T;

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

		ECS_INLINE bool Compare(const Queue<T>& other) const {
			return Internal::CompareQueue(*this, other);
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

		// The index is counted backwards - that is, index 0 means the first element
		// to be popped, while index size - 1 means the last added element
		T PeekByIndex(unsigned int index) const {
			unsigned int final_index = index + m_first_item;
			final_index = final_index >= m_queue.capacity ? final_index - m_queue.capacity : final_index;
			return m_queue[final_index];
		}

		// The index is counted "normally" - that is, index 0 means the last pushed element
		// while index 0 means the first element to be popped
		T PushPeekByIndex(unsigned int index) const {
			unsigned int final_index = m_first_item + m_queue.size - 1 - index;
			final_index = final_index >= m_queue.capacity ? final_index - m_queue.capacity : final_index;
			return m_queue[final_index];
		}

		// The index is counted "normally" - that is, index 0 means the last pushed element
		// while index 0 means the first element to be popped
		T* PushPeekByIndexIntrusive(unsigned int index) {
			unsigned int final_index = m_first_item + m_queue.size - 1 - index;
			final_index = final_index >= m_queue.capacity ? final_index - m_queue.capacity : final_index;
			return &m_queue[final_index];

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

		// Pops multiple elements at once from the queue
		// The count must be smaller or equal to the size
		void PopRange(T* elements, unsigned int count) {
			ECS_ASSERT(count <= GetSize());

			if (m_first_item + count <= m_queue.capacity) {
				// Can memcpy once
				memcpy(elements, m_queue.buffer + m_first_item, sizeof(T) * count);
			}
			else {
				unsigned int first_copy_count = m_queue.capacity - m_first_item;
				// Need 2 memcpy's
				memcpy(elements, m_queue.buffer + m_first_item, sizeof(T) * first_copy_count);
				memcpy(elements + first_copy_count, m_queue.buffer, (count - first_copy_count) * sizeof(T));
			}
			m_queue.size -= count;
		}

	private:
		template<typename ExtractValue>
		ECS_INLINE void PushImpl(ExtractValue&& extract_value) {
			unsigned int index = m_first_item + m_queue.size;
			bool is_greater = index >= m_queue.capacity;
			m_queue[is_greater ? index - m_queue.capacity : index] = extract_value();
			if (m_queue.size == m_queue.capacity) {
				m_first_item = m_first_item + 1 == m_queue.capacity ? 0 : m_first_item + 1;
			}
			else {
				m_queue.size++;
			}
		}

	public:
		void Push(T element) {
			PushImpl([&]() { return element; });
		}

		void Push(const T* element) {
			PushImpl([&]() { return *element; });
		}

		void PushRange(Stream<T> elements) {
			if (elements.size == 0) {
				return;
			}

			if (elements.size <= m_queue.capacity) {
				if (elements.size + m_first_item + m_queue.size <= m_queue.capacity) {
					// Single memcpy
					memcpy(m_queue.buffer + m_first_item + m_queue.size, elements.buffer, elements.MemoryOf(elements.size));
				}
				else {
					if (m_queue.capacity > m_first_item + m_queue.size) {
						// Double memcpy
						unsigned int elements_to_copy = m_queue.capacity - m_first_item - m_queue.size;
						memcpy(m_queue.buffer + m_first_item + m_queue.size, elements.buffer, elements.MemoryOf(elements_to_copy));
						memcpy(m_queue.buffer, elements.buffer + elements_to_copy, elements.MemoryOf(elements.size - elements_to_copy));
					}
					else {
						unsigned int initial_offset = m_first_item + m_queue.size - m_queue.capacity;
						if (initial_offset + elements.size <= m_queue.capacity) {
							// Single memcpy
							memcpy(m_queue.buffer + initial_offset, elements.buffer, elements.MemoryOf(elements.size));
						}
						else {
							unsigned int first_write_count = m_queue.capacity - initial_offset;
							memcpy(m_queue.buffer + initial_offset, elements.buffer, elements.MemoryOf(first_write_count));
							memcpy(m_queue.buffer, elements.buffer + first_write_count, elements.MemoryOf(elements.size - first_write_count));					
						}
					}
					unsigned int overwritten_elements = elements.size + m_first_item + m_queue.size - m_queue.capacity;
					m_first_item += overwritten_elements;
					m_first_item = m_first_item >= m_queue.capacity ? m_first_item - m_queue.capacity : m_first_item;
				}
				m_queue.size = ClampMax<unsigned int>(elements.size + m_queue.size, m_queue.capacity);
			}
			else {
				// Since there are more elements than in the entire capacity,
				// We can reset the first elements to 0 and copy the last capacity elements into the beginning
				unsigned int offset = elements.size - m_queue.capacity;
				memcpy(m_queue.buffer, elements.buffer + offset, elements.MemoryOf(m_queue.capacity));
				m_first_item = 0;
				m_queue.size = m_queue.capacity;
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
		void Initialize(Allocator* allocator, size_t capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = allocator->Allocate(memory_size, alignof(T), debug_info);
			InitializeFromBuffer(allocation, capacity);
		}

		void Initialize(AllocatorPolymorphic allocator, size_t capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = Allocate(allocator, memory_size, alignof(T), debug_info);
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

		template<bool early_exit = false, typename Functor>
		bool ForEachRange(unsigned int starting_index, unsigned int ending_index, Functor&& functor) {
			unsigned int difference = ending_index - starting_index;
			unsigned int capacity = GetCapacity();
			unsigned int current_index = m_first_item + starting_index;
			current_index = current_index >= capacity ? current_index - capacity : current_index;
			for (unsigned int index = 0; index < difference; index++) {
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

		template<bool early_exit = false, typename Functor>
		bool ForEachRange(unsigned int starting_index, unsigned int ending_index, Functor&& functor) const {
			unsigned int difference = ending_index - starting_index;
			unsigned int capacity = GetCapacity();
			unsigned int current_index = m_first_item + starting_index;
			current_index = current_index >= capacity ? current_index - capacity : current_index;
			for (unsigned int index = 0; index < difference; index++) {
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
		typedef T T;

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

		// Returns true if both queue have the same content, else false
		ECS_INLINE bool Compare(const ResizableQueue<T>& other) const {
			return Internal::CompareQueue(*this, other);
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

		// The index is counted backwards - that is, index 0 means the first element
		// to be popped, while index size - 1 means the last added element
		T PeekByIndex(unsigned int index) const {
			unsigned int final_index = index + m_first_item;
			final_index = final_index >= m_queue.capacity ? final_index - m_queue.capacity : final_index;
			return m_queue[final_index];
		}

		// The index is counted "normally" - that is, index 0 means the last pushed element
		// while index 0 means the first element to be popped
		T PushPeekByIndex(unsigned int index) const {
			unsigned int final_index = m_first_item + m_queue.size - 1 - index;
			final_index = final_index >= m_queue.capacity ? final_index - m_queue.capacity : final_index;
			return m_queue[final_index];
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

		// Pops multiple elements at once from the queue
		// The count must be smaller or equal to the size
		void PopRange(T* elements, unsigned int count) {
			ECS_ASSERT(count <= GetSize());

			if (m_first_item + count <= m_queue.capacity) {
				// Can memcpy once
				memcpy(elements, m_queue.buffer + m_first_item, sizeof(T) * count);
			}
			else {
				unsigned int first_copy_count = m_queue.capacity - m_first_item;
				// Need 2 memcpy's
				memcpy(elements, m_queue.buffer + m_first_item, sizeof(T) * first_copy_count);
				memcpy(elements + first_copy_count, m_queue.buffer, (count - first_copy_count) * sizeof(T));
			}
			m_queue.size -= count;
		}

	private:
		template<typename Functor>
		void PushImpl(DebugInfo debug_info, Functor&& extract_value) {
			if (m_queue.size == m_queue.capacity) {
				unsigned int new_capacity = (unsigned int)((m_queue.capacity + 1) * ECS_CIRCULAR_QUEUE_RESIZE_FACTOR);
				Resize(new_capacity, debug_info);
				m_queue.Add(extract_value());
			}
			else {
				unsigned int index = m_first_item + m_queue.size;
				bool is_greater = index >= m_queue.capacity;
				m_queue[is_greater ? index - m_queue.capacity : index] = extract_value();
				m_queue.size++;
			}
		}

	public:
		void Push(T element, DebugInfo debug_info = ECS_DEBUG_INFO) {
			return PushImpl(debug_info, [&]() { return element; });
		}

		void Push(const T* element, DebugInfo debug_info = ECS_DEBUG_INFO) {
			return PushImpl(debug_info, [&]() { return *element; });
		}

		void PushRange(Stream<T> elements, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (elements.size == 0) {
				return;
			}

			if (m_queue.size + elements.size <= m_queue.capacity) {
				if (m_queue.size + m_first_item + elements.size <= m_queue.capacity) {
					// One memcpy
					memcpy(m_queue.buffer + m_first_item + m_queue.size, elements.buffer, elements.MemoryOf(elements.size));
				}
				else {
					if (m_queue.size + m_first_item < m_queue.capacity) {
						// Two memcpy's
						unsigned int first_write_count = m_queue.capacity - m_first_item - m_queue.size;
						memcpy(m_queue.buffer + m_first_item + m_queue.size, elements.buffer, elements.MemoryOf(first_write_count));
						memcpy(m_queue.buffer, elements.buffer + first_write_count, elements.MemoryOf(elements.size - first_write_count));
					}
					else {
						// One memcpy
						unsigned int initial_offset = m_first_item + m_queue.size - m_queue.capacity;
						memcpy(m_queue.buffer + initial_offset, elements.buffer, elements.MemoryOf(elements.size));
					}
				}
				m_queue.size += elements.size;
			}
			else {
				// We need to resize - increase the amount by a small factor
				// Such as to prevent many small resizes
				Resize((m_queue.size + elements.size) * 1.2f);
				// We can now add the elements as in a stream
				m_queue.AddStream(elements);
			}
		}

		ECS_INLINE void Reset() {
			m_first_item = 0;
			m_queue.Clear();
		}

		ECS_INLINE void ResizeNoCopy(unsigned int capacity) {
			m_first_item = 0;
			m_queue.ResizeNoCopy(capacity);
		}

		void Resize(unsigned int new_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			unsigned int old_capacity = m_queue.capacity;
			T* old_buffer = m_queue.buffer;
			void* new_buffer = Allocate(m_queue.allocator, m_queue.MemoryOf(new_capacity), alignof(T), debug_info);
			m_queue.buffer = (T*)new_buffer;

			memcpy(m_queue.buffer, old_buffer + m_first_item, sizeof(T) * (old_capacity - m_first_item));
			memcpy(m_queue.buffer + old_capacity - m_first_item, old_buffer, sizeof(T) * m_first_item);

			if (old_buffer != nullptr && m_queue.capacity > 0) {
				Deallocate(m_queue.allocator, old_buffer, debug_info);
			}
			m_queue.capacity = new_capacity;
			m_first_item = 0;
		}

		ECS_INLINE const void* GetAllocatedBuffer() const {
			return m_queue.buffer;
		}

		ECS_INLINE void Initialize(AllocatorPolymorphic allocator, unsigned int initial_capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			m_queue.Initialize(allocator, initial_capacity, debug_info);
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

	template<typename T, typename UnderlyingQueue>
	struct ThreadSafeQueueAdapter
	{
		ECS_INLINE ThreadSafeQueueAdapter() {}

		ThreadSafeQueueAdapter(const ThreadSafeQueueAdapter& other) = default;
		ThreadSafeQueueAdapter& operator = (const ThreadSafeQueueAdapter& other) = default;

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

		ECS_INLINE unsigned int GetSizeAtomic() {
			Lock();
			unsigned int size = GetSize();
			Unlock();
			return size;
		}

		ECS_INLINE void CopyTo(void* memory) const {
			m_queue.CopyTo(memory);
		}

		ECS_INLINE void Lock() {
			m_lock.Lock();
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

		// Pops multiple elements at once from the queue
		// The count must be smaller or equal to the size
		ECS_INLINE void PopRangeNonAtomic(T* elements, unsigned int count) {
			m_queue.PopRange(elements, count);
		}

		ECS_INLINE void PopRange(T* elements, unsigned int count) {
			Lock();
			PopRangeNonAtomic(elements, count);
			Unlock();
		}

		// Pops all the elements from the queue into the elements
		// Basically combines a GetSizeAtomic() with PopRange
		// Returns the number of entries popped
		unsigned int PopRangeAll(AdditionStream<T> elements) {
			Lock();
			unsigned int size = GetSize();
			T* elements_to_write = elements.Reserve(size);
			PopRangeNonAtomic(elements_to_write, size);
			Unlock();

			return size;
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

		ECS_INLINE void PushRangeNonAtomic(Stream<T> elements) {
			m_queue.PushRange(elements);
		}

		void PushRange(Stream<T> elements) {
			// Do a precheck such that we don't lock and unlock
			// For nothing
			if (elements.size > 0) {
				Lock();
				PushRangeNonAtomic(elements);
				Unlock();
			}
		}

		// Returns true if it could peek an element
		bool Peek(T& element) {
			Lock();
			bool has_element = PeekNonAtomic(element);
			Unlock();
			return has_element;
		}

		ECS_INLINE void PushNonAtomic(T element) {
			m_queue.Push(element);
		}

		ECS_INLINE void PushNonAtomic(const T* element) {
			m_queue.Push(element);
		}

		ECS_INLINE bool PeekNonAtomic(T& element) const {
			return m_queue.Peek(element);
		}

		ECS_INLINE void Unlock() {
			m_lock.Unlock();
		}

		ECS_INLINE bool TryLock() {
			return m_lock.TryLock();
		}

		// It has good sleep behaviour on many missed loads
		ECS_INLINE void SpinWaitElements() {
			if (m_queue.GetSize() == 0) {
				m_lock.WaitSignaled();
			}
		}

		ECS_INLINE static size_t MemoryOf(size_t size) {
			return UnderlyingQueue::MemoryOf(size);
		}

		void InitializeFromBuffer(void* buffer, size_t capacity) {
			m_queue.InitializeFromBuffer(buffer, capacity);
			m_lock.Clear();
		}

		ECS_INLINE void InitializeFromBuffer(uintptr_t& buffer, size_t capacity) {
			InitializeFromBuffer((void*)buffer, capacity);
			buffer += MemoryOf(capacity);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, size_t capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			m_queue.Initialize(allocator, capacity, debug_info);
			m_lock.Clear();
		}

		void Initialize(AllocatorPolymorphic allocator, size_t capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			m_queue.Initialize(allocator, capacity, debug_info);
			m_lock.Clear();
		}

		// This can be used when on single thread - it performs no locking
		// Return true in the functor to early exit, if desired. The functor takes as parameter a T or T&
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		ECS_INLINE bool ForEach(Functor&& functor) {
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

		UnderlyingQueue m_queue;
		SpinLock m_lock;
	};

	// Thread-safe queue, not suited to high contention
	template<typename T>
	using ThreadSafeQueue = ThreadSafeQueueAdapter<T, Queue<T>>;
	
	template<typename T>
	using ThreadSafeResizableQueue = ThreadSafeQueueAdapter<T, ResizableQueue<T>>;

	template<typename ElementType, typename ContainerType>
	ECS_INLINE constexpr bool IsQueueType() {
		return std::is_same_v<ContainerType, Queue<ElementType>>;
	}

}

