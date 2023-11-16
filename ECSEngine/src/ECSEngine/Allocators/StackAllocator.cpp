#include "ecspch.h";
#include "StackAllocator.h"
#include "../Utilities/PointerUtilities.h"
#include "AllocatorCallsDebug.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {

	void* StackAllocator::Allocate(size_t size, size_t alignment, DebugInfo debug_info) {
		ECS_ASSERT(m_buffer != nullptr);

		// calculating the current pointer and aligning it
		uintptr_t current_pointer = (uintptr_t)(m_buffer + m_top);

		uintptr_t offset = AlignPointerStack(current_pointer, alignment);

		// transforming to relative offset
		offset -= (uintptr_t)m_buffer;

		ECS_ASSERT(offset + size <= m_capacity);

		void* pointer = &m_buffer[offset];
		m_buffer[offset - 1] = offset - 1;
		m_last_top = m_top;
		m_top = offset + size;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = pointer;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_STACK, &tracked);
		}

		if (m_profiling_mode) {
			AllocatorProfilingAddAllocation(this, GetCurrentUsage());
		}

		return pointer;
	}

	template<bool trigger_error_if_not_found>
	bool StackAllocator::Deallocate(const void* block, DebugInfo debug_info) {
		ECS_ASSERT(block != nullptr);

		uintptr_t offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		m_top = offset_position - m_buffer[offset_position];

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = block;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_DEALLOCATE;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_STACK, &tracked);
		}
		// For this type of allocator, we shouldn't record the deallocation
		// Counts for profiling since they are of not much use

		return true;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, StackAllocator::Deallocate, const void*, DebugInfo);

	void StackAllocator::Deallocate(DebugInfo debug_info) {
		m_top = m_last_top;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = m_buffer + m_top;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_DEALLOCATE;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_STACK, &tracked);
		}
	}

	void StackAllocator::Clear(DebugInfo debug_info) {
		m_top = 0;
		m_marker = 0;
		m_last_top = 0;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = nullptr;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_CLEAR;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_STACK, &tracked);
		}
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

	void StackAllocator::ReturnToMarker(DebugInfo debug_info) {
		m_top = m_marker;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = (void*)m_marker;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_RETURN_TO_MARKER;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_STACK, &tracked);
		}
	}

	size_t StackAllocator::GetCapacity() const {
		return m_capacity;
	}

	void StackAllocator::ExitDebugMode()
	{
		m_debug_mode = false;
	}

	void StackAllocator::ExitProfilingMode()
	{
		m_profiling_mode = false;
	}

	void StackAllocator::SetDebugMode(const char* name, bool resizable)
	{
		m_debug_mode = true;
		DebugAllocatorManagerChangeOrAddEntry(this, name, true, ECS_ALLOCATOR_STACK);
	}

	void StackAllocator::SetProfilingMode(const char* name)
	{
		m_profiling_mode = false;
		AllocatorProfilingAddEntry(this, ECS_ALLOCATOR_STACK, name);
	}

	size_t StackAllocator::GetTop() const {
		return m_top;
	}
	
	// ---------------------- Thread safe variants -----------------------------

	void* StackAllocator::Allocate_ts(size_t size, size_t alignment, DebugInfo debug_info) {
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Allocate(size, alignment, debug_info);
		});
	}

	template<bool trigger_error_if_not_found>
	bool StackAllocator::Deallocate_ts(const void* block, DebugInfo debug_info) {
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Deallocate<trigger_error_if_not_found>(block, debug_info);
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, StackAllocator::Deallocate_ts, const void*, DebugInfo);

	void StackAllocator::SetMarker_ts() {
		ThreadSafeFunctor(&m_spin_lock, [&]() {
			SetMarker();
		});
	}

	void StackAllocator::ReturnToMarker_ts(DebugInfo debug_info) {	
		ThreadSafeFunctor(&m_spin_lock, [&]() {
			ReturnToMarker(debug_info);
		});
	}

}