#include "ecspch.h";
#include "StackAllocator.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	void* StackAllocator::Allocate(size_t size, size_t alignment) {
		ECS_ASSERT(m_buffer != nullptr);

		// calculating the current pointer and aligning it
		uintptr_t current_pointer = (uintptr_t)(m_buffer + m_top);

		uintptr_t offset = function::AlignPointerStack(current_pointer, alignment);

		// transforming to relative offset
		offset -= (uintptr_t)m_buffer;

		ECS_ASSERT(offset + size <= m_capacity);

		void* pointer = &m_buffer[offset];
		m_buffer[offset - 1] = offset - 1;
		m_last_top = m_top;
		m_top = offset + size;

		return pointer;
	}

	void StackAllocator::Deallocate(const void* block) {
		ECS_ASSERT(block != nullptr);

		uintptr_t offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		m_top = offset_position - m_buffer[offset_position];
	}

	void StackAllocator::Deallocate() {
		m_top = m_last_top;
	}

	void StackAllocator::Clear() {
		m_top = 0;
		m_marker = 0;
		m_last_top = 0;
	}

	bool StackAllocator::Belongs(const void* pointer) const
	{
		uintptr_t ptr = (uintptr_t)pointer;
		return ptr >= (uintptr_t)m_buffer && ptr < (uintptr_t)m_buffer + m_capacity;
	}

	void* StackAllocator::GetAllocatedBuffer()
	{
		return m_buffer;
	}

	void StackAllocator::SetMarker() {
		m_marker = m_top;
	}

	void StackAllocator::ReturnToMarker() {
		m_top = m_marker;
	}

	size_t StackAllocator::GetCapacity() const {
		return m_capacity;
	}

	size_t StackAllocator::GetTop() const {
		return m_top;
	}
	
	// ---------------------- Thread safe variants -----------------------------

	void* StackAllocator::Allocate_ts(size_t size, size_t alignment) {
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Allocate(size, alignment);
		});
	}

	void StackAllocator::Deallocate_ts(const void* block) {
		ThreadSafeFunctor(&m_spin_lock, [&]() {
			Deallocate(block);
		});
	}

	void StackAllocator::SetMarker_ts() {
		ThreadSafeFunctor(&m_spin_lock, [&]() {
			SetMarker();
		});
	}

	void StackAllocator::ReturnToMarker_ts() {	
		ThreadSafeFunctor(&m_spin_lock, [&]() {
			ReturnToMarker();
		});
	}

}