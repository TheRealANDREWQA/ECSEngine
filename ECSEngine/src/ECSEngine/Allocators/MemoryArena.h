#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "MemoryManager.h"

#define ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE 4

namespace ECSEngine {

	// Stream and Resizable stream are not used here because they will create a cycle with AllocatorPolymorphic
	// which is included in Stream.h

	struct ECSENGINE_API MemoryArena
	{
		MemoryArena();
		// arena_buffer is the memory to hold the multipool allocators, should be determined with MemoryOf
		MemoryArena(
			void* buffer,
			size_t capacity,
			size_t allocator_count,
			size_t blocks_per_allocator
		);

		MemoryArena(const MemoryArena& other) = default;
		MemoryArena& operator = (const MemoryArena& other) = default;

		void* Allocate(size_t size, size_t alignment = 8);

		bool Belongs(const void* buffer) const;

		// Deallocates everything (as if nothing is allocated)
		void Clear();

		// Return the original buffer given
		const void* GetAllocatedBuffer() const;

		bool IsEmpty() const;

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block);

		// ----------------------------------------  Thread safe --------------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);
		
		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block);

		static size_t MemoryOfArenas(size_t allocator_count, size_t blocks_per_allocator);

		static size_t MemoryOf(size_t capacity, size_t allocator_count, size_t blocks_per_allocator);

		MultipoolAllocator* m_allocators;
		size_t m_allocator_count;
		// This buffer is not actually the buffer that was given in the constructor but instead the starting pointer of the
		// memory that the allocators start allocating from. Used by Belongs and Deallocate functions
		void* m_initial_buffer;
		unsigned int m_size_per_allocator;
		unsigned int m_current_index;
		SpinLock m_lock;
	};

	struct ECSENGINE_API ResizableMemoryArena {
	//public:
		ResizableMemoryArena();
		ResizableMemoryArena(
			GlobalMemoryManager* backup, 
			size_t initial_arena_capacity, 
			size_t initial_allocator_count,
			size_t initial_blocks_per_allocator,
			size_t new_arena_capacity,
			size_t new_allocator_count,
			size_t new_blocks_per_allocator
		);

		ResizableMemoryArena(const ResizableMemoryArena& other) = default;
		ResizableMemoryArena& operator = (const ResizableMemoryArena& other) = default;

		void* Allocate(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block);

		bool Belongs(const void* buffer) const;

		// Deallocates all arenas besides the first one.
		void Clear();

		void CreateArena();

		void CreateArena(size_t arena_capacity, size_t allocator_count, size_t blocks_per_allocator);

		void DeallocateArena(size_t index);

		// Deallocates everythign
		void Free();

		// ------------------------------------------------- Thread safe ----------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block);

	//private:
		GlobalMemoryManager* m_backup;
		MemoryArena* m_arenas;
		size_t m_arena_size;
		size_t m_arena_capacity;
		size_t m_new_arena_capacity;
		size_t m_new_allocator_count;
		size_t m_new_blocks_per_allocator;
		SpinLock m_lock;
	};

}

