#pragma once
#include "CPUFrameProfiler.h"
#include "PhysicalMemoryProfiler.h"
#include "AllocatorProfiling.h"

namespace ECSEngine {

	struct World;

	enum ECS_WORLD_PROFILING_OPTIONS : unsigned char {
		ECS_WORLD_PROFILING_NONE = 0,
		ECS_WORLD_PROFILING_CPU = 1 << 0,
		ECS_WORLD_PROFILING_PHYSICAL_MEMORY = 1 << 1,
		ECS_WORLD_PROFILING_ALLOCATOR = 1 << 2,
		ECS_WORLD_PROFILING_GPU = 1 << 3,

		ECS_WORLD_PROFILING_ALL = ECS_WORLD_PROFILING_CPU | ECS_WORLD_PROFILING_PHYSICAL_MEMORY | ECS_WORLD_PROFILING_ALLOCATOR | ECS_WORLD_PROFILING_GPU
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_WORLD_PROFILING_OPTIONS);

	struct WorldProfilingInitializeDescriptor {
		// ------------------------ Mandatory ----------------------------
		AllocatorPolymorphic allocator;
		unsigned int entry_capacity;
		ECS_WORLD_PROFILING_OPTIONS init_options;
		ECS_WORLD_PROFILING_OPTIONS starting_options;
		World* world;
		// ------------------------ Mandatory ----------------------------

		// ------------------------- Optional ----------------------------
		// For the CPU profiler
		const TaskManager* task_manager = nullptr;
		// If these are left at 0, it will use the defaults
		size_t cpu_thread_arena_capacity = 0;
		size_t cpu_thread_arena_backup_capacity = 0;
		// ------------------------- Optional ----------------------------
	};

	struct ECSENGINE_API WorldProfiling {
		// Clears all the enabled profilers
		void Clear();

		// Changes the global profilers to these ones from here
		void ChangeGlobals();

		// Disables some profilers
		void DisableOption(ECS_WORLD_PROFILING_OPTIONS option);

		// Enables some profilers
		void EnableOption(ECS_WORLD_PROFILING_OPTIONS option);

		// This is a once time off cleanup phase that needs to be done
		// After the simulation ends
		void EndSimulation();

		// Should be called after the frame
		void EndFrame();

		// This is a once time off initialization that needs to be done
		// Before the simulation starts
		void StartSimulation();

		// Should be called before the frame
		void StartFrame();

		// It will initialize the profilers according to the init_options, but set the options according to starting_options
		// The task manager is needed to if you want to initialize the CPU profiler, else it can be left as nullptr
		void Initialize(const WorldProfilingInitializeDescriptor* descriptor);

		static size_t AllocatorSize(
			unsigned int thread_count,
			unsigned int entry_capacity,
			size_t cpu_thread_arena_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY,
			size_t cpu_thread_arena_backup_capacity = ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY
		);

		World* world;
		CPUFrameProfiler cpu_profiler;
		PhysicalMemoryProfiler physical_memory_profiler;
		AllocatorProfiling allocator_profiler;
		ECS_WORLD_PROFILING_OPTIONS options;
		bool in_frame;
	};

	ECSENGINE_API void AddAllocatorProfilingWorldAllocators(AllocatorProfiling* allocator_profiler, const World* world);

	ECSENGINE_API void AddPhysicalMemoryWorldAllocators(PhysicalMemoryProfiler* physical_memory_profiler, const World* world);

}