#include "ecspch.h";
#include "StackAllocator.h"
#include "../Utilities/Function.h"

#define ECS_SHOW_LOGS_STACKALLOCATOR 0

namespace ECSEngine {

	void* StackAllocator::Allocate(size_t size, size_t alignment) {
		ECS_ASSERT(m_buffer != nullptr);

		// calculating the current pointer and aligning it
		uintptr_t current_pointer = (uintptr_t)(m_buffer + m_top);

		uintptr_t offset = function::align_pointer_stack(current_pointer, alignment);

		// transforming to relative offset
		offset -= (uintptr_t)m_buffer;

		ECS_ASSERT(offset + size <= m_capacity);

		void* pointer = &m_buffer[offset];
		m_buffer[offset - 1] = offset - 1;
		m_last_top = m_top;
		m_top = offset + size;

#if ECS_SHOW_LOGS_STACKALLOCATOR
		ECSENGINE_CORE_INFO("Succesfull stack allocation! {0} {1} {2}", m_top, size, alignment);
#endif

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
		m_spin_lock.lock();
		void* pointer = Allocate(size, alignment);
		m_spin_lock.unlock();
		return pointer;
	}

	void StackAllocator::Deallocate_ts(const void* block) {
		m_spin_lock.lock();
		Deallocate(block);
		m_spin_lock.unlock();
	}

	void StackAllocator::Deallocate_ts() {
		m_spin_lock.lock();
		Deallocate();
		m_spin_lock.unlock();
	}

	void StackAllocator::SetMarker_ts() {
		m_spin_lock.lock();
		SetMarker();
		m_spin_lock.unlock();
	}

	void StackAllocator::ReturnToMarker_ts() {
		m_spin_lock.lock();
		ReturnToMarker();
		m_spin_lock.unlock();
	}

}