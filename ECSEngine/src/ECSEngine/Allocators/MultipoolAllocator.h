#pragma once
#include "../Core.h"
#include "../Containers/BlockRange.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

namespace ECSEngine {

	/* General allocator that uses a block range to keep track of available chunks of memory 
	* To avoid fragmentation it is recommended to request big chunks of memory and to keep the number of 
	* allocations as low as possible. The parameter pool_count given to the constructor represents the maximum 
	* number of pools that it can track of, including the ones occupied.
	*/
	struct ECSENGINE_API MultipoolAllocator final : public AllocatorBase
	{
		ECS_INLINE MultipoolAllocator() : AllocatorBase(ECS_ALLOCATOR_MULTIPOOL), m_buffer(nullptr), m_size(0), m_range(nullptr, 0, 0), m_power_of_two_factor(0) {}
		MultipoolAllocator(void* buffer, size_t size, size_t pool_count);
		MultipoolAllocator(void* buffer, void* block_range_buffer, size_t size, size_t pool_count);
		
		// Override the operator such that the vtable is always copied
		ECS_INLINE MultipoolAllocator& operator = (const MultipoolAllocator& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}
		
		// The Ts functions can use the default

		virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssert(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* Reallocate(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) override;

		ECS_INLINE virtual void Free(bool assert_that_is_standalone = false, DebugInfo debug_info = ECS_DEBUG_INFO) override {
			ECS_ASSERT(!assert_that_is_standalone, "MultipoolAllocator is not standalone!");
		}

		ECS_INLINE virtual void FreeFrom(AllocatorBase* backup_allocator, bool multithreaded_deallocation, DebugInfo debug_info = ECS_DEBUG_INFO) override {
			if (multithreaded_deallocation) {
				backup_allocator->DeallocateTs(GetAllocatedBuffer(), debug_info);
			}
			else {
				backup_allocator->Deallocate(GetAllocatedBuffer(), debug_info);
			}
		}

		// Returns whether or not there is something currently allocated from this allocator
		virtual bool IsEmpty() const override;

		ECS_INLINE virtual void* GetAllocatedBuffer() const override {
			return m_buffer;
		}

		ECS_INLINE size_t GetSize() const {
			return m_size;
		}

		virtual bool Belongs(const void* buffer) const override;

		ECS_INLINE virtual size_t GetCurrentUsage() const override {
			return m_range.GetCurrentUsage();
		}

		ECS_INLINE unsigned int GetBlockCount() const {
			return m_range.GetFreeBlockCount() + m_range.GetUsedBlockCount();
		}

		ECS_INLINE size_t GetHighestOffsetInUse() const {
			return m_range.GetHighestIndexInUse();
		}

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		virtual size_t GetRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const override;

		static size_t MemoryOf(size_t pool_count);

		static size_t MemoryOf(size_t pool_count, size_t size);
	
		// From a fixed size and a number of known block count, calculate the amount of memory it can reference
		static size_t CapacityFromFixedSize(size_t fixed_size, size_t pool_count);

		// We have this as unsigned char to make it easier to use the alignment stack trick
		// where we write before the allocation a byte telling us the real offset of the allocation
		unsigned char* m_buffer;
		size_t m_size;
		// This factor is used to support allocations greater than
		// The block range allows for. It represents the power of two
		// Multiplicative factor of a single byte i.e. a range of x
		// From the block range actually represents a range of x * power_of_two_factor
		// Of allocation bytes. The default is 0, which means 1 to 1 mapping
		size_t m_power_of_two_factor;
		BlockRange m_range;
	};

}

