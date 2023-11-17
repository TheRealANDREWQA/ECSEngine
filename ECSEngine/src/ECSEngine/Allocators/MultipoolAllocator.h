#pragma once
#include "../Core.h"
#include "../Containers/BlockRange.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"

namespace ECSEngine {

	/* General allocator that uses a block range to keep track of available chunks of memory 
	* To avoid fragmentation it is recommended to request big chunks of memory and to keep the number of 
	* allocations as low as possible. The parameter pool_count given to the constructor represents the maximum 
	* number of pools that it can track of, including the ones occupied.
	*/
	struct ECSENGINE_API MultipoolAllocator
	{
		ECS_INLINE MultipoolAllocator() : m_buffer(nullptr), m_size(0), m_range(nullptr, 0, 0), m_debug_mode(false), m_profiling_mode(false) {}
		MultipoolAllocator(void* buffer, size_t size, size_t pool_count);
		MultipoolAllocator(void* buffer, void* block_range_buffer, size_t size, size_t pool_count);
		
		MultipoolAllocator& operator = (const MultipoolAllocator& other) = default;
		
		void* Allocate(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate(const void* block, size_t new_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns whether or not there is something currently allocated from this allocator
		bool IsEmpty() const;

		ECS_INLINE void* GetAllocatedBuffer() const {
			return m_buffer;
		}

		ECS_INLINE size_t GetSize() const {
			return m_size;
		}

		bool Belongs(const void* buffer) const;

		void ExitDebugMode();

		void ExitProfilingMode();

		void SetDebugMode(const char* name = nullptr, bool resizable = false);

		void SetProfilingMode(const char* name);

		ECS_INLINE void Lock() {
			m_spin_lock.Lock();
		}

		ECS_INLINE void Unlock() {
			m_spin_lock.Unlock();
		}

		ECS_INLINE size_t GetCurrentUsage() const {
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
		size_t GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const;

		// --------------------------------------------------- Thread safe variants ------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate_ts(const void* block, size_t new_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		static size_t MemoryOf(unsigned int pool_count);
		static size_t MemoryOf(unsigned int pool_count, unsigned int size);
		// From a fixed size and a number of known block count, calculate the amount of memory it can reference
		static size_t CapacityFromFixedSize(unsigned int fixed_size, unsigned int pool_count);

		// We have this as unsigned char to make it easier to use the alignment stack trick
		// where we write before the allocation a byte telling us the real offset of the allocation
		unsigned char* m_buffer;
		size_t m_size;
		SpinLock m_spin_lock;
		bool m_debug_mode;
		bool m_profiling_mode;
		BlockRange m_range;
	};

}

