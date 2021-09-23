#include "ecspch.h";
#include "LinearAllocator.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	void* LinearAllocator::Allocate(size_t size, size_t alignment) {
		// calculating the current pointer and aligning it
		uintptr_t current_pointer = (uintptr_t)m_buffer + m_top;

		uintptr_t offset = function::align_pointer(current_pointer, alignment);

		// transforming to relative aligned offset
		offset -= (uintptr_t)m_buffer;

		ECS_ASSERT(offset + size <= m_capacity);

		void* pointer = (void*)((uintptr_t)m_buffer + offset);
		m_top = offset + size;

		return pointer;

	}

	void LinearAllocator::Deallocate(const void* block) {
		ECS_ASSERT(m_top > m_marker);
		m_top = m_marker;
	}

	void LinearAllocator::Clear() {
		m_top = 0;
		m_marker = 0;
	}

	void LinearAllocator::SetMarker() {
		m_marker = m_top;
	}

	// ---------------------- Thread safe variants -----------------------------

	void* LinearAllocator::Allocate_ts(size_t size, size_t alignment) {
		m_spinLock.lock();
		void* pointer = Allocate(size, alignment);
		m_spinLock.unlock();
		return pointer;
	}

	void LinearAllocator::Deallocate_ts(const void* block) {
		m_spinLock.lock();
		Deallocate(block);
		m_spinLock.unlock();
	}

	void LinearAllocator::SetMarker_ts() {
		m_spinLock.lock();
		SetMarker();
		m_spinLock.unlock();
	}
}