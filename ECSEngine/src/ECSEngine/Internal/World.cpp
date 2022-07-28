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
		HID::Mouse* _mouse,
		HID::Keyboard* _keyboard
	) : memory(_memory), entity_manager(_entity_manager), resource_manager(_resource_manager), task_manager(_task_manager),
		graphics(_resource_manager->m_graphics), mouse(_mouse), keyboard(_keyboard), task_scheduler(_task_scheduler) {};

	// ---------------------------------------------------------------------------------------------------------------------

	World::World(const WorldDescriptor& descriptor) {
		// first the global allocator
		memory = new GlobalMemoryManager(descriptor.global_memory_size, descriptor.global_memory_pool_count, descriptor.global_memory_new_allocation_size);
		graphics = descriptor.graphics;
		task_scheduler = descriptor.task_scheduler;
		mouse = descriptor.mouse;
		keyboard = descriptor.keyboard;

		// Coallesce all the allocations for these objects such that as to not take useful slots from the global memory manager
		size_t coallesced_allocation_size =
			sizeof(MemoryManager) + // Entity manager allocator
			sizeof(MemoryManager) +  // Resource manager allocator
			sizeof(ResourceManager) +
			sizeof(TaskManager) +
			sizeof(EntityManager) +
			sizeof(SystemManager); 

		void* allocation = memory->Allocate(coallesced_allocation_size);

		MemoryManager* entity_manager_memory = (MemoryManager*)allocation;
		new (entity_manager_memory) MemoryManager(
			descriptor.entity_manager_memory_size,
			descriptor.entity_manager_memory_pool_count,
			descriptor.entity_manager_memory_new_allocation_size,
			memory
		);
		allocation = function::OffsetPointer(allocation, sizeof(MemoryManager));

		MemoryManager* resource_manager_allocator = (MemoryManager*)allocation;
		*resource_manager_allocator = DefaultResourceManagerAllocator(memory);
		allocation = function::OffsetPointer(allocation, sizeof(MemoryManager));

		// resource manager
		resource_manager = (ResourceManager*)allocation;
		new (resource_manager) ResourceManager(resource_manager_allocator, graphics, descriptor.thread_count);
		allocation = function::OffsetPointer(allocation, sizeof(ResourceManager));

		// task manager
		task_manager = (TaskManager*)allocation;
		new (task_manager) TaskManager(descriptor.thread_count, memory);
		task_manager->SetWorld(this);
		allocation = function::OffsetPointer(allocation, sizeof(TaskManager));

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
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void DestroyWorld(World* world)
	{
		// Pretty much all that there is left to do is delete the graphics resources and deallocate the global memory manager

		// Destory the graphics object
		DestroyGraphics(world->graphics);

		// Release the global allocator
		world->memory->ReleaseResources();
	}

	// ---------------------------------------------------------------------------------------------------------------------

	WorldDescriptor GetDefaultWorldDescriptor()
	{
		WorldDescriptor world_descriptor;

		world_descriptor.graphics = nullptr;
		world_descriptor.mouse = nullptr;
		world_descriptor.keyboard = nullptr;

		world_descriptor.thread_count = std::thread::hardware_concurrency();

		world_descriptor.entity_manager_memory_new_allocation_size = ECS_MB * 20;
		world_descriptor.entity_manager_memory_size = ECS_MB * 50;
		world_descriptor.entity_manager_memory_pool_count = 1024;

		// 256 * ECS_KB entities per chunk
		world_descriptor.entity_pool_power_of_two = 18;

		world_descriptor.global_memory_new_allocation_size = ECS_MB * 25;
		world_descriptor.global_memory_pool_count = 1024;
		world_descriptor.global_memory_size = ECS_MB * 60;

		return world_descriptor;
	}

	// ---------------------------------------------------------------------------------------------------------------------

}