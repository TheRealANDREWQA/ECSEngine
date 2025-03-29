#include "ecspch.h"
#include "../Utilities/PointerUtilities.h"
#include "MultipoolAllocator.h"
#include "AllocatorCallsDebug.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {
	
	static size_t CalculateBlockRangeSize(const MultipoolAllocator* allocator, size_t size) {
		return AlignPointer(size, (size_t)1 << allocator->m_power_of_two_factor) >> allocator->m_power_of_two_factor;
	}

	MultipoolAllocator::MultipoolAllocator(void* buffer, size_t size, size_t pool_count) : MultipoolAllocator(buffer, OffsetPointer(buffer, size), size, pool_count) {}

	MultipoolAllocator::MultipoolAllocator(void* buffer, void* block_range_buffer, size_t size, size_t pool_count)
		: AllocatorBase(ECS_ALLOCATOR_MULTIPOOL), m_buffer((unsigned char*)buffer), m_size(size) {
		// Determine the power of two factor necessary
		size_t addressable_range = ECS_GB * 4;
		m_power_of_two_factor = 0;
		while (addressable_range < size) {
			addressable_range <<= 1;
			m_power_of_two_factor++;
		}
		// We need to cap the size according to the power of two size
		size >>= m_power_of_two_factor;

		m_range = BlockRange((unsigned int*)block_range_buffer, pool_count, size);
	}
	
	void* MultipoolAllocator::Allocate(size_t size, size_t alignment, DebugInfo debug_info) {
		ECS_ASSERT(alignment <= ECS_CACHE_LINE_SIZE);
		if (size > m_size) {
			// Early exit if it is too large
			ECS_ASSERT(!m_crash_on_allocation_failure, "MultipoolAllocator: single allocation exceeds the entire allocator's capacity");
			return nullptr;
		}

		// To make sure that the allocation is aligned we are requesting a surplus of alignment from the block range.
		// Then we are aligning the pointer like the stack one and placing the offset byte before the allocation

		size_t total_request_size = size + alignment;
		// Use the power of two factor to determine how big the range request has to be
		total_request_size = CalculateBlockRangeSize(this, total_request_size);

		unsigned int index = m_range.Request(total_request_size);

		if (index == 0xFFFFFFFF)
		{
			ECS_ASSERT(!m_crash_on_allocation_failure, "MultipoolAllocator capacity was exceeded");
			return nullptr;
		}

		size_t actual_offset = (size_t)index << m_power_of_two_factor;
		uintptr_t allocation = AlignPointerStack((uintptr_t)m_buffer + actual_offset, alignment);
		size_t offset = allocation - (uintptr_t)m_buffer;
		m_buffer[offset - 1] = offset - actual_offset - 1;

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

	bool MultipoolAllocator::DeallocateNoAssert(const void* block, DebugInfo debug_info) {
		// Calculating the start index of the block by reading the byte metadata 
		uintptr_t byte_offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		unsigned char byte_offset = m_buffer[byte_offset_position];
		if (byte_offset >= ECS_CACHE_LINE_SIZE) {
			// Exit if the alignment is very high
			return false;
		}

		// We need to take into account the power of two factor
		size_t block_start = (byte_offset_position - byte_offset) >> m_power_of_two_factor;
		bool was_deallocated = m_range.Free<false>(block_start);
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

	void* MultipoolAllocator::Reallocate(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		if (new_size > m_size) {
			// Early exit if it is too large
			ECS_ASSERT(!m_crash_on_allocation_failure, "MultipoolAllocator: single allocation exceeds the entire allocator's capacity");
			return nullptr;
		}

		uintptr_t byte_offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		ECS_ASSERT(m_buffer[byte_offset_position] < ECS_CACHE_LINE_SIZE);
		size_t block_start = (byte_offset_position - (size_t)m_buffer[byte_offset_position]) >> m_power_of_two_factor;
		size_t new_request_size = CalculateBlockRangeSize(this, new_size + alignment);

		unsigned int new_start = m_range.ReallocateBlock(block_start, new_request_size);
		if (new_start != -1) {
			size_t size_t_new_start = (size_t)new_start << m_power_of_two_factor;
			uintptr_t allocation = AlignPointerStack((uintptr_t)m_buffer + size_t_new_start, alignment);
			if (size_t_new_start != block_start) {
				unsigned int offset = allocation - (uintptr_t)m_buffer;
				m_buffer[offset - 1] = offset - size_t_new_start - 1;
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
		ECS_ASSERT(!m_crash_on_allocation_failure, "MultipoolAllocator reallocate request cannot be fulfilled");
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

	size_t MultipoolAllocator::MemoryOf(size_t pool_count, size_t size) {
		return BlockRange::MemoryOf(pool_count) + size + 8;
	}

	size_t MultipoolAllocator::CapacityFromFixedSize(size_t fixed_size, size_t pool_count)
	{
		size_t block_range_size = BlockRange::MemoryOf(pool_count);
		if (fixed_size > block_range_size + 8) {
			return fixed_size - block_range_size - 8;
		}
		ECS_ASSERT(false, "Multipool allocator size is too small - the block range occupies it all");
		return 0;
	}

	size_t MultipoolAllocator::MemoryOf(size_t pool_count) {
		return BlockRange::MemoryOf(pool_count);
	}

	size_t MultipoolAllocator::GetRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const
	{
		if (pointer_capacity >= 1) {
			*region_start = GetAllocatedBuffer();
			*region_size = GetSize();
		}
		return 1;
	}

}
