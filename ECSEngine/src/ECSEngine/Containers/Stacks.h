#pragma once
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/FunctionInterfaces.h"

constexpr float ECS_CIRCULAR_STACK_RESIZE_FACTOR = 1.5f;

namespace ECSEngine {

	// does not check to see if it goes out of boundaries
	template<typename T>
	struct Stack {
	public:
		Stack() : m_stack(nullptr, 0, 0), m_first_item(0), m_last_item(0) {}
		Stack(void* buffer, size_t capacity) {
			InitializeFromBuffer(buffer, capacity);
		}

		Stack(const Stack& other) = default;
		Stack& operator = (const Stack& other) = default;

		ECS_INLINE unsigned int GetSize() const {
			return m_stack.size;
		}

		ECS_INLINE unsigned int GetCapacity() const {
			return m_stack.capacity;
		}

		ECS_INLINE CapacityStream<T>* GetStack() const {
			return &m_stack;
		}

		ECS_INLINE bool HasSpace(unsigned int count) const {
			return m_stack.size + count <= m_stack.capacity;
		}

		ECS_INLINE bool Peek(T& element) const {
			if (m_stack.size > 0) {
				if (m_last_item != 0) {
					element = m_stack[m_last_item - 1];
				}
				else {
					element = m_stack[m_stack.size - 1];
				}
				return true;
			}
			else {
				return false;
			}
		}

		bool Pop(T& element) {
			if (m_stack.size > 0) {
				m_last_item = function::Select(m_last_item == 0, m_stack.capacity - 1, m_last_item - 1);
				element = m_stack[m_last_item];
				m_stack.size--;
				return true;
			}
			return false;
		}

		void Push(T element) {
			if (m_stack.size == m_stack.capacity) {
				m_stack[m_first_item] = element;
				m_last_item = m_first_item;
				m_first_item = function::Select<unsigned int>(m_first_item == m_stack.capacity - 1, 0, m_first_item + 1);
			}
			else {
				m_stack[m_last_item] = element;
				m_last_item = function::Select<unsigned int>(m_first_item == m_stack.capacity - 1, 0, m_last_item + 1);
				m_stack.size++;
			}
		}

		void Push(const T* element) {
			if (m_stack.size == m_stack.capacity) {
				m_stack[m_first_item] = *element;
				m_last_item = m_first_item;
				m_first_item = function::Select<unsigned int>(m_first_item == m_stack.capacity - 1, 0, m_first_item + 1);
			}
			else {
				m_stack[m_last_item] = *element;
				m_last_item = function::Select<unsigned int>(m_first_item == m_stack.capacity - 1, 0, m_last_item + 1);
				m_stack.size++;
			}
		}

		void Reset() {
			m_first_item = 0;
			m_last_item = 0;
			m_stack.Reset();
		}

		ECS_INLINE void* GetAllocatedBuffer() const {
			return m_stack.buffer;
		}

		ECS_INLINE const T* PeekLastItem() const {
			return &m_stack[m_first_item];
		}

		ECS_INLINE static size_t MemoryOf(size_t count) {
			return CapacityStream<T>::MemoryOf(count);
		}

		void InitializeFromBuffer(void* buffer, size_t capacity) {
			m_stack = CapacityStream<T>(buffer, 0, capacity); 
			m_first_item = 0; 
			m_last_item = 0;
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

		// Returns the i'th element - 0 means the top most one (the one that would be returned by peek)
		// and increasingly away from it
		ECS_INLINE T GetElement(unsigned int index) const {
			if (m_last_item < index - 1) {
				return m_stack[m_stack.size + m_last_item - index - 1];
			}
			return m_stack[m_last_item - index - 1];
		}

	//private:
		CapacityStream<T> m_stack;
		unsigned int m_first_item;
		unsigned int m_last_item;
	};

	template<typename T>
	struct ResizableStack {
	public:
		ResizableStack() : m_first_item(0), m_last_item(0) {}
		ResizableStack(AllocatorPolymorphic allocator, size_t capacity) : m_stack(allocator, 0, capacity), m_first_item(0), m_last_item(0) {}

		ResizableStack(const ResizableStack& other) = default;
		ResizableStack& operator = (const ResizableStack& other) = default;

		ECS_INLINE void FreeBuffer() {
			m_stack.FreeBuffer();
		}

		ECS_INLINE unsigned int GetElementCount() const {
			return m_stack.size;
		}

		ECS_INLINE unsigned int GetCurrentCapacity() const {
			return m_stack.capacity;
		}

		ECS_INLINE ResizableStream<T>* GetStream() const {
			return &m_stack;
		}

		ECS_INLINE bool Peek(T& element) const {
			if (m_stack.size > 0) {
				unsigned int index = function::Select(m_last_item != 0, m_last_item - 1, m_stack.size - 1);
				element = m_stack[index];
				return true;
			}
			else {
				return false;
			}
		}

		bool Pop(T& element) {
			if (m_stack.size > 0) {
				element = m_stack[m_last_item];
				m_last_item = function::Select(m_last_item == 0, m_stack.capacity - 1, m_last_item - 1);
				m_stack.size--;
				return true;
			}
			return false;
		}

		void Push(T element) {
			if (m_stack.size == m_stack.capacity) {
				unsigned int old_capacity = m_stack.capacity;
				const T* old_buffer = m_stack.buffer;

				m_stack.FreeBuffer();
				m_stack.ResizeNoCopy(static_cast<size_t>((m_stack.capacity + 1) * ECS_CIRCULAR_STACK_RESIZE_FACTOR));

				memcpy(m_stack.buffer, old_buffer + m_first_item, sizeof(T) * (old_capacity - m_first_item));
				memcpy(m_stack.buffer + old_capacity - m_first_item, old_buffer, sizeof(T) * m_first_item);
				m_first_item = 0;
				m_stack.Add(element);
				m_last_item = m_stack.size;
			}
			else {
				m_stack[m_last_item] = element;
				m_last_item = function::Select(m_last_item == m_stack.capacity, 0, m_last_item + 1);
				m_stack.size++;
			}
		}

		void Push(const T* element) {
			if (m_stack.size == m_stack.capacity) {
				unsigned int old_capacity = m_stack.capacity;
				const T* old_buffer = m_stack.buffer;

				m_stack.FreeBuffer();
				m_stack.ResizeNoCopy(static_cast<size_t>((m_stack.capacity + 1) * ECS_CIRCULAR_STACK_RESIZE_FACTOR));

				memcpy(m_stack.buffer, old_buffer + m_first_item, sizeof(T) * (old_capacity - m_first_item));
				memcpy(m_stack.buffer + old_capacity - m_first_item, old_buffer, sizeof(T) * m_first_item);
				m_first_item = 0;
				m_stack.Add(element);
				m_last_item = m_stack.size;
			}
			else {
				m_stack[m_last_item] = *element;
				m_last_item = function::Select(m_last_item == m_stack.capacity, 0, m_last_item + 1);
				m_stack.size++;
			}
		}

		void Reset() {
			m_first_item = 0;
			m_last_item = 0;
			m_stack.Reset();
		}

	//private:
		ResizableStream<T> m_stack;
		unsigned int m_first_item;
		unsigned int m_last_item;
	};

}