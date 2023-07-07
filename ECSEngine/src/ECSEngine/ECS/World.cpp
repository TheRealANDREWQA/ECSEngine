// ECS_REFLECT
#include "ecspch.h"
#include "World.h"

namespace ECSEngine {

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
		memory = new GlobalMemoryManager(descriptor.global_memory_size, descriptor.global_memory_pool_count, descriptor.global_memory_new_allocation_size);
		if (descriptor.graphics_descriptor) {
			MemoryManager* graphics_allocator = (MemoryManager*)memory->Allocate(sizeof(MemoryManager) + sizeof(Graphics));
			*graphics_allocator = DefaultGraphicsAllocator(memory);

			GraphicsDescriptor new_descriptor;
			memcpy(&new_descriptor, descriptor.graphics_descriptor, sizeof(new_descriptor));
			new_descriptor.allocator = graphics_allocator;

			graphics = (Graphics*)function::OffsetPointer(graphics_allocator, sizeof(MemoryManager));
			*graphics = Graphics(&new_descriptor);
		}
		else {
			graphics = descriptor.graphics;
		}

		task_scheduler = descriptor.task_scheduler;
		mouse = descriptor.mouse;
		keyboard = descriptor.keyboard;

		// Coallesce all the allocations for these objects such that as to not take useful slots from the global memory manager
		size_t coallesced_allocation_size =
			sizeof(MemoryManager) + // Entity manager allocator
			sizeof(EntityPool) +
			sizeof(EntityManager) +
			sizeof(SystemManager);

		if (descriptor.task_manager != nullptr) {
			task_manager = descriptor.task_manager;
		}
		else {
			coallesced_allocation_size += sizeof(TaskManager);
		}

		if (descriptor.resource_manager == nullptr) {
			coallesced_allocation_size += sizeof(MemoryManager) + sizeof(ResourceManager);
		}
		else {
			resource_manager = descriptor.resource_manager;
		}

		if (descriptor.debug_drawer == nullptr && descriptor.debug_drawer_allocator_size > 0) {
			coallesced_allocation_size += sizeof(MemoryManager) + sizeof(DebugDrawer);
		}

		void* allocation = memory->Allocate(coallesced_allocation_size);

		MemoryManager* entity_manager_memory = (MemoryManager*)allocation;
		new (entity_manager_memory) MemoryManager(
			descriptor.entity_manager_memory_size,
			descriptor.entity_manager_memory_pool_count,
			descriptor.entity_manager_memory_new_allocation_size,
			memory
		);
		allocation = function::OffsetPointer(allocation, sizeof(MemoryManager));
		
		if (descriptor.resource_manager == nullptr) {
			MemoryManager* resource_manager_allocator = (MemoryManager*)allocation;
			*resource_manager_allocator = DefaultResourceManagerAllocator(memory);
			allocation = function::OffsetPointer(allocation, sizeof(MemoryManager));

			// resource manager
			resource_manager = (ResourceManager*)allocation;
			new (resource_manager) ResourceManager(resource_manager_allocator, graphics);
			allocation = function::OffsetPointer(allocation, sizeof(ResourceManager));
		}

		EntityPool* entity_pool = (EntityPool*)allocation;
		new (entity_pool) EntityPool(entity_manager_memory, descriptor.entity_pool_power_of_two);
		allocation = function::OffsetPointer(allocation, sizeof(EntityPool));

		EntityManagerDescriptor entity_descriptor;
		entity_descriptor.memory_manager = entity_manager_memory;
		entity_descriptor.entity_pool = entity_pool;
		// entity manager
		entity_manager = (EntityManager*)allocation;
		new (entity_manager) EntityManager(entity_descriptor);
		allocation = function::OffsetPointer(allocation, sizeof(EntityManager));

		system_manager = (SystemManager*)allocation;
		*system_manager = SystemManager(memory);
		allocation = function::OffsetPointer(allocation, sizeof(SystemManager));

		// task manager - if needed
		if (descriptor.task_manager == nullptr) {
			size_t thread_count = std::thread::hardware_concurrency();

			task_manager = (TaskManager*)allocation;
			new (task_manager) TaskManager(thread_count, memory, descriptor.per_thread_temporary_memory_size);
			task_manager->SetWorld(this);
			allocation = function::OffsetPointer(allocation, sizeof(TaskManager));
		}

		// Debug drawer - if needed
		if (descriptor.debug_drawer == nullptr && descriptor.debug_drawer_allocator_size > 0) {
			size_t thread_count = std::thread::hardware_concurrency();

			MemoryManager* debug_drawer_allocator = (MemoryManager*)allocation;
			allocation = function::OffsetPointer(allocation, sizeof(MemoryManager));
			*debug_drawer_allocator = DebugDrawer::DefaultAllocator(memory);

			debug_drawer = (DebugDrawer*)allocation;
			allocation = function::OffsetPointer(allocation, sizeof(DebugDrawer));

			*debug_drawer = DebugDrawer(debug_drawer_allocator, resource_manager, thread_count);		
		}
		else {
			debug_drawer = descriptor.debug_drawer;
		}
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

		world_descriptor.entity_manager_memory_new_allocation_size = ECS_MB_10 * 250;
		world_descriptor.entity_manager_memory_size = ECS_MB_10 * 600;
		world_descriptor.entity_manager_memory_pool_count = ECS_KB_10 * 2;

		// 256 * ECS_KB entities per chunk
		world_descriptor.entity_pool_power_of_two = 18;

		world_descriptor.global_memory_new_allocation_size = ECS_GB_10;
		world_descriptor.global_memory_pool_count = ECS_KB_10;
		world_descriptor.global_memory_size = ECS_GB_10;

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
		descriptor.entity_manager_memory_size = ECS_GB * 4; // At the moment limit to 4GB. The block range uses unsigned ints
															// and it is limited to 4GB

		descriptor.entity_manager_memory_pool_count = ECS_KB * 16; // After 16k the performance will start to degrade for deallocations
		descriptor.entity_manager_memory_new_allocation_size = ECS_GB * 4; // At the moment limit new allocations to 4GB. Block range limit
		
		descriptor.global_memory_size = ECS_GB * 4; // Block range limit
		descriptor.global_memory_pool_count = ECS_KB * 16; // Same as entity manager
		descriptor.global_memory_new_allocation_size = ECS_GB * 4; // Block range limit

		descriptor.per_thread_temporary_memory_size = ECS_GB; // At the moment a GB should be enough
		descriptor.debug_drawer_allocator_size = ECS_GB; // One GB should be more than enough

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

	void PrepareWorld(World* world, Stream<TaskSchedulerElement> scheduler_elements)
	{
		if (scheduler_elements.size > 0) {
			world->task_scheduler->Add(scheduler_elements);
		}

		// Set the task manager tasks now
		world->task_scheduler->SetTaskManagerTasks(world->task_manager);
		world->task_scheduler->SetTaskManagerWrapper(world->task_manager);
		world->task_scheduler->InitializeSchedulerInfo(world);

		world->task_manager->SetWorld(world);

		// TODO: Determine if task stealing is more overhead than letting the thread finish its work
		// Allow the threads now to steal tasks and sleep wait when finishing their dynamic tasks
		world->task_manager->SetWaitType(/*ECS_TASK_MANAGER_WAIT_STEAL |*/ ECS_TASK_MANAGER_WAIT_SLEEP);

		// Finish the static task registration
		world->task_manager->FinishStaticTasks();

		if (world->debug_drawer != nullptr) {
			world->debug_drawer->Clear();
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
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void PauseWorld(World* world) {
		world->task_manager->SleepThreads(true);
	}

	// ---------------------------------------------------------------------------------------------------------------------

}