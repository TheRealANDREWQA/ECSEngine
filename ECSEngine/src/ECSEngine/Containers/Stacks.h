#pragma once
#include "../Core.h"
#include "Stream.h"

#define ECS_CIRCULAR_STACK_RESIZE_FACTOR (1.5f)

namespace ECSEngine {

	// It uses a capacity stream as it basis. The difference between a capacity stream and this
	// is that the stack when gets full it will overwrite the old elements (useful for undo stacks
	// for example)
	template<typename T>
	struct Stack {
	public:
		ECS_INLINE Stack() : m_stack(nullptr, 0, 0), m_first_item(0) {}
		ECS_INLINE Stack(void* buffer, unsigned int capacity) {
			InitializeFromBuffer(buffer, capacity);
		}

		Stack(const Stack& other) = default;
		Stack& operator = (const Stack& other) = default;

		// Iterates all elements in the stack order
		// Return true if you want to early exit. It returns true
		// If it early exited, else false. The returned indices are given
		// With respect to the top element (so the 0th element is the top most one)
		// To retrieve data use GetElement()
		template<bool early_exit = false, typename Functor>
		bool ForEachIndex(Functor&& functor) const {
			unsigned int starting_index = m_first_item + m_stack.size;
			unsigned int access_index = m_stack.capacity <= starting_index ? starting_index - m_stack.capacity : starting_index;
			for (unsigned int index = 0; index < m_stack.size; index++) {
				if constexpr (early_exit) {
					if (functor(access_index)) {
						return true;
					}
				}
				else {
					functor(access_index);
				}
				access_index = access_index == 0 ? m_stack.capacity - 1 : access_index - 1;
			}

			return false;
		}

		// Iterates all elements in the stack order
		// Return true if you want to early exit. It returns true
		// If it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			return ForEachIndex<early_exit>([&](unsigned int index) {
				return functor(GetElement(index));
			});
		}

		// Iterates all elements in the stack order
		// Return true if you want to early exit. It returns true
		// If it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) const {
			return ForEachIndex<early_exit>([&](unsigned int index) {
				return functor(GetElement(index));
			});
		}

		// This version allows you to iterate over elements that are outside the valid
		// elements of this stack. This is useful for undo/redo stacks where there are
		// elements outside the stack which are still valid. The starting_index and ending_index
		// Are both relative to the first item. Starting index of 0 means the last added item
		// (or m_first_item) and size means the next element after the last pushed element
		template<bool early_exit = false, typename Functor>
		bool ForEachRange(unsigned int starting_index, unsigned int ending_index, Functor&& functor) {
			unsigned int iterate_count = ending_index - starting_index;

			starting_index = m_first_item + starting_index;
			unsigned int access_index = m_stack.capacity <= starting_index ? starting_index - m_stack.capacity : starting_index;
			for (unsigned int index = 0; index < iterate_count; index++) {
				if constexpr (early_exit) {
					if (functor(m_stack[access_index])) {
						return true;
					}
				}
				else {
					functor(m_stack[access_index]);
				}
				access_index = access_index == m_stack.capacity - 1 ? 0 : access_index + 1;
			}

			return false;
		}

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

	private:
		// Returns the index of the "first" item
		// Make sure that there is an item in the stack.
		ECS_INLINE unsigned int PeekIndex(unsigned int top_index = 0) const {
			unsigned int last_index = m_first_item + m_stack.size - top_index;
			last_index = last_index > m_stack.capacity ? last_index - m_stack.capacity - 1 : last_index - 1;
			return last_index;
		}

	public:

		// Returns a pointer to the "first" element (the one to be popped)
		// such that it can be modifed while inside the stack.
		// Make sure that the size is greater than 0
		ECS_INLINE T* PeekIntrusive() {
			return m_stack.buffer + PeekIndex();
		}

		ECS_INLINE const T* PeekConstant() const {
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

		// Only decrements the size, useful for redo/undo situations
		bool PopIndexOnly() {
			if (m_stack.size > 0) {
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

		// This version returns true if an element is overwritten and fills it in the given pointer
		bool PushOverwrite(T element, T* overwritten_element) {
			unsigned int peek_index = PeekIndex();
			peek_index = peek_index == (m_stack.capacity - 1) ? 0 : peek_index + 1;

			if (m_stack.size == m_stack.capacity) {
				*overwritten_element = m_stack[m_first_item];
				m_stack[peek_index] = element;
				m_first_item = m_first_item == m_stack.capacity - 1 ? 0 : m_first_item + 1;
				return true;
			}
			else {
				m_stack[peek_index] = element;
				m_stack.size++;
				return false;
			}
		}

		// This version returns true if an element is overwritten and fills it in the given pointer
		bool PushOverwrite(const T* element, T* overwritten_element) {
			unsigned int peek_index = PeekIndex();
			peek_index = peek_index == (m_stack.capacity - 1) ? 0 : peek_index + 1;

			if (m_stack.size == m_stack.capacity) {
				*overwritten_element = m_stack[m_first_item];
				m_stack[peek_index] = *element;
				m_first_item = m_first_item == m_stack.capacity - 1 ? 0 : m_first_item + 1;
				return true;
			}
			else {
				m_stack[peek_index] = *element;
				m_stack.size++;
				return false;
			}
		}

		// This only increases the count - useful for undo/redo
		void PushIndexOnly() {
			ECS_ASSERT(m_stack.size < m_stack.capacity);
			m_stack.size++;
		}

		ECS_INLINE void Reset() {
			m_first_item = 0;
			m_stack.Reset();
		}

		// Niche case function. If you can identify some elements other than at the top
		// That need to be removed, use this function. The index must be relative to the top element,
		// You can get such an index from ForEachIndex
		void RemoveAtIndex(unsigned int index) {
			unsigned int peek_index = PeekIndex(index);
			if (m_first_item == 0) {
				m_stack.Remove(peek_index);
			}
			else {
				unsigned int last_index = PeekIndex(0);
				if (peek_index < m_first_item) {
					memmove(m_stack.buffer + peek_index, m_stack.buffer + peek_index + 1, sizeof(T) * (last_index - peek_index));
				}
				else {
					bool overflows = m_stack.size + m_first_item > m_stack.capacity;
					if (overflows) {
						unsigned int remaining_elements = m_stack.capacity - peek_index - 1;
						memmove(m_stack.buffer + peek_index, m_stack.buffer + peek_index + 1, sizeof(T) * remaining_elements);
						m_stack[m_stack.capacity - 1] = m_stack[0];
						memmove(m_stack.buffer, m_stack.buffer + 1, sizeof(T) * last_index);
					}
					else {
						memmove(m_stack.buffer + peek_index, m_stack.buffer + peek_index + 1, sizeof(T) * (last_index - peek_index));
					}
				}
				m_stack.size--;
			}
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

		void Initialize(AllocatorPolymorphic allocator, unsigned int capacity, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = ECSEngine::Allocate(allocator, memory_size, alignof(T), debug_info);
			InitializeFromBuffer(allocation, capacity);
		}

		// Returns the i'th element - 0 means the top most one (the one that would be returned by peek)
		// and increasingly away from it
		ECS_INLINE T GetElement(unsigned int index) const {
			return m_stack[PeekIndex(index)];
		}

		ECS_INLINE T* GetElementPtr(unsigned int index) {
			return &m_stack[PeekIndex(index)];
		}

		CapacityStream<T> m_stack;
		unsigned int m_first_item;
	};

	// The same as a resizable stream, the difference being that it has only a push, pop, peek interface
	template<typename T>
	struct ResizableStack {
	public:
		ECS_INLINE ResizableStack() {}
		ECS_INLINE ResizableStack(AllocatorPolymorphic allocator, unsigned int capacity) : m_stack(allocator, capacity) {}

		ResizableStack(const ResizableStack& other) = default;
		ResizableStack& operator = (const ResizableStack& other) = default;

		// Iterates all elements in the stack order
		// Return true if you want to early exit. It returns true
		// If it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachIndex(Functor&& functor) {
			for (unsigned int index = m_stack.size; index > 0; index--) {
				if constexpr (early_exit) {
					if (functor(index - 1)) {
						return true;
					}
				}
				else {
					functor(index - 1);
				}
			}

			return false;
		}

		// Iterates all elements in the stack order
		// Return true if you want to early exit. It returns true
		// If it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			return ForEachIndex<early_exit>([&](unsigned int index) {
				if constexpr (early_exit) {
					return functor(m_stack[index]);
				}
				else {
					functor(m_stack[index]);
				}
			});
		}

		// Iterates all elements in the stack order
		// Return true if you want to early exit. It returns true
		// If it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) const {
			return ForEachIndex<early_exit>([&](unsigned int index) {
				if constexpr (early_exit) {
					return functor(m_stack[index]);
				}
				else {
					functor(m_stack[index]);
				}
			});
		}

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

		ECS_INLINE void Push(T element) {
			m_stack.Add(element);
		}

		ECS_INLINE void Push(const T* element) {
			m_stack.Add(element);
		}

		// Pushes the elements from the iterator in reverse order, meaning the first iterator
		// Element will be the first one to be popped
		void PushReverseOrder(IteratorInterface<T>* iterator) {
			// Reserve space for the number of iterator elements
			size_t iterate_count = iterator->GetRemainingCount();
			m_stack.Reserve(iterate_count);
			// Write them in reverse order
			unsigned int write_index = m_stack.size + iterate_count - 1;
			iterator->ForEach([&](const T* element) {
				m_stack[write_index] = *element;
				write_index--;
			});
		}

		// Pushes the elements from the iterator in reverse order, but the iterator element type is
		// Different from the one stored here, and the element functor creates a proper element of
		// The one from the iterator. The functor receives as argument (const IteratorElementType& element)
		template<typename IteratorElementType, typename ElementFunctor>
		void PushReverseOrder(IteratorInterface<IteratorElementType>* iterator, ElementFunctor&& functor) {
			// Reserve space for the number of iterator elements
			size_t iterate_count = iterator->GetRemainingCount();
			m_stack.Reserve(iterate_count);
			// Write them in reverse order
			unsigned int write_index = m_stack.size + iterate_count - 1;
			iterator->ForEach([&](const IteratorElementType* element) {
				m_stack[write_index] = functor(*element);
				write_index--;
			});
		}

		ECS_INLINE void Reset() {
			m_stack.Reset();
		}

		// Niche case function. If you can identify some elements other than at the top
		// That need to be removed, use this function
		ECS_INLINE void RemoveAtIndex(unsigned int index) {
			m_stack.Remove(index);
		}

		ResizableStream<T> m_stack;
	};

}