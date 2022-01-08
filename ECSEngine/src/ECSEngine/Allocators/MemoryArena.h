#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"
#include "MemoryManager.h"

#define ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE 4

namespace ECSEngine {

	class ECSENGINE_API MemoryArena
	{
		friend class ResizableMemoryArena;
	public:
		MemoryArena();
		// arena_buffer is the memory to hold the multipool allocators, should be determined with MemoryOf
		MemoryArena(
			void* arena_buffer,
			void* buffer, 
			size_t capacity,
			size_t allocator_count,
			size_t blocks_per_allocator
		);

		MemoryArena(const MemoryArena& other) = default;
		MemoryArena& operator = (const MemoryArena& other) = default;

		void* Allocate(size_t size, size_t alignment = 8);
		template<bool trigger_error_if_not_found = true>
		void Deallocate(const void* block);

		// ----------------------------------------  Thread safe --------------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);
		
		template<bool trigger_error_if_not_found = true>
		void Deallocate_ts(const void* block);

		static size_t MemoryOf(size_t allocator_count, size_t blocks_per_allocator);

	private:
		containers::Stream<MultipoolAllocator> m_allocators;
		void* m_initial_buffer;
		unsigned int m_size_per_allocator;
		unsigned int m_current_index;
		SpinLock m_lock;
	};

	class ECSENGINE_API ResizableMemoryArena {
	public:
		ResizableMemoryArena();
		ResizableMemoryArena(
			GlobalMemoryManager* backup, 
			unsigned int initial_arena_capacity, 
			unsigned int initial_allocator_count,
			unsigned int initial_blocks_per_allocator,
			unsigned int new_arena_capacity,
			unsigned int new_allocator_count,
			unsigned int new_blocks_per_allocator
		);

		ResizableMemoryArena(const ResizableMemoryArena& other) = default;
		ResizableMemoryArena& operator = (const ResizableMemoryArena& other) = default;

		void* Allocate(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate(const void* block);

		void CreateArena();
		void CreateArena(unsigned int arena_capacity, unsigned int allocator_count, unsigned int blocks_per_allocator);

		// ------------------------------------------------- Thread safe ----------------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate_ts(const void* block);

	private:
		GlobalMemoryManager* m_backup;
		containers::ResizableStream<MemoryArena, GlobalMemoryManager> m_arenas;
		SpinLock m_lock;
		unsigned int m_new_arena_capacity;
		unsigned int m_new_allocator_count;
		unsigned int m_new_blocks_per_allocator;
	};

}

