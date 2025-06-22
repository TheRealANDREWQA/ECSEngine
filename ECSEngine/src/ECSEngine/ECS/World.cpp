// ECS_REFLECT
#include "ecspch.h"
#include "World.h"
#include "../OS/PhysicalMemory.h"

namespace ECSEngine {

#define STOP_SIMULATION_IDENTIFIER "__StopSimulation"

	// ---------------------------------------------------------------------------------------------------------------------

	World::World() : memory(nullptr), entity_manager(nullptr), task_manager(nullptr), resource_manager(nullptr) {};

	World::World(
		GlobalMemoryManager* _memory, 
		EntityManager* _entity_manager, 
		ResourceManager* _resource_manager,
		TaskManager* _task_manager,
		TaskScheduler* _task_scheduler,
		Mouse* _mouse,
		Keyboard* _keyboard,
		DebugDrawer* _debug_drawer
	) : memory(_memory), entity_manager(_entity_manager), resource_manager(_resource_manager), task_manager(_task_manager),
		graphics(_resource_manager->m_graphics), mouse(_mouse), keyboard(_keyboard), task_scheduler(_task_scheduler), debug_drawer(_debug_drawer) {};

	// ---------------------------------------------------------------------------------------------------------------------

	World::World(const WorldDescriptor& descriptor) {
		// first the global allocator
		memory = (GlobalMemoryManager*)Malloc(sizeof(GlobalMemoryManager));
		CreateGlobalMemoryManager(memory, descriptor.global_memory_size, descriptor.global_memory_pool_count, descriptor.global_memory_new_allocation_size);
		if (descriptor.graphics_descriptor) {
			MemoryManager* graphics_allocator = (MemoryManager*)memory->Allocate(sizeof(MemoryManager) + sizeof(Graphics));
			DefaultGraphicsAllocator(graphics_allocator, memory);

			GraphicsDescriptor new_descriptor;
			memcpy(&new_descriptor, descriptor.graphics_descriptor, sizeof(new_descriptor));
			new_descriptor.allocator = graphics_allocator;

			graphics = (Graphics*)OffsetPointer(graphics_allocator, sizeof(MemoryManager));
			*graphics = Graphics(&new_descriptor);
		}
		else {
			graphics = descriptor.graphics;
		}

		task_scheduler = descriptor.task_scheduler;
		mouse = descriptor.mouse;
		keyboard = descriptor.keyboard;

		// Coallesce all the allocations for these objects such that as to not take useful slots from the global memory manager
		size_t coalesced_allocation_size =
			sizeof(MemoryManager) + // Entity manager allocator
			sizeof(EntityPool) +
			sizeof(EntityManager) +
			sizeof(SystemManager);

		if (descriptor.task_manager != nullptr) {
			task_manager = descriptor.task_manager;
		}
		else {
			coalesced_allocation_size += sizeof(TaskManager);
		}

		if (descriptor.resource_manager == nullptr) {
			coalesced_allocation_size += sizeof(MemoryManager) + sizeof(ResourceManager);
		}
		else {
			resource_manager = descriptor.resource_manager;
		}

		if (descriptor.debug_drawer == nullptr && descriptor.debug_drawer_allocator_size > 0) {
			coalesced_allocation_size += sizeof(MemoryManager) + sizeof(DebugDrawer);
		}

		void* allocation = memory->Allocate(coalesced_allocation_size);

		MemoryManager* entity_manager_memory = (MemoryManager*)allocation;
		new (entity_manager_memory) MemoryManager(
			descriptor.entity_manager_memory_size,
			descriptor.entity_manager_memory_pool_count,
			descriptor.entity_manager_memory_new_allocation_size,
			memory
		);
		allocation = OffsetPointer(allocation, sizeof(MemoryManager));
		
		if (descriptor.resource_manager == nullptr) {
			MemoryManager* resource_manager_allocator = (MemoryManager*)allocation;
			DefaultResourceManagerAllocator(resource_manager_allocator, memory);
			allocation = OffsetPointer(allocation, sizeof(MemoryManager));

			// resource manager
			resource_manager = (ResourceManager*)allocation;
			new (resource_manager) ResourceManager(resource_manager_allocator, graphics);
			allocation = OffsetPointer(allocation, sizeof(ResourceManager));
		}

		EntityPool* entity_pool = (EntityPool*)allocation;
		new (entity_pool) EntityPool(entity_manager_memory, descriptor.entity_pool_power_of_two);
		allocation = OffsetPointer(allocation, sizeof(EntityPool));

		EntityManagerDescriptor entity_descriptor;
		entity_descriptor.memory_manager = entity_manager_memory;
		entity_descriptor.entity_pool = entity_pool;
		// entity manager
		entity_manager = (EntityManager*)allocation;
		new (entity_manager) EntityManager(entity_descriptor);
		allocation = OffsetPointer(allocation, sizeof(EntityManager));

		system_manager = (SystemManager*)allocation;
		*system_manager = SystemManager(memory);
		allocation = OffsetPointer(allocation, sizeof(SystemManager));

		// task manager - if needed
		if (descriptor.task_manager == nullptr) {
			size_t thread_count = std::thread::hardware_concurrency();

			task_manager = (TaskManager*)allocation;
			new (task_manager) TaskManager(thread_count, memory, descriptor.per_thread_temporary_memory_size);
			task_manager->SetWorld(this);
			allocation = OffsetPointer(allocation, sizeof(TaskManager));
		}

		// Debug drawer - if needed
		if (descriptor.debug_drawer == nullptr && descriptor.debug_drawer_allocator_size > 0) {
			size_t thread_count = std::thread::hardware_concurrency();

			MemoryManager* debug_drawer_allocator = (MemoryManager*)allocation;
			allocation = OffsetPointer(allocation, sizeof(MemoryManager));
			DebugDrawer::DefaultAllocator(debug_drawer_allocator, memory);

			debug_drawer = (DebugDrawer*)allocation;
			allocation = OffsetPointer(allocation, sizeof(DebugDrawer));

			*debug_drawer = DebugDrawer(debug_drawer_allocator, resource_manager, thread_count);		
		}
		else {
			debug_drawer = descriptor.debug_drawer;
		}

		speed_up_factor = 1.0f;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void DestroyWorld(World* world)
	{
		// Pretty much all that there is left to do is delete the graphics resources and deallocate the global memory manager
		// Also terminate the threads if the task manager was created by it
		if (world->memory->Belongs(world->task_manager)) {
			world->task_manager->TerminateThreads(true);
		}
		world->task_manager->ClearThreadAllocators();

		if (world->memory->Belongs(world->graphics)) {
			// Destory the graphics object
			DestroyGraphics(world->graphics);
		}

		// Release the global allocator
		world->memory->Free();
	}

	// ---------------------------------------------------------------------------------------------------------------------

	WorldDescriptor GetDefaultWorldDescriptor()
	{
		WorldDescriptor world_descriptor;

		world_descriptor.graphics = nullptr;
		world_descriptor.mouse = nullptr;
		world_descriptor.keyboard = nullptr;

		world_descriptor.entity_manager_memory_new_allocation_size = ECS_GB_10 * 4;
		world_descriptor.entity_manager_memory_size = ECS_GB_10 * 4;
		world_descriptor.entity_manager_memory_pool_count = ECS_KB_10 * 4;
		// 256 * ECS_KB entities per chunk
		world_descriptor.entity_pool_power_of_two = 18;

		world_descriptor.global_memory_new_allocation_size = ECS_GB_10 * 5;
		world_descriptor.global_memory_pool_count = ECS_KB_10;
		world_descriptor.global_memory_size = ECS_GB_10 * 5;

		world_descriptor.per_thread_temporary_memory_size = ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE;
		world_descriptor.debug_drawer_allocator_size = DebugDrawer::DefaultAllocatorSize();

		return world_descriptor;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	WorldDescriptor GetWorldDescriptorMinBounds()
	{
		WorldDescriptor descriptor;

		descriptor.entity_pool_power_of_two = 5; // 64 entities per chunk
		descriptor.entity_manager_memory_size = ECS_KB * 512;
		descriptor.entity_manager_memory_pool_count = 64;
		descriptor.entity_manager_memory_new_allocation_size = ECS_KB * 256;

		descriptor.global_memory_new_allocation_size = ECS_MB_10 * 5;
		descriptor.global_memory_pool_count = 64;
		descriptor.global_memory_size = ECS_MB_10 * 10;

		descriptor.per_thread_temporary_memory_size = 0; // Use the default
		descriptor.debug_drawer_allocator_size = 0; // Disable the debug drawer
		
		return descriptor;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	WorldDescriptor GetWorldDescriptorMaxBounds()
	{
		WorldDescriptor descriptor;

		descriptor.entity_pool_power_of_two = 25; // ECS_MB * 32 entities per chunk
		descriptor.entity_manager_memory_size = ECS_GB_10 * 100;
		descriptor.entity_manager_memory_pool_count = ECS_KB_10 * 16; // After 16k the performance will start to degrade for deallocations
		descriptor.entity_manager_memory_new_allocation_size = ECS_GB_10 * 25;
		
		descriptor.global_memory_size = ECS_GB_10 * 128;
		descriptor.global_memory_pool_count = ECS_KB_10 * 16; // Same as entity manager
		descriptor.global_memory_new_allocation_size = ECS_GB_10 * 128;

		descriptor.per_thread_temporary_memory_size = ECS_GB_10; // At the moment a GB should be enough
		descriptor.debug_drawer_allocator_size = ECS_GB_10; // One GB should be more than enough

		return descriptor;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	bool ValidateWorldDescriptor(const WorldDescriptor* world_descriptor)
	{
		// At the moment, it needs to accomodate only the allocator for the resource manager + the allocator
		// for the entity manager. The graphics allocator is needed only if the graphics descriptor is specified
		size_t min_bound = DefaultResourceManagerAllocatorSize() + world_descriptor->entity_manager_memory_size + std::thread::hardware_concurrency() * 
			world_descriptor->per_thread_temporary_memory_size + ECS_MB;
		if (world_descriptor->graphics_descriptor != nullptr) {
			// Add a MB for all the intermediary allocations
			min_bound += DefaultGraphicsAllocatorSize();
		}
		
		if (world_descriptor->global_memory_size < min_bound) {
			return false;
		}

		// Also, for the backup allocation only make sure it is greater than the entity's manager
		if (world_descriptor->global_memory_new_allocation_size < world_descriptor->entity_manager_memory_new_allocation_size) {
			return false;
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void PrepareWorldConcurrency(
		World* world, 
		const TaskSchedulerSetManagerOptions* set_options,
		Stream<TaskSchedulerElement> scheduler_elements
	)
	{
		if (scheduler_elements.size > 0) {
			world->task_scheduler->Add(scheduler_elements);
		}
		world->task_manager->SetWorld(world);

		// Set the task manager tasks now
		world->task_scheduler->SetTaskManagerTasks(world->task_manager, set_options);
		world->task_scheduler->SetTaskManagerWrapper(world->task_manager);
		world->task_scheduler->InitializeSchedulerInfo(world);

		// TODO: Determine if task stealing is more overhead than letting the thread finish its work
		// Allow the threads now to steal tasks and sleep wait when finishing their dynamic tasks
		world->task_manager->SetWaitType(/*ECS_TASK_MANAGER_WAIT_STEAL |*/ ECS_TASK_MANAGER_WAIT_SLEEP);

		// Finish the static task registration
		world->task_manager->FinishStaticTasks();
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void PrepareWorldBase(World* world)
	{
		world->entity_manager->ClearCache();
		world->entity_manager->ClearFrame();
		world->SetDeltaTime(0.0f);
		world->elapsed_seconds = 0.0f;
		world->elapsed_frames = 0;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void PrepareWorld(World* world, const TaskSchedulerSetManagerOptions* options, Stream<TaskSchedulerElement> scheduler_elements)
	{
		PrepareWorldBase(world);
		TaskSchedulerSetManagerOptions set_options;
		if (options == nullptr) {
			options = &set_options;
		}
		PrepareWorldConcurrency(world, options, scheduler_elements);

		// Don't clear the debug drawer - at the moment it is a use case to fill in
		// The debug drawer before calling prepare world
		/*if (world->debug_drawer != nullptr) {
			world->debug_drawer->Clear();
		}*/
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void ClearWorld(World* world, bool release_physical_memory)
	{
		// Clear everything that can be cleared
		world->task_manager->Reset();
		// Assume the components are not maintained
		world->entity_manager->ClearAll();
		world->task_scheduler->Reset();
		world->system_manager->Clear();

		if (world->debug_drawer != nullptr) {
			world->debug_drawer->Clear();
		}
		world->timer.SetUninitialized();
		world->SetDeltaTime(0.0f);

		if (release_physical_memory) {
			// We can release the memory for the entire global memory manager
			size_t global_memory_allocator_count = world->memory->m_allocator_count;
			for (size_t index = 0; index < global_memory_allocator_count; index++) {
				void* global_allocation = world->memory->GetAllocatorBasePointer(index);
				size_t global_allocation_size = world->memory->GetAllocatorBaseAllocationSize(index);
				// Here we can omit this check, but let's assert just for good measure
				ECS_ASSERT(OS::EmptyPhysicalMemory(global_allocation, global_allocation_size), "Failed to release physical pages for World");
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void DoFrame(World* world, bool wait_frame) {
		// Clear any temporary resources here
		world->task_manager->ClearFrame();
		world->task_scheduler->ClearFrame();
		world->entity_manager->ClearFrame();
		world->system_manager->ClearFrame();

		world->task_manager->DoFrame(wait_frame);
		// Update the elapsed seconds and frame count
		world->elapsed_seconds += world->delta_time;
		world->elapsed_frames++;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void PauseWorld(World* world) {
		world->task_manager->SleepThreads(true);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void CopyWorld(World* destination_world, const World* source_world) {
		// We don't want to clear the physical pages, they might be used again in this call
		ClearWorld(destination_world, false);

		// The structure that need to be copied are the entity manager, the system manager, the task scheduler,
		// The task manager (static tasks) and the resource manager, if the user specified so, if the pointers are different. 
		// If it is the same resource manager instance, then the resources don't need to be copied.

		destination_world->entity_manager->CopyOther(source_world->entity_manager);
		destination_world->system_manager->CopyOther(source_world->system_manager);
		destination_world->task_scheduler->CopyOther(source_world->task_scheduler);
		destination_world->task_manager->AddTasksFromOther(source_world->task_manager);
		if (destination_world->resource_manager != source_world->resource_manager) {
			destination_world->resource_manager->InheritResources(source_world->resource_manager);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void StopSimulation(SystemManager* system_manager) {
		bool* status = (bool*)system_manager->GetData(STOP_SIMULATION_IDENTIFIER);
		*status = true;
	}

	void StopSimulation(World* world) {
		StopSimulation(world->system_manager);
	}

	bool GetStopSimulationStatus(const SystemManager* system_manager) {
		return *(bool*)system_manager->GetData(STOP_SIMULATION_IDENTIFIER);
	}

	void SetStopSimulationStatus(SystemManager* system_manager, bool status) {
		system_manager->BindData(STOP_SIMULATION_IDENTIFIER, &status, sizeof(status));
	}

	// ---------------------------------------------------------------------------------------------------------------------

}