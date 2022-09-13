#include "ecspch.h";
#include "LinearAllocator.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	void* LinearAllocator::Allocate(size_t size, size_t alignment) {
		// calculating the current pointer and aligning it
		uintptr_t current_pointer = (uintptr_t)m_buffer + m_top;

		uintptr_t offset = function::AlignPointer(current_pointer, alignment);

		// transforming to relative aligned offset
		offset -= (uintptr_t)m_buffer;

		ECS_ASSERT(offset + size <= m_capacity);

		void* pointer = (void*)((uintptr_t)m_buffer + offset);
		m_top = offset + size;

		return pointer;
	}

	void LinearAllocator::Deallocate(const void* block) {}

	void LinearAllocator::Clear() {
		m_top = 0;
		m_marker = 0;
	}

	size_t LinearAllocator::GetMarker() const
	{
		return m_top;
	}

	void* LinearAllocator::GetAllocatedBuffer()
	{
		return m_buffer;
	}

	void LinearAllocator::ReturnToMarker(size_t marker)
	{
		ECS_ASSERT(marker <= m_top);
		m_top = marker;
	}

	bool LinearAllocator::Belongs(const void* buffer) const
	{
		return function::IsPointerRange(m_buffer, m_top, buffer);
	}

	void LinearAllocator::SetMarker() {
		m_marker = m_top;
	}

	// ---------------------- Thread safe variants -----------------------------

	void* LinearAllocator::Allocate_ts(size_t size, size_t alignment) {
		m_spin_lock.lock();
		void* pointer = Allocate(size, alignment);
		m_spin_lock.unlock();
		return pointer;
	}

	void LinearAllocator::Deallocate_ts(const void* block) {}

	void LinearAllocator::SetMarker_ts() {
		m_spin_lock.lock();
		SetMarker();
		m_spin_lock.unlock();
	}
}