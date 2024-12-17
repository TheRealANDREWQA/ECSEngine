#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

#define ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE 4

namespace ECSEngine {

	struct ECSENGINE_API MemoryArena : public AllocatorBase
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
		MemoryArena& operator = (const MemoryArena& other) = default;

		void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		bool Belongs(const void* buffer) const;

		// Deallocates everything (as if nothing is allocated)
		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		// Return the original buffer given
		ECS_INLINE void* GetAllocatedBuffer() const {
			return m_allocators;
		}

		bool IsEmpty() const;

		ECS_INLINE size_t InitialArenaCapacity() const {
			return m_size_per_allocator * (size_t)m_allocator_count;
		}

		AllocatorPolymorphic GetAllocator(size_t index) const;

		// Returns the base allocator info with which this instance was initialized with
		CreateBaseAllocatorInfo GetInitialBaseAllocatorInfo() const;

		size_t GetCurrentUsage() const;

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		size_t GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const;

		// ----------------------------------------  Thread safe --------------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);
		
		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate_ts(const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

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

