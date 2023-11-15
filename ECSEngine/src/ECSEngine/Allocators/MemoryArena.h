#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorTypes.h"

#define ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE 4

namespace ECSEngine {

	struct ECSENGINE_API MemoryArena
	{
		ECS_INLINE MemoryArena() {
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

		void* Allocate(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		bool Belongs(const void* buffer) const;

		// Deallocates everything (as if nothing is allocated)
		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		// Return the original buffer given
		const void* GetAllocatedBuffer() const;

		bool IsEmpty() const;

		ECS_INLINE void Lock() {
			m_lock.Lock();
		}

		ECS_INLINE void Unlock() {
			m_lock.Unlock();
		}

		ECS_INLINE size_t InitialArenaCapacity() const {
			return m_size_per_allocator * (size_t)m_allocator_count;
		}

		AllocatorPolymorphic GetAllocator(size_t index) const;

		size_t GetCurrentUsage() const;

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate(const void* block, size_t new_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		void ExitDebugMode();

		void SetDebugMode(const char* name = nullptr, bool resizable = false);

		// ----------------------------------------  Thread safe --------------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);
		
		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate_ts(const void* block, size_t new_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		static size_t MemoryOf(size_t allocator_count, CreateBaseAllocatorInfo info);

		static size_t MemoryOf(size_t allocator_count, size_t total_capacity, ECS_ALLOCATOR_TYPE allocator_type);

		void* m_allocators;
		// This buffer is not actually the buffer that was given in the constructor but instead the starting pointer of the
		// memory that the allocators start allocating from. Used by Belongs and Deallocate functions
		void* m_data_buffer;
		size_t m_size_per_allocator;
		SpinLock m_lock;
		unsigned char m_allocator_count;
		bool m_debug_mode;
		ECS_ALLOCATOR_TYPE m_base_allocator_type;
		unsigned char m_current_index;
		unsigned short m_base_allocator_byte_size;
	};

}

