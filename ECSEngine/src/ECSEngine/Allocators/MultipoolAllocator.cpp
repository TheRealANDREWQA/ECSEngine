#include "ecspch.h"
#include "../Utilities/Function.h"
#include "MultipoolAllocator.h"

namespace ECSEngine {

	void* MultipoolAllocator::Allocate(size_t size, size_t alignment) {
		ECS_ASSERT(alignment <= ECS_CACHE_LINE_SIZE);
		// To make sure that the allocation is aligned we are requesting a surplus of alignment from the block range.
		// Then we are aligning the pointer like the stack one and placing the offset byte before the allocation
		size_t index = m_range.Request(size + alignment);

		if (index == 0xFFFFFFFF)
			return nullptr;

		uintptr_t allocation = function::align_pointer_stack((uintptr_t)m_buffer + index, alignment);
		size_t offset = allocation - (uintptr_t)m_buffer;
		m_buffer[offset - 1] = offset - index - 1;
		ECS_ASSERT(offset - index - 1 < alignment);

		return (void*)allocation;
	}

	template<bool trigger_error_if_not_found>
	void MultipoolAllocator::Deallocate(const void* block) {
		// Calculating the start index of the block by reading the byte metadata 

		uintptr_t byte_offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		ECS_ASSERT(m_buffer[byte_offset_position] < ECS_CACHE_LINE_SIZE);
		m_range.Free<trigger_error_if_not_found>((unsigned int)(byte_offset_position - m_buffer[byte_offset_position]));
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, MultipoolAllocator::Deallocate, const void*);

	void MultipoolAllocator::Clear() {
		m_range.Clear();
	}

	size_t MultipoolAllocator::MemoryOf(unsigned int pool_count, unsigned int size) {
		return containers::BlockRange::MemoryOf(pool_count) + size + 8;
	}
	
	void MultipoolAllocator::SetDebugBuffer(unsigned int* buffer)
	{
#ifdef ECSENGINE_DEBUG
		m_range.SetDebugBuffer(buffer);
#endif
	}

	size_t MultipoolAllocator::MemoryOf(unsigned int pool_count) {
		return containers::BlockRange::MemoryOf(pool_count);
	}

	// ---------------------- Thread safe variants -----------------------------

	void* MultipoolAllocator::Allocate_ts(size_t size, size_t alignment) {
		m_spinLock.lock();
		void* pointer = Allocate(size, alignment);
		m_spinLock.unlock();
		return pointer;
	}

	template<bool trigger_error_if_not_found>
	void MultipoolAllocator::Deallocate_ts(const void* block) {
		m_spinLock.lock();
		Deallocate<trigger_error_if_not_found>(block);
		m_spinLock.unlock();
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, MultipoolAllocator::Deallocate_ts, const void*);
}
