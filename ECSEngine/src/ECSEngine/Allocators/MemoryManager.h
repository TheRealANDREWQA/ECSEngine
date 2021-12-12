#pragma once
#include "../Core.h"
#include "MultipoolAllocator.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	/* One instance should be created for each "World" */
	class ECSENGINE_API GlobalMemoryManager
	{
	public:
		// size is the initial size that will be allocated
		GlobalMemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size);

		GlobalMemoryManager& operator = (const GlobalMemoryManager& other) = default;

		void* Allocate(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate(const void* block);

		void CreateAllocator(size_t size, size_t maximum_pool_count);

		// Removes the allocators that have currently no allocations active
		void Trim();

		void ReleaseResources();

		// ----------------------------------------------------- Thread safe ---------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate_ts(const void* block);
	
	private:
		SpinLock m_spin_lock;
		void** m_buffers;
		size_t* m_buffers_capacity;
		MultipoolAllocator* m_allocators;
		size_t m_allocator_count;
		size_t m_new_allocation_size;
		size_t m_maximum_pool_count;
	};


	/* The interface to allocate memory for the application. Several can be instanced for finer grained
	control. Maximum number of allocators is set by ECS_MEMORY_MANAGER_SIZE.
	*/
	struct ECSENGINE_API MemoryManager
	{
	public:
		MemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size, GlobalMemoryManager* backup);

		MemoryManager& operator = (const MemoryManager& other) = default;
		
		void* Allocate(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate(const void* block);

		void CreateAllocator(size_t size, size_t maximum_pool_count);
		// deallocates its buffers
		void Free();

		void SetDebugBuffer(void* buffer, unsigned int group_count = 0);

		// Locks the SpinLock
		void Lock();

		// Unlocks the SpinLock
		void Unlock();

		// Removes the last allocators if they have currently no allocations active
		void Trim();

		// ---------------------------------------------------- Thread safe --------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate_ts(const void* block);
	
	//private:
		SpinLock m_spin_lock;
		void** m_buffers;
		size_t* m_buffers_capacity;
		MultipoolAllocator* m_allocators;
		size_t m_allocator_count;
		size_t m_new_allocation_size;
		size_t m_maximum_pool_count;
		GlobalMemoryManager* m_backup;
	};
}
