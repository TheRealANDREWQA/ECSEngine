#pragma once
#include "../Core.h"
#include "../Utilities/DebugInfo.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "AllocatorBase.h"

namespace ECSEngine {

	struct ECSENGINE_API MemoryProtectedAllocator final : AllocatorBase {
		ECS_INLINE MemoryProtectedAllocator() : AllocatorBase(ECS_ALLOCATOR_MEMORY_PROTECTED) {
			memset(this, 0, sizeof(*this));
		}
		
		// If the chunk size is 0, then each allocation will be on a separate page.
		// If the chunk size is given, then it will make allocations of that chunk size
		// When the chunk size is given, you can specify if the allocations should be made
		// like a linear allocator or as a multipool allocator
		MemoryProtectedAllocator(size_t chunk_size, bool linear_chunks = false);

		// Override the operator such that the vtable is always copied
		MemoryProtectedAllocator(const MemoryProtectedAllocator& other) = default;
		ECS_INLINE MemoryProtectedAllocator& operator = (const MemoryProtectedAllocator& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}

		virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssert(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* Reallocate(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool Belongs(const void* block) const override;

		virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void Free(bool assert_that_is_standalone = false, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		ECS_INLINE virtual void FreeFrom(AllocatorBase* backup_allocator, DebugInfo debug_info = ECS_DEBUG_INFO) override {
			// Simply forward to the standalone call
			Free(false, debug_info);
		}

		virtual bool IsEmpty() const override;

		// Disables write protection for all allocations which were made by this allocator
		// Returns true if it succeeded, else false
		bool DisableWriteProtection() const;

		// Disables write protection for a specific allocation - this works only in non chunk mode
		// The block must have been allocated by this allocator, otherwise it will fail
		// Returns true if it succeeded, else false
		bool DisableWriteProtection(void* block) const;

		// Enables write protection for all allocations which were made by this allocator
		// Returns true if it suceeded, else false
		bool EnableWriteProtection() const;

		// Enables write protection for a specific allocation - this works only in non chunk mode
		// The block must have been allocated by this allocator, otherwise it will fail
		// Returns true if it suceeded, else false. 
		bool EnableWriteProtection(void* block) const;

		union {
			struct {
				void** allocators;
				bool linear_allocators;
			};
			struct {
				void** allocation_pointers;
			};
		};
		// This is the current count of allocators/allocations
		unsigned int count;
		// This is the capacity of allocators/allocations
		unsigned int capacity;
		size_t chunk_size;
	};

}