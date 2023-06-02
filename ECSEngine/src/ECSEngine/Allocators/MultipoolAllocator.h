#pragma once
#include "../Core.h"
#include "../Containers/BlockRange.h"
#include "../Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	/* General allocator that uses a block range to keep track of available chunks of memory 
	* To avoid fragmentation it is recommended to request big chunks of memory and to keep the number of 
	* allocations as low as possible. The parameter pool_count given to the constructor represents the maximum 
	* number of pools that it can track of, including the ones occupied.
	*/
	class ECSENGINE_API MultipoolAllocator
	{
	public:
		MultipoolAllocator();
		MultipoolAllocator(unsigned char* buffer, size_t size, size_t pool_count);
		MultipoolAllocator(unsigned char* buffer, void* block_range_buffer, size_t size, size_t pool_count);
		
		MultipoolAllocator& operator = (const MultipoolAllocator& other) = default;
		
		void* Allocate(size_t size, size_t alignment = 8);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block);

		void* Reallocate(const void* block, size_t new_size, size_t alignment = 8);

		void* ReallocateAndCopy(const void* block, size_t new_size, size_t alignment = 8);

		void Clear();

		// Returns whether or not there is something currently allocated from this allocator
		bool IsEmpty() const;

		void* GetAllocatedBuffer() const;

		size_t GetSize() const;

		bool Belongs(const void* buffer) const;

		// --------------------------------------------------- Thread safe variants ------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block);

		void* Reallocate_ts(const void* block, size_t new_size, size_t alignment = 8);

		void* ReallocateAndCopy_ts(const void* block, size_t new_size, size_t alignment = 8);

		static size_t MemoryOf(unsigned int pool_count);
		static size_t MemoryOf(unsigned int pool_count, unsigned int size);

	//private:
		unsigned char* m_buffer;
		size_t m_size;
		SpinLock m_spin_lock;
		BlockRange m_range;
	};

}

