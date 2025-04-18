#include "ecspch.h"
#include "LinearAllocator.h"
#include "../Utilities/PointerUtilities.h"
#include "AllocatorCallsDebug.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {

	void* LinearAllocator::Allocate(size_t size, size_t alignment, DebugInfo debug_info) {
		// calculating the current pointer and aligning it
		uintptr_t current_pointer = (uintptr_t)m_buffer + m_top;

		uintptr_t offset = AlignPointer(current_pointer, alignment);

		// transforming to relative aligned offset
		offset -= (uintptr_t)m_buffer;

		if (offset + size > m_capacity) {
			ECS_ASSERT(!m_crash_on_allocation_failure, "LinearAllocator capacity exceeded!");
			return nullptr;
		}
		
		void* pointer = (void*)((uintptr_t)m_buffer + offset);
		m_top = offset + size;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = pointer;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_LINEAR, &tracked);
		}

		if (m_profiling_mode) {
			AllocatorProfilingAddAllocation(this, GetCurrentUsage());
		}

		return pointer;
	}

	// For this allocator, do not record the profiling deallocations
	bool LinearAllocator::DeallocateNoAssert(const void* block, DebugInfo debug_info) { return true; }

	void LinearAllocator::Clear(DebugInfo debug_info) {
		m_top = 0;
		m_marker = 0;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = nullptr;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_CLEAR;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_LINEAR, &tracked);
		}
	}

	size_t LinearAllocator::GetMarker() const
	{
		return m_top;
	}

	void LinearAllocator::ReturnToMarker(size_t marker, DebugInfo debug_info)
	{
		ECS_ASSERT(marker <= m_top);
		m_top = marker;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = (void*)marker;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_RETURN_TO_MARKER;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_LINEAR, &tracked);
		}
	}

	bool LinearAllocator::Belongs(const void* buffer) const
	{
		return IsPointerRange(m_buffer, m_top, buffer);
	}

	size_t LinearAllocator::GetRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const
	{
		if (pointer_capacity >= 1) {
			*region_start = GetAllocatedBuffer();
			*region_size = m_capacity;
		}
		return 1;
	}

	void LinearAllocator::SetMarker() {
		m_marker = m_top;
	}

	LinearAllocator LinearAllocator::InitializeFrom(AllocatorPolymorphic allocator, size_t capacity) {
		LinearAllocator return_value;
		
		return_value.m_buffer = ECSEngine::Allocate(allocator, capacity);
		return_value.m_capacity = capacity;
		return_value.m_top = 0;
		return_value.m_marker = 0;
		return_value.m_lock.Clear();
		return_value.m_debug_mode = false;
		return_value.m_profiling_mode = false;

		return return_value;
	}

	void LinearAllocator::SetMarkerTs() {
		ThreadSafeFunctor(&m_lock, [&]() {
			SetMarker();
		});
	}
}