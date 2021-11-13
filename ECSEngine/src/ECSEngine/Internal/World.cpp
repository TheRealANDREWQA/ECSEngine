// ECS_REFLECT
#include "ecspch.h"
#include "World.h"

namespace ECSEngine {

	struct ECS_REFLECT TO_BE_REFLECT {
		CapacityStream<float2> type;
		float3* ptr;
		float* other_ptr;
		CapacityStream<double> dob;
	};

	World::World() : memory(nullptr), entity_manager(nullptr), system_manager(nullptr), task_manager(nullptr),
		resource_manager(nullptr) {};

	World::World(GlobalMemoryManager* _memory, EntityManager* _entity_manager, SystemManager* _system_manager,
		ResourceManager* _resource_manager) : memory(_memory), entity_manager(_entity_manager), system_manager(_system_manager),
		resource_manager(_resource_manager) {
		task_manager = system_manager->m_task_manager;
	};

	World::World(const WorldDescriptor& descriptor) {
		// first the global allocator
		memory = new GlobalMemoryManager(descriptor.global_memory_size, descriptor.global_memory_pool_count, descriptor.global_memory_new_allocation_size);

		MemoryManager* temp_internal_memory;
		// the internal memory manager
		if (descriptor.internal_allocator == nullptr) {
			temp_internal_memory = new MemoryManager(descriptor.memory_manager_size, descriptor.memory_manager_maximum_pool_count, descriptor.memory_manager_new_allocation_size, memory);
			internal_memory = temp_internal_memory;
		}
		else {
			temp_internal_memory = descriptor.internal_allocator;
			internal_memory = nullptr;
		}

		// the graphics object
		Graphics* temp_graphics;
		if (descriptor.resource_manager_graphics == nullptr) {
			GraphicsDescriptor graphics_descriptor;
			graphics_descriptor.window_size = { descriptor.graphics_window_size_x, descriptor.graphics_window_size_y };
			temp_graphics = new Graphics(descriptor.graphics_hWnd, &graphics_descriptor);
			graphics = temp_graphics;
		}
		else {
			temp_graphics = descriptor.resource_manager_graphics;
			graphics = nullptr;
		}

		// resource manager
		resource_manager = new ResourceManager(temp_internal_memory, temp_graphics, descriptor.thread_count, "Resources\\");

		// task manager
		task_manager = new TaskManager(descriptor.thread_count, internal_memory, descriptor.task_manager_max_dynamic_tasks);
		task_manager->SetWorld(this);

		// entity pool
		EntityPool* entity_pool_temp;
		if (descriptor.entity_pool == nullptr) {
			entity_pool_temp = new EntityPool(descriptor.entity_pool_power_of_two, descriptor.entity_pool_arena_count,
				descriptor.entity_pool_block_count, temp_internal_memory);
			entity_pool = entity_pool_temp;
		}
		else {
			entity_pool_temp = descriptor.entity_pool;
			entity_pool = nullptr;
		}

		// entity manager
		entity_manager = new EntityManager(temp_internal_memory, entity_pool_temp, descriptor.entity_manager_max_dynamic_archetype_count, descriptor.entity_manager_max_static_archetype_count);

		// system manager
		system_manager = new SystemManager(task_manager, temp_internal_memory, descriptor.system_manager_max_systems);
	}

	void World::ReleaseResources() {
		delete memory;
		delete resource_manager;
		delete task_manager;
		delete entity_manager;
		delete system_manager;

		if (internal_memory != nullptr) {
			delete internal_memory;
		}
		if (entity_pool != nullptr) {
			delete entity_pool;
		}
	}

}