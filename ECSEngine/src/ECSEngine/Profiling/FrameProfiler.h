#pragma once
#include "../Core.h"
#include "Statistic.h"
#include "../Containers/HashTable.h"
#include "../Containers/Stacks.h"
#include "../Allocators/MemoryManager.h"

namespace ECSEngine {

#define ECS_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY ECS_KB * 96
#define ECS_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY ECS_KB * 256

	struct ECSENGINE_API FrameProfilerEntry {
		ECS_INLINE void Add(float value) {
			statistic.Add(value);
		}

		void Initialize(AllocatorPolymorphic allocator, Stream<char> name, unsigned int entry_capacity, unsigned int min_values_capacity, unsigned char tag = 0);

		// At the moment represent everything as a float
		// Since they provide suffice precision for normal values
		Statistic<float> statistic;
		Stream<char> name;
		// This value is undefined - its meaning is totally up to the user
		// You can tag certain entries in their own way and display them accordingly
		// or filter by them
		unsigned char tag = 0;
	};

	struct ECSENGINE_API FrameProfilerThread {
		AllocatorPolymorphic Allocator() const;

		// Adds a new entry, either as a root or as a child of a current root
		void Push(Stream<char> name, unsigned int entry_capacity, unsigned int entry_min_value_capacity, unsigned char tag = 0);

		void Pop(float value);

		void Initialize(AllocatorPolymorphic backup_allocator, size_t arena_capacity, size_t arena_backup_capacity);

		ECS_INLINE void ResetFrame() {
			root_index = -1;
			child_index = 0;
		}

		// This uses a memory arena as a basic allocator
		// Since we will have many small allocations
		MemoryManager allocator;
		// Here we need multiple roots for a thread, each root can have a hierarchy of entries
		ResizableStream<ResizableStream<FrameProfilerEntry>> roots;
		// This index is incremented during the frame run and it used to determine the current root
		unsigned int root_index;
		// This is the index of the child of the current root
		unsigned int child_index;
	private:
		// Padd the struct on a cache line boundary to avoid any possible false sharing happening between threads
		// Inside the manager
		unsigned char padding[2 * ECS_CACHE_LINE_SIZE - sizeof(allocator) - sizeof(roots) - sizeof(root_index) - sizeof(child_index)];
	};

	// Returns an estimate of how much memory this would use
	ECSENGINE_API size_t FrameProfilerAllocatorReserve(
		unsigned int thread_count,
		size_t thread_arena_capacity = ECS_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY,
		size_t thread_arena_backup_capacity = ECS_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY
	);

	struct ECSENGINE_API FrameProfiler {
		// Adds a new entry, either as a root or as a child of a current root
		void Push(unsigned int thread_id, Stream<char> name, unsigned char tag = 0);

		void Pop(unsigned int thread_id, float value);

		void Initialize(
			AllocatorPolymorphic allocator, 
			unsigned int thread_count,
			unsigned int entry_capacity,
			unsigned int entry_min_value_capacity,
			size_t thread_arena_capacity = ECS_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY,
			size_t thread_arena_backup_capacity = ECS_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY
		);

		Stream<FrameProfilerThread> threads;
		unsigned int entry_capacity;
		unsigned int entry_min_value_capacity;
	};

}