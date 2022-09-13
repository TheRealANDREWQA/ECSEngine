#include "ecspch.h"
#include "ResizableLinearAllocator.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

#define RESIZE_FACTOR (1.5f)
#define MAX_BACKUPS 16

	// ---------------------------------------------------------------------------------

	ResizableLinearAllocator::ResizableLinearAllocator() : m_initial_buffer(nullptr), m_initial_capacity(0), m_allocated_buffers(nullptr), 
		m_allocated_buffer_capacity(0), m_allocated_buffer_size(0), m_top(0), m_marker(0), m_backup_size(0), m_backup({ nullptr }) {}

	ResizableLinearAllocator::ResizableLinearAllocator(void* buffer, size_t capacity, size_t backup_size, AllocatorPolymorphic allocator) 
		: m_top(0), m_marker(0), m_backup_size(backup_size), m_backup(allocator) 
	{
		ECS_ASSERT(capacity > MAX_BACKUPS * sizeof(void*));

		m_allocated_buffers = (void**)buffer;
		m_allocated_buffer_capacity = MAX_BACKUPS;
		m_allocated_buffer_size = 0;

		m_initial_buffer = function::OffsetPointer(buffer, sizeof(void*) * MAX_BACKUPS);
		m_initial_capacity = capacity - sizeof(void*) * MAX_BACKUPS;
	}

	void* ResizableLinearAllocator::Allocate(size_t size, size_t alignment)
	{
		if (m_top < m_initial_capacity) {
			uintptr_t ptr = (uintptr_t)m_initial_buffer + m_top;
			ptr = function::AlignPointer(ptr, alignment);

			size_t difference = function::PointerDifference((void*)(ptr + size), m_initial_buffer);
			if (difference <= m_initial_capacity) {
				m_top = difference;
				return (void*)ptr;
			}

			ECS_ASSERT(size + alignment < m_backup_size, "Trying to allocate a size bigger than the backup for a ResizableLinearAllocator.");

			void* allocation = AllocateEx(m_backup, m_backup_size);
			m_allocated_buffers[0] = allocation;

			ptr = (uintptr_t)m_allocated_buffers[0];
			ptr = function::AlignPointer(ptr, alignment);

			m_top = m_initial_capacity + function::PointerDifference((void*)ptr, m_allocated_buffers[0]) + size;
			return (void*)ptr;
		}
		else {
			size_t remaining_capacity = m_backup_size - (m_top - m_initial_capacity - (m_allocated_buffer_size - 1) * m_backup_size);
			if (remaining_capacity < size + alignment) {
				// Allocate another one
				ECS_ASSERT(m_allocated_buffer_size < m_allocated_buffer_capacity);

				void* allocation = AllocateEx(m_backup, m_backup_size);
				m_allocated_buffers[m_allocated_buffer_size++] = allocation;
			}

			uintptr_t ptr = (uintptr_t)m_allocated_buffers[m_allocated_buffer_size - 1];
			ptr = function::AlignPointer(ptr, alignment);

			m_top = m_initial_capacity + (m_allocated_buffer_size - 1) * m_backup_size +
				function::PointerDifference((void*)ptr, m_allocated_buffers[m_allocated_buffer_size - 1]) + size;
			return (void*)ptr;
		}
	}

	// ---------------------------------------------------------------------------------

	// Do nothing
	void ResizableLinearAllocator::Deallocate(const void* block) {}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::SetMarker()
	{
		m_marker = m_top;
	}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::ClearBackup()
	{
		for (size_t index = 0; index < m_allocated_buffer_size; index++) {
			DeallocateEx(m_backup, m_allocated_buffers[index]);
		}

		m_allocated_buffer_size = 0;

		if (m_top > m_initial_capacity) {
			m_top = m_initial_capacity - 1;
		}
	}

	void ResizableLinearAllocator::Clear()
	{
		ClearBackup();
		m_top = 0;
	}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::Free()
	{
		ClearBackup();
		DeallocateIfBelongs(m_backup, m_initial_buffer);
	}

	// ---------------------------------------------------------------------------------

	size_t ResizableLinearAllocator::GetMarker() const
	{
		return m_marker;
	}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::ReturnToMarker(size_t marker)
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
	}

	// ---------------------------------------------------------------------------------

	bool ResizableLinearAllocator::Belongs(const void* buffer) const
	{
		if (function::IsPointerRange(m_initial_buffer, m_initial_capacity, buffer)) {
			return true;
		}
		else {
			for (size_t index = 0; index < m_allocated_buffer_size; index++) {
				if (function::IsPointerRange(m_allocated_buffers[index], m_backup_size, buffer)) {
					return true;
				}
			}
		}

		return false;
	}

	// ---------------------------------------------------------------------------------

	void* ResizableLinearAllocator::Allocate_ts(size_t size, size_t alignment)
	{
		m_spin_lock.lock();
		void* allocation = Allocate(size, alignment);
		m_spin_lock.unlock();

		return allocation;
	}

	// ---------------------------------------------------------------------------------

	// Do nothing
	void ResizableLinearAllocator::Deallocate_ts(const void* block) {}

	// ---------------------------------------------------------------------------------

	void ResizableLinearAllocator::SetMarker_ts()
	{
		m_spin_lock.lock();
		SetMarker();
		m_spin_lock.unlock();
	}

	// ---------------------------------------------------------------------------------

}