#pragma once
#include "../Core.h"
#include "../Containers/BlockRange.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	/* General allocator that uses a block range to keep track of available chunks of memory 
	* To avoid fragmentation it is recommended to request big chunks of memory and to keep the number of 
	* allocations as low as possible. The parameter pool_count given to the constructor represents the maximum 
	* number of pools that it can track of, including the ones occupied.
	*/
	class ECSENGINE_API MultipoolAllocator
	{
	public:
		MultipoolAllocator() : m_buffer(nullptr), m_size(0), m_range(nullptr, 0, 0) {}
		MultipoolAllocator(unsigned char* buffer, size_t size, size_t pool_count)
			: m_buffer((unsigned char*)((uintptr_t)buffer + containers::BlockRange::MemoryOf(pool_count))),
			m_spinLock(), m_size(size), m_range((unsigned int*)buffer, pool_count, size) {
#ifdef ECSENGINE_DEBUG
			m_initial_buffer = m_buffer;
#endif
		}
		MultipoolAllocator(unsigned char* buffer, void* block_range_buffer, size_t size, size_t pool_count)
			: m_buffer(buffer), m_spinLock(), m_size(size),
			m_range((unsigned int*)block_range_buffer, pool_count, size) {
#ifdef ECSENGINE_DEBUG
			m_initial_buffer = m_buffer;
#endif
		}
		
		MultipoolAllocator& operator = (const MultipoolAllocator& other) = default;
		MultipoolAllocator& operator = (MultipoolAllocator&& other) = default;
		
		void* Allocate(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate(const void* block);

		void Clear();

		// --------------------------------------------------- Thread safe variants ------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate_ts(const void* block);

		void SetDebugBuffer(unsigned int* buffer);

		static size_t MemoryOf(unsigned int pool_count);
		static size_t MemoryOf(unsigned int pool_count, unsigned int size);

	private:
		unsigned char* m_buffer;
#ifdef ECSENGINE_DEBUG
		void* m_initial_buffer;
#endif
		size_t m_size;
		SpinLock m_spinLock;
		containers::BlockRange m_range;
	};

}

