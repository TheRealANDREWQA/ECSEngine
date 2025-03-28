#pragma once
#include "../Core.h"
#include "MultipoolAllocator.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

namespace ECSEngine {
	
	struct ECSENGINE_API MemoryManager : public AllocatorBase
	{
		ECS_INLINE MemoryManager() : AllocatorBase(ECS_ALLOCATOR_MANAGER), m_backup({ nullptr }), m_allocators(nullptr), m_allocator_count(0) {}
		// This is a short hand for the multipool version
		MemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size, AllocatorPolymorphic backup);
		MemoryManager(CreateBaseAllocatorInfo initial_info, CreateBaseAllocatorInfo backup_info, AllocatorPolymorphic backup);
		
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(MemoryManager);
		
		void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		template<typename T>
		ECS_INLINE T* Allocate(DebugInfo debug_info = ECS_DEBUG_INFO) {
			return (T*)Allocate(sizeof(T), alignof(T), debug_info);
		}

		void CreateAllocator(CreateBaseAllocatorInfo info);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns true if it did deallocate it
		bool DeallocateIfBelongs(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		// Deallocates all the extra buffers, keeps only the first one and resets the allocations such that there are none
		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		// Deallocates all of its buffers
		void Free(DebugInfo debug_info = ECS_DEBUG_INFO);

		size_t GetCurrentUsage() const;

		bool IsEmpty() const;

		// Removes the last allocators if they have currently no allocations active
		void Trim();

		bool Belongs(const void* buffer) const;

		AllocatorPolymorphic GetAllocator(size_t index) const;

		void* GetAllocatorBasePointer(size_t index) const;

		void* GetAllocatorBasePointer(size_t index);

		size_t GetAllocatorBaseAllocationSize(size_t index) const;

		// For multipool base, it will report the highest byte currently in use,
		// Else 0
		size_t GetHighestOffsetInUse(size_t allocator_index) const;

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		size_t GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const;

		// Returns the base allocator info with which this instance was initialized with
		CreateBaseAllocatorInfo GetInitialAllocatorInfo() const;

		// ---------------------------------------------------- Thread safe --------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate_ts(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		bool DeallocateIfBelongs_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);
	
		// This field is not needed for this to function, it is used only for serialization/deserialization purposes
		// It is placed here to reduce the waste coming from padding bytes
		ECS_ALLOCATOR_TYPE m_initial_allocator_type;
		unsigned char m_allocator_count;
		// Cache this value such that we don't have to query it every single time
		unsigned short m_base_allocator_byte_size;
		// This field is not needed for this to function, it is used only for serialization/deserialization purposes
		// It is placed here to reduce the waste coming from padding bytes
		unsigned int m_initial_allocator_multipool_count;
		void* m_allocators;
		CreateBaseAllocatorInfo m_backup_info;
		AllocatorPolymorphic m_backup;
		// This is needed for the GetAllocatorBaseAllocationSize() to report correctly
		// For the first allocator
		size_t m_initial_allocator_size;
		
		// The following fields are not needed for this to function, they are used only for serialization/deserialization purposes
		ECS_ALLOCATOR_TYPE m_initial_allocator_nested_type;
		unsigned char m_initial_allocator_arena_count;
		unsigned int m_initial_allocator_multipool_block_count;
		// This is not exactly the same as m_initial_allocator_size
		size_t m_initial_allocator_capacity;
	};

	typedef MemoryManager GlobalMemoryManager;
	typedef MemoryManager ResizableMemoryArena;

	ECS_INLINE GlobalMemoryManager CreateGlobalMemoryManager(CreateBaseAllocatorInfo initial_info, CreateBaseAllocatorInfo backup_info) {
		return MemoryManager(initial_info, backup_info, { nullptr });
	}

	ECSENGINE_API GlobalMemoryManager CreateGlobalMemoryManager(
		size_t size,
		size_t maximum_pool_count,
		size_t new_allocation_size
	);

	// If left the values are left at 0, then it will use the initial values as backup as well
	ECSENGINE_API ResizableMemoryArena CreateResizableMemoryArena(
		size_t initial_arena_capacity,
		size_t initial_allocator_count,
		size_t initial_blocks_per_allocator,
		AllocatorPolymorphic backup,
		size_t arena_capacity = 0,
		size_t allocator_count = 0,
		size_t blocks_per_allocator = 0
	);

}
