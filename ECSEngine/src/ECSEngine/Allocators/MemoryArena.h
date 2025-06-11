#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

#define ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE 4

namespace ECSEngine {

	struct ECSENGINE_API MemoryArena final : public AllocatorBase
	{
		ECS_INLINE MemoryArena() : AllocatorBase(ECS_ALLOCATOR_ARENA) {
			memset(this, 0, sizeof(*this));
		}

		// If the base_info allocator type is set to ARENA, it will fit
		// the nested allocator into the given entire capacity
		// Else it will determine the total size from the nested values
		MemoryArena(
			void* buffer,
			size_t allocator_count,
			CreateBaseAllocatorInfo base_info
		);

		// If the base_info allocator type is set to ARENA, it will fit
		// the nested allocator into the given entire capacity
		// Else it will determine the total size from the nested values
		MemoryArena(
			AllocatorPolymorphic buffer_allocator,
			size_t allocator_count,
			CreateBaseAllocatorInfo base_info
		);

		// Shorthand version for the multipool one
		MemoryArena(void* buffer, size_t allocator_count, size_t arena_capacity, size_t blocks_per_allocator);

		MemoryArena(const MemoryArena& other) = default;
		// Override the operator such that the vtable is always copied
		ECS_INLINE MemoryArena& operator = (const MemoryArena& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}

		virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;
		
		virtual void* AllocateTs(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssert(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssertTs(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual void* Reallocate(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;
		
		virtual void* ReallocateTs(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool Belongs(const void* buffer) const override;

		// Deallocates everything (as if nothing is allocated)
		virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) override;

		ECS_INLINE virtual void Free(bool assert_that_is_standalone = false, DebugInfo debug_info = ECS_DEBUG_INFO) override {
			ECS_ASSERT(!assert_that_is_standalone, "MemoryArena is not standalone!");
		}

		ECS_INLINE virtual void FreeFrom(AllocatorBase* backup_allocator, bool multithreaded_deallocation, DebugInfo debug_info = ECS_DEBUG_INFO) override {
			if (multithreaded_deallocation) {
				backup_allocator->DeallocateTs(GetAllocatedBuffer(), debug_info);
			}
			else {
				backup_allocator->Deallocate(GetAllocatedBuffer(), debug_info);
			}
		}

		// Return the original buffer given
		ECS_INLINE virtual void* GetAllocatedBuffer() const override {
			return m_allocators;
		}

		virtual bool IsEmpty() const override;

		virtual size_t GetCurrentUsage() const override;

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		virtual size_t GetRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const override;
		
		ECS_INLINE size_t InitialArenaCapacity() const {
			return m_size_per_allocator * (size_t)m_allocator_count;
		}

		AllocatorPolymorphic GetAllocator(size_t index) const;

		// Returns the base allocator info with which this instance was initialized with
		CreateBaseAllocatorInfo GetInitialBaseAllocatorInfo() const;

		static size_t MemoryOf(size_t allocator_count, CreateBaseAllocatorInfo info);

		static size_t MemoryOf(size_t allocator_count, size_t total_capacity, ECS_ALLOCATOR_TYPE allocator_type);

		void* m_allocators;
		// This buffer is not actually the buffer that was given in the constructor but instead the starting pointer of the
		// memory that the allocators start allocating from. Used by Belongs and Deallocate functions
		void* m_data_buffer;
		size_t m_size_per_allocator;
		unsigned char m_current_index;
		unsigned char m_allocator_count;
		// This is the type the type that is actually being used as base allocator
		ECS_ALLOCATOR_TYPE m_base_allocator_type;
		// This is the type that was passed in during the init/constructor call.
		// Used for serialization/deserialization purposes
		ECS_ALLOCATOR_TYPE m_base_allocator_creation_type;
		unsigned short m_base_allocator_byte_size;
		
		// These fields are added for serialization/deserialization purposes. They don't have any functional role,
		// Paying 8 extra bytes for this information is reasonable
		struct {
			// Used by both the arena base with nested multipool, or a simple multipool
			unsigned int m_base_allocator_creation_multipool_block_count;
		};
	};

}

