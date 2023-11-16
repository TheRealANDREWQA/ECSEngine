#include "ecspch.h"
#include "ResizableLinearAllocator.h"
#include "../Utilities/PointerUtilities.h"
#include "AllocatorCallsDebug.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {

#define RESIZE_FACTOR (1.5f)
#define MAX_BACKUPS 16

	// ---------------------------------------------------------------------------------

	ResizableLinearAllocator::ResizableLinearAllocator(size_t capacity, size_t backup_size, AllocatorPolymorphic allocator)
		: ResizableLinearAllocator(ECSEngine::Allocate(allocator, capacity), capacity, backup_size, allocator) {}

	ResizableLinearAllocator::ResizableLinearAllocator(void* buffer, size_t capacity, size_t backup_size, AllocatorPolymorphic allocator) 
		: m_top(0), m_marker(0), m_backup_size(backup_size), m_backup(allocator), m_debug_mode(false), m_profiling_mode(false), m_current_usage(0)
	{
		ECS_ASSERT(capacity > MAX_BACKUPS * sizeof(void*), "Too small of a capacity for ResizableLinearAllocator");

		m_allocated_buffers = (void**)buffer;
		m_allocated_buffer_capacity = MAX_BACKUPS;
		m_allocated_buffer_size = 0;

		m_initial_buffer = OffsetPointer(buffer, sizeof(void*) * MAX_BACKUPS);
		m_initial_capacity = capacity - sizeof(void*) * MAX_BACKUPS;
	}

	void* ResizableLinearAllocator::Allocate(size_t size, size_t alignment, DebugInfo debug_info)
	{
		if (m_top < m_initial_capacity) {
			uintptr_t ptr = (uintptr_t)m_initial_buffer + m_top;
			ptr = AlignPointer(ptr, alignment);

			size_t difference = PointerDifference((void*)(ptr + size), m_initial_buffer);
			if (difference <= m_initial_capacity) {
				m_top = difference;
				return (void*)ptr;
			}

			ECS_ASSERT(size + alignment < m_backup_size, "Trying to allocate a size bigger than the backup for a ResizableLinearAllocator.");

			void* allocation = AllocateEx(m_backup, m_backup_size);
			m_allocated_buffers[0] = allocation;
			m_allocated_buffer_size++;

			ptr = (uintptr_t)m_allocated_buffers[0];
			ptr = AlignPointer(ptr, alignment);

			m_top = m_initial_capacity + PointerDifference((void*)ptr, m_allocated_buffers[0]) + size;
			m_current_usage = m_top;
			if (m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = (void*)ptr;
				tracked.debug_info = debug_info;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
				DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_RESIZABLE_LINEAR, &tracked);
			}
			if (m_profiling_mode) {
				AllocatorProfilingAddAllocation(this, GetCurrentUsage(), 0, m_allocated_buffer_size);
			}
			return (void*)ptr;
		}
		else {
			size_t current_buffer_allocated_size = m_top - m_initial_capacity - (m_allocated_buffer_size - 1) * m_backup_size;
			size_t remaining_capacity = m_backup_size - current_buffer_allocated_size;
			if (remaining_capacity < size + alignment) {
				// Allocate another one
				ECS_ASSERT(m_allocated_buffer_size < m_allocated_buffer_capacity);

				void* allocation = AllocateEx(m_backup, m_backup_size);
				m_allocated_buffers[m_allocated_buffer_size++] = allocation;
				current_buffer_allocated_size = 0;
			}

			uintptr_t ptr = (uintptr_t)m_allocated_buffers[m_allocated_buffer_size - 1];
			uintptr_t unaligned_pointer = ptr + current_buffer_allocated_size;
			ptr = AlignPointer(unaligned_pointer, alignment);

			m_top = m_initial_capacity + (m_allocated_buffer_size - 1) * m_backup_size +
				PointerDifference((void*)ptr, m_allocated_buffers[m_allocated_buffer_size - 1]) + size;
			// Update the current usage
			m_current_usage += size + ptr - unaligned_pointer;
			if (m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = (void*)ptr;
				tracked.debug_info = debug_info;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
				DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_RESIZABLE_LINEAR, &tracked);
			}
			if (m_profiling_mode) {
				AllocatorProfilingAddAllocation(this, GetCurrentUsage(), 0, m_allocated_buffer_size);
			}
			return (void*)ptr;
		}
	}

	// ---------------------------------------------------------------------------------

	// Do nothing
	template<bool trigger_error_if_not_found>
	bool ResizableLinearAllocator::Deallocate(const void* block, DebugInfo debug_info) {
		// Don't record deallocations for this type of allocator
		return true;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, ResizableLinearAllocator::Deallocate, const void*, DebugInfo);

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::SetMarker()
	{
		m_marker = m_top;
		m_marker_current_usage = m_current_usage;
	}

	// ---------------------------------------------------------------------------------

	template<bool disable_debug_info>
	void ClearBackupImpl(ResizableLinearAllocator* allocator, DebugInfo debug_info) {
		for (size_t index = 0; index < allocator->m_allocated_buffer_size; index++) {
			DeallocateEx(allocator->m_backup, allocator->m_allocated_buffers[index]);
		}

		allocator->m_allocated_buffer_size = 0;

		if (allocator->m_top > allocator->m_initial_capacity) {
			allocator->m_top = allocator->m_initial_capacity - 1;
			allocator->m_current_usage = allocator->m_top;
		}

		if constexpr (disable_debug_info) {
			if (allocator->m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = nullptr;
				tracked.debug_info = debug_info;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_CLEAR;
				DebugAllocatorManagerAddEntry(allocator, ECS_ALLOCATOR_RESIZABLE_LINEAR, &tracked);
			}
		}
	}

	void ResizableLinearAllocator::ClearBackup(DebugInfo debug_info)
	{
		ClearBackupImpl<false>(this, debug_info);
	}

	void ResizableLinearAllocator::Clear(DebugInfo debug_info)
	{
		ClearBackupImpl<true>(this, debug_info);
		m_top = 0;
		m_current_usage = 0;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = nullptr;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_CLEAR;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_RESIZABLE_LINEAR, &tracked);
		}
	}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::Free(DebugInfo debug_info)
	{
		ClearBackupImpl<true>(this, debug_info);
		// This is the buffer that was received in the constructor
		DeallocateIfBelongs(m_backup, m_allocated_buffers);

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = nullptr;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_FREE;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_RESIZABLE_LINEAR, &tracked);
		}
	}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::GetMarker(size_t* marker, size_t* usage) const
	{
		*marker = m_marker;
		*usage = m_marker_current_usage;
	}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::ReturnToMarker(size_t marker, size_t usage, DebugInfo debug_info)
	{
		if (marker < m_initial_capacity) {
			ClearBackup();
		}
		else {
			size_t difference = marker - m_initial_capacity;
			size_t allocators_to_keep = difference / m_backup_size + ((difference % m_backup_size) != 0);
			for (size_t index = allocators_to_keep; index < m_allocated_buffer_size; index++) {
				DeallocateEx(m_backup, m_allocated_buffers[index]);
			}

			m_allocated_buffer_size = allocators_to_keep;
		}

		m_top = marker;
		m_current_usage = usage;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = (void*)marker;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_RETURN_TO_MARKER;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_RESIZABLE_LINEAR, &tracked);
		}
	}

	// ---------------------------------------------------------------------------------

	bool ResizableLinearAllocator::Belongs(const void* buffer) const
	{
		if (IsPointerRange(m_initial_buffer, m_initial_capacity, buffer)) {
			return true;
		}
		else {
			for (size_t index = 0; index < m_allocated_buffer_size; index++) {
				if (IsPointerRange(m_allocated_buffers[index], m_backup_size, buffer)) {
					return true;
				}
			}
		}

		return false;
	}

	void ResizableLinearAllocator::ExitDebugMode()
	{
		m_debug_mode = false;
	}

	void ResizableLinearAllocator::ExitProfilingMode()
	{
		m_profiling_mode = false;
	}

	void ResizableLinearAllocator::SetDebugMode(const char* name, bool resizable)
	{
		m_debug_mode = true;
		DebugAllocatorManagerChangeOrAddEntry(this, name, resizable, ECS_ALLOCATOR_RESIZABLE_LINEAR);
	}

	void ResizableLinearAllocator::SetProfilingMode(const char* name)
	{
		m_profiling_mode = true;
		AllocatorProfilingAddEntry(this, ECS_ALLOCATOR_RESIZABLE_LINEAR, name);
	}

	// ---------------------------------------------------------------------------------

	void* ResizableLinearAllocator::Allocate_ts(size_t size, size_t alignment, DebugInfo debug_info)
	{
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Allocate(size, alignment, debug_info);
		});
	}

	// ---------------------------------------------------------------------------------

	// Do nothing
	template<bool trigger_error_if_not_found>
	bool ResizableLinearAllocator::Deallocate_ts(const void* block, DebugInfo debug_info) {
		return true;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, ResizableLinearAllocator::Deallocate_ts, const void*, DebugInfo);

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::SetMarker_ts()
	{
		return ThreadSafeFunctor(&m_spin_lock, [&]() {
			SetMarker();
		});
	}

	// ---------------------------------------------------------------------------------

}