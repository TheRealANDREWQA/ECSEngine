#pragma once
#include "../Core.h"
#include "MultipoolAllocator.h"
#include "../Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	struct ECSENGINE_API GlobalMemoryManager
	{
		// size is the initial size that will be allocated
		GlobalMemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size);

		GlobalMemoryManager& operator = (const GlobalMemoryManager& other) = default;

		void* Allocate(size_t size, size_t alignment = 8);

		// Deallocates all the "extra allocators" and keeps only the first one with no allocations
		// After the function the allocator is as if there was no allocation made
		void Clear();

		void CreateAllocator(size_t size, size_t maximum_pool_count);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block);
		
		void DeallocateAllocator(size_t index);

		// Returns true if it did deallocate it
		bool DeallocateIfBelongs(const void* block);

		// Removes the allocators that have currently no allocations active
		void Trim();

		void Free();

		bool Belongs(const void* buffer) const;

		// ----------------------------------------------------- Thread safe ---------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block);
	
		SpinLock m_spin_lock;
		MultipoolAllocator* m_allocators;
		size_t m_allocator_count;
		size_t m_new_allocation_size;
		size_t m_maximum_pool_count;
	};
	
	struct ECSENGINE_API MemoryManager
	{
		MemoryManager();
		MemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size, GlobalMemoryManager* backup);
		
		MemoryManager& operator = (const MemoryManager& other) = default;
		
		void* Allocate(size_t size, size_t alignment = 8);

		void CreateAllocator(size_t size, size_t maximum_pool_count);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block);

		// Returns true if it did deallocate it
		bool DeallocateIfBelongs(const void* block);

		void DeallocateAllocator(size_t index);

		// Deallocates all the extra buffers, keeps only the first one and resets the allocations such that there are none
		void Clear();

		// Deallocates all of its buffers
		void Free();

		// Locks the SpinLock
		void Lock();

		// Unlocks the SpinLock
		void Unlock();

		// Removes the last allocators if they have currently no allocations active
		void Trim();

		bool Belongs(const void* buffer) const;

		// ---------------------------------------------------- Thread safe --------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block);
	
		SpinLock m_spin_lock;
		MultipoolAllocator* m_allocators;
		size_t m_allocator_count;
		size_t m_new_allocation_size;
		size_t m_maximum_pool_count;
		GlobalMemoryManager* m_backup;
	};
}
