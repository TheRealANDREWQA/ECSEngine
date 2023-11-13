#pragma once
#include "../Core.h"
#include "Statistic.h"
#include "../Containers/HashTable.h"
#include "../Containers/Stacks.h"
#include "../Allocators/MemoryManager.h"
#include "../Utilities/Timer.h"

namespace ECSEngine {

#define ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY ECS_KB * 512
#define ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY ECS_KB * 512

	struct ECSENGINE_API CPUFrameProfilerEntry {
		ECS_INLINE void Add(float value) {
			statistic.Add(value);
		}

		ECS_INLINE void Clear() {
			statistic.Clear();
		}

		void Initialize(AllocatorPolymorphic allocator, Stream<char> name, unsigned int entry_capacity, unsigned char tag = 0);

		// At the moment represent everything as a float
		// Since they provide suffice precision for normal values
		Statistic<float> statistic;
		Stream<char> name;
		// This value is undefined - its meaning is totally up to the user
		// You can tag certain entries in their own way and display them accordingly
		// or filter by them
		unsigned char tag = 0;
	};

	struct ECSENGINE_API CPUFrameProfilerThread {
		AllocatorPolymorphic Allocator() const;

		void Clear();

		void EndFrame();

		// Adds a new entry, either as a root or as a child of a current root
		void Push(Stream<char> name, unsigned int entry_capacity, unsigned char tag = 0);

		// Finalizes an entry
		void Pop(float value);

		void Initialize(
			AllocatorPolymorphic backup_allocator, 
			size_t arena_capacity, 
			size_t arena_backup_capacity, 
			unsigned int entry_capacity, 
			void* thread_handle
		);

		// This uses a memory arena as a basic allocator
		// Since we will have many small allocations
		MemoryManager allocator;
		// Here we need multiple roots for a thread, each root can have a hierarchy of entries
		ResizableStream<ResizableStream<CPUFrameProfilerEntry>> roots;
		// This index is incremented during the frame run and it used to determine the current root
		unsigned int root_index;
		// This is the index of the child of the current root
		unsigned int child_index;
		// Expressed in milliseconds
		Statistic<float> frame_user_time;
		// Expressed in milliseconds
		Statistic<float> frame_kernel_time;
		size_t previous_frame_user_time;
		size_t previous_frame_kernel_time;
		void* thread_handle;
	private:
		// Padd the struct on a cache line boundary to avoid any possible false sharing happening between threads
		// Inside the manager
		unsigned char padding[24];
	};

	// Returns an estimate of how much memory this would use
	ECSENGINE_API size_t CPUFrameProfilerAllocatorReserve(
		unsigned int thread_count,
		size_t thread_arena_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY,
		size_t thread_arena_backup_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY
	);

	struct TaskManager;

	/*
		This is a profiler only for the CPU side of the frame. The GPU side profiler is not yet implemented
	*/
	struct ECSENGINE_API CPUFrameProfiler {
		// Percentage from 0 to 100. Includes both user and kernel times
		// If thread_id is set to -1, then it will calculate for all threads
		unsigned char CalculateUsage(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const;

		// Percentage from 0 to 100
		// If thread_id is set to -1, then it will calculate for all threads
		unsigned char CalculateUsageUser(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const;

		// Percentage from 0 to 100
		// If thread_id is set to -1, then it will calculate for all threads
		unsigned char CalculateUsageKernel(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const;

		// Percentage from 0 to 100. Calculates the percentage of time spent in user cycles
		// from the overall time spent
		// If thread_id is set to -1, then it will calculate for all threads
		unsigned char CalculateUserToOverall(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const;

		void Clear();

		void EndFrame();

		// The duration is in ms
		float GetSimulationFrameTime(ECS_STATISTIC_VALUE_TYPE value_type) const;

		// The duration is in ms
		float GetOverallFrameTime(ECS_STATISTIC_VALUE_TYPE value_type) const;

		// Adds a new entry, either as a root or as a child of a current root
		void Push(unsigned int thread_id, Stream<char> name, unsigned char tag = 0);

		void Pop(unsigned int thread_id, float value);

		void Initialize(
			AllocatorPolymorphic allocator, 
			Stream<void*> thread_handles,
			unsigned int entry_capacity,
			size_t thread_arena_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY,
			size_t thread_arena_backup_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY
		);

		void Initialize(
			AllocatorPolymorphic allocator,
			const TaskManager* task_manager,
			unsigned int entry_capacity,
			size_t thread_arena_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY,
			size_t thread_arena_backup_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY
		);

		void StartFrame();

		Stream<CPUFrameProfilerThread> threads;
		unsigned int entry_capacity;
		// Expressed in milliseconds
		Statistic<float> simulation_frame_time;
		// Expressed in milliseconds
		Statistic<float> overall_frame_time;

		// The timer is used to measure the simulation frame and the overall frame time
		// The start has the value that is set when a new frame is started
		// And the simulation frame time is calculated on EndFrame. The overall frame
		// time is deduced from the next StartFrame call which will determine the value from start
		Timer timer;
	};

}