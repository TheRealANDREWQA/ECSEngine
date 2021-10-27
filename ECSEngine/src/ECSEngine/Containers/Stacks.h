#pragma once
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/FunctionInterfaces.h"

constexpr float ECS_CIRCULAR_STACK_RESIZE_FACTOR = 1.5f;

namespace ECSEngine {

	namespace containers {

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

			unsigned int GetElementCount() const {
				return m_stack.size;
			}

			unsigned int GetCapacity() const {
				return m_stack.capacity;
			}

			CapacityStream<T>* GetStack() const {
				return &m_stack;
			}

			bool HasSpace(unsigned int count) const {
				return m_stack.size + count <= m_stack.capacity;
			}

			bool Peek(T& element) const {
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

			void* GetAllocatedBuffer() const {
				return m_stack.buffer;
			}

			const T* PeekLastItem() const {
				return &m_stack[m_first_item];
			}

			static size_t MemoryOf(size_t count) {
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

		//private:
			CapacityStream<T> m_stack;
			unsigned int m_first_item;
			unsigned int m_last_item;
		};

		template<typename T, typename Allocator, bool zero_memory = false>
		struct ResizableStack {
		public:
			ResizableStack() : m_first_item(0), m_last_item(0) {}
			ResizableStack(Allocator* allocator, size_t capacity) : m_stack(allocator, 0, capacity), m_first_item(0), m_last_item(0) {}

			ResizableStack(const ResizableStack& other) = default;
			ResizableStack(ResizableStack&& other) = default;

			ResizableStack& operator = (const ResizableStack& other) = default;
			ResizableStack& operator = (ResizableStack&& other) = default;

			void FreeBuffer() {
				m_stack.FreeBuffer();
			}

			unsigned int GetElementCount() const {
				return m_stack.size;
			}

			unsigned int GetCurrentCapacity() const {
				return m_stack.capacity;
			}

			ResizableStream<T, Allocator, zero_memory>* GetStream() const {
				return &m_stack;
			}

			bool Peek(T& element) const {
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
					m_stack[m_stack.size++] = element;
					m_last_item = m_stack.size;
				}
				else {
					m_stack[m_last_item] = element;
					m_last_item = (m_last_item + 1) % m_stack.capacity;
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
					m_stack[m_stack.size++] = *element;
					m_last_item = m_stack.size;
				}
				else {
					m_stack[m_last_item] = *element;
					m_last_item = (m_last_item + 1) % m_stack.capacity;
					m_stack.size++;
				}
			}

			void Reset() {
				m_first_item = 0;
				m_last_item = 0;
				m_stack.Reset();
			}

		//private:
			ResizableStream<T, Allocator, zero_memory> m_stack;
			unsigned int m_first_item;
			unsigned int m_last_item;
		};

	}

}