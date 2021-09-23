#pragma once
#include "../Core.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"
#include "Stream.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	// Lock-free stream that allows multiple threads to read items from it. It is intended to be static over the course
	// of a period of a time, typically a frame. Changes can be made during transitions from one period to the other
	template<typename T>
	struct StaticLockFreeStream
	{
		StaticLockFreeStream(void* allocation, size_t capacity) : m_index(0) {
			// stream's size will be used to add items to the stream
			m_stream = CapacityStream<T>(allocation, 0, capacity);
		}

		StaticLockFreeStream(const StaticLockFreeStream& other) = default;
		StaticLockFreeStream& operator = (const StaticLockFreeStream& other) = default;

		unsigned int Add(const T& element) {
			m_stream.AddSafe(element);
			return m_stream.size - 1;
		}

		void Clear() {
			m_stream.size = 0;
			ClearIndex();
		}

		void ClearIndex() {
			m_index.store(0, ECS_RELEASE);
		}

		bool Get(T& element) const {
			if (m_index == m_stream.capacity)
				return false;

			// atomic increment while keeping the value before
			unsigned int index = m_index.fetch_add(1, ECS_ACQ_REL);
			element = m_stream[index];
			return true;
		}

		size_t GetCapacity() const {
			return m_stream.capacity;
		}

		CapacityStream<T> GetStream() const {
			return m_stream;
		}

		void Remove(unsigned int index) {
			m_stream.size--;
			for (size_t stream_index = index; stream_index < m_stream.size; stream_index++) {
				m_stream[stream_index] = m_stream[stream_index + 1];
			}
		}

		// it will do a liniar search to find it
		void Remove(const T& element) {
			for (size_t index = 0; index < m_stream.size; index++) {
				if (element == m_stream[index]) {
					Remove(index);
					return;
				}
			}
		}

		void RemoveAtSwapBack(unsigned int index) {
			m_stream.size--;
			m_stream[index] = m_stream[m_stream.size];
		}

		// it will do a liniar search to find it
		void RemoveAtSwapBack(const T& element) {
			for (size_t index = 0; index < m_stream.size; index++) {
				if (element == m_stream[index]) {
					RemoveAtSwapBack(index);
					return;
				}
			}
		}

		const void* GetAllocatedBuffer() const {
			return m_stream.GetAllocatedBuffer();
		}

		static size_t MemoryOf(size_t size) {
			return CapacityStream<T>::MemoryOf(size);
		}

		void InitializeFromBuffer(void* buffer, size_t capacity) {
			m_stream = CapacityStream<T>(buffer, 0, capacity);
			m_index.store(0, ECS_RELEASE);
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
		CapacityStream<T> m_stream;
		std::atomic<unsigned int> m_index;
	};

}

