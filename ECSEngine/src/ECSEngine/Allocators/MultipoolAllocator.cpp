#include "ecspch.h"
#include "../Utilities/PointerUtilities.h"
#include "MultipoolAllocator.h"
#include "AllocatorCallsDebug.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {
	
	MultipoolAllocator::MultipoolAllocator(void* buffer, size_t size, size_t pool_count)
		: m_buffer((unsigned char*)buffer), m_spin_lock(), m_size(size), m_range((unsigned int*)OffsetPointer(buffer, size), pool_count, size),
		m_debug_mode(false), m_profiling_mode(false) {}

	MultipoolAllocator::MultipoolAllocator(void* buffer, void* block_range_buffer, size_t size, size_t pool_count)
		: m_buffer((unsigned char*)buffer), m_spin_lock(), m_size(size), m_range((unsigned int*)block_range_buffer, pool_count, size),
		m_debug_mode(false), m_profiling_mode(false) {}
	
	void* MultipoolAllocator::Allocate(size_t size, size_t alignment, DebugInfo debug_info) {
		ECS_ASSERT(alignment <= ECS_CACHE_LINE_SIZE);
		if (size > m_size) {
			// Early exit if it is too large
			return nullptr;
		}

		// To make sure that the allocation is aligned we are requesting a surplus of alignment from the block range.
		// Then we are aligning the pointer like the stack one and placing the offset byte before the allocation
		unsigned int index = m_range.Request(size + alignment);

		if (index == 0xFFFFFFFF)
			return nullptr;

		uintptr_t allocation = AlignPointerStack((uintptr_t)m_buffer + index, alignment);
		size_t offset = allocation - (uintptr_t)m_buffer;
		ECS_ASSERT(offset - index - 1 < alignment);
		m_buffer[offset - 1] = offset - index - 1;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = (void*)allocation;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MULTIPOOL, &tracked);
		}

		if (m_profiling_mode) {
			AllocatorProfilingAddAllocation(this, GetCurrentUsage(), m_range.GetFreeBlockCount() + m_range.GetUsedBlockCount());
		}

		return (void*)allocation;
	}

	template<bool trigger_error_if_not_found>
	bool MultipoolAllocator::Deallocate(const void* block, DebugInfo debug_info) {
		// Calculating the start index of the block by reading the byte metadata 
		uintptr_t byte_offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		unsigned char byte_offset = m_buffer[byte_offset_position];
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(byte_offset < ECS_CACHE_LINE_SIZE);
		}
		else {
			if (byte_offset >= ECS_CACHE_LINE_SIZE) {
				// Exit if the alignment is very high
				return false;
			}
		}
		bool was_deallocated = m_range.Free<trigger_error_if_not_found>((unsigned int)(byte_offset_position - byte_offset));
		if (was_deallocated) {
			if (m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = block;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_DEALLOCATE;
				tracked.debug_info = debug_info;
				DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MULTIPOOL, &tracked);
			}
			if (m_profiling_mode) {
				AllocatorProfilingAddDeallocation(this);
			}
		}
		return was_deallocated;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MultipoolAllocator::Deallocate, const void*, DebugInfo);

	void* MultipoolAllocator::Reallocate(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		uintptr_t byte_offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		ECS_ASSERT(m_buffer[byte_offset_position] < ECS_CACHE_LINE_SIZE);
		unsigned int previous_start = (unsigned int)(byte_offset_position - m_buffer[byte_offset_position]);
		unsigned int new_start = m_range.ReallocateBlock(previous_start, new_size + alignment);
		if (new_start != -1) {
			uintptr_t allocation = AlignPointerStack((uintptr_t)m_buffer + new_start, alignment);
			if (new_start != previous_start) {
				unsigned int offset = allocation - (uintptr_t)m_buffer;
				ECS_ASSERT(offset - new_start - 1 < alignment);
				m_buffer[offset - 1] = offset - new_start - 1;
			}

			if (m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = block;
				tracked.secondary_pointer = (void*)allocation;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_REALLOCATE;
				tracked.debug_info = debug_info;
				DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MULTIPOOL, &tracked);
			}

			if (m_profiling_mode) {
				// Consider reallocations as a deallocate + allocation
				AllocatorProfilingAddAllocation(this, GetCurrentUsage(), m_range.GetFreeBlockCount() + m_range.GetUsedBlockCount());
				AllocatorProfilingAddDeallocation(this);
			}

			return (void*)allocation;
		}

		// The request cannot be fulfilled
		return nullptr;
	}

	void MultipoolAllocator::Clear(DebugInfo debug_info) {
		m_range.Clear();

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = nullptr;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_CLEAR;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MULTIPOOL, &tracked);
		}
	}

	bool MultipoolAllocator::IsEmpty() const
	{
		return m_range.GetUsedBlockCount() == 0;
	}

	bool MultipoolAllocator::Belongs(const void* buffer) const
	{
		uintptr_t ptr = (uintptr_t)buffer;
		return ptr >= (uintptr_t)m_buffer && ptr < (uintptr_t)m_buffer + m_size;
	}

	void MultipoolAllocator::ExitDebugMode()
	{
		m_debug_mode = false;
	}

	void MultipoolAllocator::ExitProfilingMode()
	{
		m_profiling_mode = false;
	}

	void MultipoolAllocator::SetDebugMode(const char* name, bool resizable)
	{
		m_debug_mode = true;
		DebugAllocatorManagerChangeOrAddEntry(this, name, resizable, ECS_ALLOCATOR_MULTIPOOL);
	}

	void MultipoolAllocator::SetProfilingMode(const char* name)
	{
		m_profiling_mode = true;
		AllocatorProfilingAddEntry(this, ECS_ALLOCATOR_MULTIPOOL, name);
	}

	size_t MultipoolAllocator::MemoryOf(unsigned int pool_count, unsigned int size) {
		return BlockRange::MemoryOf(pool_count) + size + 8;
	}

	size_t MultipoolAllocator::CapacityFromFixedSize(unsigned int fixed_size, unsigned int pool_count)
	{
		size_t block_range_size = BlockRange::MemoryOf(pool_count);
		if (fixed_size > block_range_size + 8) {
			return fixed_size - block_range_size - 8;
		}
		return 0;
	}

	size_t MultipoolAllocator::MemoryOf(unsigned int pool_count) {
		return BlockRange::MemoryOf(pool_count);
	}

	// ---------------------- Thread safe variants -----------------------------

	void* MultipoolAllocator::Allocate_ts(size_t size, size_t alignment, DebugInfo debug_info) {
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Allocate(size, alignment, debug_info);
		});
	}

	template<bool trigger_error_if_not_found>
	bool MultipoolAllocator::Deallocate_ts(const void* block, DebugInfo debug_info) {
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Deallocate<trigger_error_if_not_found>(block, debug_info);
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MultipoolAllocator::Deallocate_ts, const void*, DebugInfo);

	void* MultipoolAllocator::Reallocate_ts(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		return ThreadSafeFunctorReturn(&m_spin_lock, [&]() {
			return Reallocate(block, new_size, alignment, debug_info);
		});
	}

}
