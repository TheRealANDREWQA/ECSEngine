#pragma once
#include "AllocatorBase.h"

namespace ECSEngine {

	struct ECSENGINE_API MallocAllocator final : AllocatorBase {
		ECS_INLINE MallocAllocator() : AllocatorBase(ECS_ALLOCATOR_MALLOC) {}

		// Delete this operator such that only the global instance exists
		MallocAllocator(const MallocAllocator& other) = delete;
		MallocAllocator& operator =(const MallocAllocator& other) = delete;

		virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* AllocateTs(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssert(const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssertTs(const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* Reallocate(const void* buffer, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* ReallocateTs(const void* buffer, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void Free(bool assert_that_is_standalone = true, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void FreeFrom(AllocatorBase* backup_allocator, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool Belongs(const void* buffer) const override;

		virtual void* GetAllocatedBuffer() const override;

		virtual bool IsEmpty() const override;

		virtual size_t GetCurrentUsage() const override;

		virtual size_t GetRegions(void** region_pointers, size_t* region_size, size_t pointer_capacity) const override;
	};

}