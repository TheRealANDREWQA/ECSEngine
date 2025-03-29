#pragma once
#include "../Core.h"
#include "MultipoolAllocator.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

namespace ECSEngine {
	
	struct ECSENGINE_API MemoryManager final : public AllocatorBase
	{
		ECS_INLINE MemoryManager() : AllocatorBase(ECS_ALLOCATOR_MANAGER), m_backup(nullptr), m_allocators(nullptr), m_allocator_count(0) {}
		// This is a short hand for the multipool version
		MemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size, AllocatorPolymorphic backup);
		MemoryManager(CreateBaseAllocatorInfo initial_info, CreateBaseAllocatorInfo backup_info, AllocatorPolymorphic backup);
		
		MemoryManager(const MemoryManager& other) = default;
		// Override the operator such that the vtable is always copied
		ECS_INLINE MemoryManager& operator = (const MemoryManager& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}
		
		virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* AllocateTs(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssert(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssertTs(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* Reallocate(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* ReallocateTs(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		// Deallocates all the extra buffers, keeps only the first one and resets the allocations such that there are none
		virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) override;

		// Deallocates all of its buffers
		virtual void Free(bool assert_that_is_standalone = false, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		ECS_INLINE virtual void FreeFrom(AllocatorBase* backup_allocator, DebugInfo debug_info) override {
			// Simply ignore the parameter, use the backup that we have recorded
			Free(false, debug_info);
		}

		virtual size_t GetCurrentUsage() const override;

		virtual bool IsEmpty() const override;

		virtual void* GetAllocatedBuffer() const override {
			// It doesn't make sense for this allocator
			return nullptr;
		}

		virtual bool Belongs(const void* buffer) const override;

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		virtual size_t GetRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const override;

		// Removes the last allocators if they have currently no allocations active
		void Trim();
		
		void CreateAllocator(CreateBaseAllocatorInfo info);

		AllocatorPolymorphic GetAllocator(size_t index) const;

		void* GetAllocatorBasePointer(size_t index) const;

		void* GetAllocatorBasePointer(size_t index);

		size_t GetAllocatorBaseAllocationSize(size_t index) const;

		// For multipool base, it will report the highest byte currently in use,
		// Else 0
		size_t GetHighestOffsetInUse(size_t allocator_index) const;

		// Returns the base allocator info with which this instance was initialized with
		CreateBaseAllocatorInfo GetInitialAllocatorInfo() const;

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

	ECS_INLINE void CreateGlobalMemoryManager(GlobalMemoryManager* allocator, CreateBaseAllocatorInfo initial_info, CreateBaseAllocatorInfo backup_info) {
		new (allocator) GlobalMemoryManager(initial_info, backup_info, ECS_MALLOC_ALLOCATOR);
	}

	ECSENGINE_API void CreateGlobalMemoryManager(
		GlobalMemoryManager* allocator,
		size_t size,
		size_t maximum_pool_count,
		size_t new_allocation_size
	);

	// If left the values are left at 0, then it will use the initial values as backup as well
	ECSENGINE_API void CreateResizableMemoryArena(
		ResizableMemoryArena* allocator,
		size_t initial_arena_capacity,
		size_t initial_allocator_count,
		size_t initial_blocks_per_allocator,
		AllocatorPolymorphic backup,
		size_t arena_capacity = 0,
		size_t allocator_count = 0,
		size_t blocks_per_allocator = 0
	);

}
