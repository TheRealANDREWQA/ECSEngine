#include "ecspch.h"
#include "WorldProfiling.h"
#include "CPUFrameProfilerGlobal.h"
#include "AllocatorProfilingGlobal.h"
#include "../ECS/World.h"
#include "../ECS/ArchetypeQueryCache.h"

namespace ECSEngine {

	template<typename Functor>
	void ForEachProfiler(WorldProfiling* profiling, Functor&& functor) {
		if (HasFlag(profiling->options, ECS_WORLD_PROFILING_CPU)) {
			functor(&profiling->cpu_profiler);
		}
		if (HasFlag(profiling->options, ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
			functor(&profiling->physical_memory_profiler);
		}
		if (HasFlag(profiling->options, ECS_WORLD_PROFILING_ALLOCATOR)) {
			functor(&profiling->allocator_profiler);
		}
		if (HasFlag(profiling->options, ECS_WORLD_PROFILING_GPU)) {

		}
	}

	void WorldProfiling::Clear()
	{
		ForEachProfiler(this, [](auto* profiler) {
			profiler->Clear();
		});
	}

	void WorldProfiling::ChangeGlobals()
	{
		if (HasFlag(options, ECS_WORLD_PROFILING_CPU)) {
			ChangeCPUFrameProfilerGlobal(&cpu_profiler);
		}
		if (HasFlag(options, ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
			// No global here
		}
		if (HasFlag(options, ECS_WORLD_PROFILING_ALLOCATOR)) {
			ChangeAllocatorProfilingGlobal(&allocator_profiler);
		}
		if (HasFlag(options, ECS_WORLD_PROFILING_GPU)) {

		}
	}

	void WorldProfiling::DisableOption(ECS_WORLD_PROFILING_OPTIONS option)
	{
		options = (ECS_WORLD_PROFILING_OPTIONS)ClearFlag(options, option);
	}

	void WorldProfiling::EnableOption(ECS_WORLD_PROFILING_OPTIONS option)
	{
		options = (ECS_WORLD_PROFILING_OPTIONS)SetFlag(options, option);
	}

	void WorldProfiling::EndSimulation()
	{
		if (HasFlag(options, ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
			physical_memory_profiler.EndSimulation();
		}
		if (HasFlag(options, ECS_WORLD_PROFILING_ALLOCATOR)) {
			allocator_profiler.ExitAllocators();
		}
	}

	void WorldProfiling::EndFrame()
	{
		ForEachProfiler(this, [](auto* profiler) {
			profiler->EndFrame();
		});
		in_frame = false;
	}

	void WorldProfiling::StartSimulation()
	{
		// Clear all profilers
		Clear();
		if (HasFlag(options, ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
			physical_memory_profiler.StartSimulation(world);
		}
		// For the allocator profiler, we also need to reinitialize from the world, if it is there
		if (HasFlag(options, ECS_WORLD_PROFILING_ALLOCATOR)) {
			if (world != nullptr) {
				AddAllocatorProfilingWorldAllocators(&allocator_profiler, world);
			}
		}
	}

	void WorldProfiling::StartFrame()
	{
		// Change the global profilers to these ones
		ChangeGlobals();
		ForEachProfiler(this, [](auto* profiler) {
			profiler->StartFrame();
		});
		in_frame = true;
	}

	void WorldProfiling::Initialize(const WorldProfilingInitializeDescriptor* descriptor)
	{
		if (HasFlag(descriptor->init_options, ECS_WORLD_PROFILING_CPU)) {
			size_t arena_size = descriptor->cpu_thread_arena_capacity == 0 ? ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_CAPACITY : descriptor->cpu_thread_arena_capacity;
			size_t backup_size = descriptor->cpu_thread_arena_backup_capacity == 0 ? ECS_CPU_FRAME_PROFILER_THREAD_ALLOCATOR_BACKUP_CAPACITY 
				: descriptor->cpu_thread_arena_backup_capacity;
			ECS_ASSERT(descriptor->task_manager != nullptr);

			cpu_profiler.Initialize(
				descriptor->allocator, 
				descriptor->task_manager, 
				descriptor->entry_capacity, 
				arena_size,
				backup_size
			);
		}
		if (HasFlag(descriptor->init_options, ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
			physical_memory_profiler.Initialize(descriptor->allocator, descriptor->entry_capacity);
		}
		if (HasFlag(descriptor->init_options, ECS_WORLD_PROFILING_ALLOCATOR)) {
			allocator_profiler.Initialize(descriptor->allocator, descriptor->entry_capacity);
		}
		if (HasFlag(descriptor->init_options, ECS_WORLD_PROFILING_GPU)) {

		}

		world = descriptor->world;
		options = descriptor->starting_options;
		in_frame = false;
	}

	size_t WorldProfiling::AllocatorSize(
		unsigned int thread_count, 
		unsigned int entry_capacity, 
		size_t cpu_thread_arena_capacity, 
		size_t cpu_thread_arena_backup_capacity
	)
	{
		return CPUFrameProfilerAllocatorReserve(thread_count, cpu_thread_arena_capacity, cpu_thread_arena_backup_capacity) +
			AllocatorProfilingEstimateAllocatorSize(thread_count, entry_capacity) + PhysicalMemoryProfilerAllocatorSize(entry_capacity);
	}

	void AddAllocatorProfilingWorldAllocators(AllocatorProfiling* allocator_profiler, const World* world) {
		// Add the global memory, the entity manager allocators (all, the main one, the small one and the temporary one),
		// the entity pool, the entity hierarchy, the entity query cache, the graphics allocator, the resource manager
		// allocator, the thread linear and dynamic allocators from the task manager, the task scheduler's allocator
		// and finally the debug drawer allocator
		AllocatorProfiling* previous_global = ECS_ALLOCATOR_PROFILING_GLOBAL;
		ChangeAllocatorProfilingGlobal(allocator_profiler);

		world->memory->SetProfilingMode("World Global");
		world->entity_manager->m_memory_manager->SetProfilingMode("Entity Manager");
		world->entity_manager->m_small_memory_manager.SetProfilingMode("Entity Manager Small");
		world->entity_manager->m_temporary_allocator.SetProfilingMode("Entity Manager Temporary");
		world->entity_manager->m_entity_pool->m_memory_manager->SetProfilingMode("Entity Pool");
		world->entity_manager->m_hierarchy_allocator->SetProfilingMode("Entity Hierarchy");
		SetAllocatorProfilingMode(world->entity_manager->m_query_cache->allocator, "Entity Query Cache");
		world->graphics->m_allocator->SetProfilingMode("Graphics");
		world->resource_manager->m_memory->SetProfilingMode("Resource Manager");
		unsigned int thread_count = world->task_manager->GetThreadCount();
		ECS_STACK_CAPACITY_STREAM(char, thread_allocator_name, 512);
		for (unsigned int index = 0; index < thread_count; index++) {
			thread_allocator_name.size = 0;
			ECS_FORMAT_STRING(thread_allocator_name, "Thread {#} Linear", index);
			world->task_manager->m_thread_linear_allocators[index]->SetProfilingMode(thread_allocator_name.buffer);

			thread_allocator_name.size = 0;
			ECS_FORMAT_STRING(thread_allocator_name, "Thread {#} Dynamic", index);
			world->task_manager->m_dynamic_task_allocators[index]->SetProfilingMode(thread_allocator_name.buffer);
		}
		SetAllocatorProfilingMode(world->task_scheduler->Allocator(), "Task Scheduler");
		if (world->debug_drawer != nullptr) {
			SetAllocatorProfilingMode(world->debug_drawer->Allocator(), "Debug Drawer");
		}

		ChangeAllocatorProfilingGlobal(previous_global);
	}

}