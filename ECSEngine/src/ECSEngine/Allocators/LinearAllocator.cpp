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

		ECS_ASSERT(offset + size <= m_capacity);

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
	template<bool trigger_error_if_not_found>
	bool LinearAllocator::Deallocate(const void* block, DebugInfo debug_info) { return true; }

	ECS_TEMPLATE_FUNCTION_BOOL(bool, LinearAllocator::Deallocate, const void*, DebugInfo);

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

	void* LinearAllocator::GetAllocatedBuffer() const
	{
		return m_buffer;
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

	void LinearAllocator::ExitDebugMode()
	{
		m_debug_mode = false;
	}

	void LinearAllocator::ExitProfilingMode()
	{
		m_profiling_mode = false;
	}

	void LinearAllocator::SetDebugMode(const char* name, bool resizable)
	{
		m_debug_mode = true;
		DebugAllocatorManagerChangeOrAddEntry(this, name, resizable, ECS_ALLOCATOR_LINEAR);
	}

	void LinearAllocator::SetProfilingMode(const char* name)
	{
		m_profiling_mode = true;
		AllocatorProfilingAddEntry(this, ECS_ALLOCATOR_LINEAR, name);
	}

	size_t LinearAllocator::GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const
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
		return_value.m_spin_lock.Clear();
		return_value.m_debug_mode = false;
		return_value.m_profiling_mode = false;

		return return_value;
	}

	// ---------------------- Thread safe variants -----------------------------

	void* LinearAllocator::Allocate_ts(size_t size, size_t alignment, DebugInfo debug_info) {
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Allocate(size, alignment, debug_info);
		});
	}

	template<bool trigger_error_if_not_found>
	bool LinearAllocator::Deallocate_ts(const void* block, DebugInfo debug_info) { return true; }

	ECS_TEMPLATE_FUNCTION_BOOL(bool, LinearAllocator::Deallocate_ts, const void*, DebugInfo);

	void LinearAllocator::SetMarker_ts() {
		ThreadSafeFunctor(&m_spin_lock, [&]() {
			SetMarker();
		});
	}
}