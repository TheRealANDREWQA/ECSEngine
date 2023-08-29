#pragma once
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/FunctionInterfaces.h"

#define ECS_CIRCULAR_STACK_RESIZE_FACTOR (1.5f)

namespace ECSEngine {

	// It uses a capacity stream as it basis. The difference between a capacity stream and this
	// is that the stack when gets full it will overwrite the old elements (useful for undo stacks
	// for example)
	template<typename T>
	struct Stack {
	public:
		Stack() : m_stack(nullptr, 0, 0), m_first_item(0) {}
		Stack(void* buffer, unsigned int capacity) {
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

		// Returns the index of the "first" item
		// Make sure that there is an item in the stack.
		ECS_INLINE unsigned int PeekIndex() const {
			unsigned int last_index = m_first_item + m_stack.size;
			last_index = last_index >= m_stack.capacity ? last_index - m_stack.capacity : last_index;
			return last_index - 1;
		}

		// Returns a pointer to the "first" element (the one to be popped)
		// such that it can be modifed while inside the stack.
		// Make sure that the size is greater than 0
		ECS_INLINE T* PeekIntrusive() {
			return m_stack.buffer + PeekIndex();
		}

		ECS_INLINE bool Peek(T& element) const {
			if (m_stack.size > 0) {
				element = m_stack[PeekIndex()];
				return true;
			}
			else {
				return false;
			}
		}

		bool Pop(T& element) {
			if (m_stack.size > 0) {
				unsigned int peek_index = PeekIndex();
				element = m_stack[peek_index];
				m_stack.size--;
				return true;
			}
			return false;
		}

		void Push(T element) {
			unsigned int peek_index = PeekIndex();
			peek_index = peek_index == (m_stack.capacity - 1) ? 0 : peek_index + 1;
			m_stack[peek_index] = element;

			if (m_stack.size == m_stack.capacity) {
				m_first_item = m_first_item == m_stack.capacity - 1 ? 0 : m_first_item + 1;
			}
			else {
				m_stack.size++;
			}
		}

		void Push(const T* element) {
			unsigned int peek_index = PeekIndex();
			peek_index = peek_index == (m_stack.capacity - 1) ? 0 : peek_index + 1;
			m_stack[peek_index] = *element;

			if (m_stack.size == m_stack.capacity) {
				m_first_item = m_first_item == m_stack.capacity - 1 ? 0 : m_first_item + 1;
			}
			else {
				m_stack.size++;
			}
		}

		void Reset() {
			m_first_item = 0;
			m_stack.Reset();
		}

		ECS_INLINE void* GetAllocatedBuffer() const {
			return m_stack.buffer;
		}

		ECS_INLINE static size_t MemoryOf(size_t count) {
			return CapacityStream<T>::MemoryOf(count);
		}

		void InitializeFromBuffer(void* buffer, unsigned int capacity) {
			m_stack = CapacityStream<T>(buffer, 0, capacity); 
			m_first_item = 0; 
		}

		void InitializeFromBuffer(uintptr_t& buffer, unsigned int capacity) {
			InitializeFromBuffer((void*)buffer, capacity);
			buffer += MemoryOf(capacity);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = allocator->Allocate(memory_size, alignof(T), debug_info);
			InitializeFromBuffer(allocation, capacity);
		}

		// Returns the i'th element - 0 means the top most one (the one that would be returned by peek)
		// and increasingly away from it
		ECS_INLINE T GetElement(unsigned int index) const {
			unsigned int peek_index = m_first_item + m_stack.size - index;
			peek_index = peek_index >= m_stack.capacity ? peek_index - m_stack.capacity : peek_index;
			return m_stack[peek_index];
		}

		CapacityStream<T> m_stack;
		unsigned int m_first_item;
	};

	// The same as a resizable stream, the difference being that it has only a push, pop, peek interface
	template<typename T>
	struct ResizableStack {
	public:
		ResizableStack() {}
		ResizableStack(AllocatorPolymorphic allocator, unsigned int capacity) : m_stack(allocator, capacity) {}

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

		// Make sure that there is an element
		ECS_INLINE T* PeekIntrusive() {
			return m_stack.buffer + m_stack.size - 1;
		}

		ECS_INLINE bool Peek(T& element) const {
			if (m_stack.size > 0) {
				element = m_stack[m_stack.size - 1];
				return true;
			}
			else {
				return false;
			}
		}

		bool Pop(T& element) {
			if (m_stack.size > 0) {
				m_stack.size--;
				element = m_stack[m_stack.size];
				return true;
			}
			return false;
		}

		void Push(T element) {
			m_stack.Add(element);
		}

		void Push(const T* element) {
			m_stack.Add(element);
		}

		void Reset() {
			m_stack.Reset();
		}

		ResizableStream<T> m_stack;
	};

}