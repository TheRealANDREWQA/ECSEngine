//ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "EntityManager.h"
#include "Multithreading/TaskManager.h"
#include "../Rendering/Graphics.h"
#include "../Internal/EntityManager.h"
#include "../Internal/InternalStructures.h"
#include "../Internal/Resources/ResourceManager.h"
#include "../Allocators/MemoryManager.h"
#include "System/SystemManager.h"

namespace ECSEngine {

	struct ECS_REFLECT WorldDescriptor {
		size_t global_memory_size; 
		size_t global_memory_pool_count;
		size_t global_memory_new_allocation_size;
		Graphics* ECS_OMIT_FIELD_REFLECT(8) resource_manager_graphics = nullptr;
		HWND ECS_OMIT_FIELD_REFLECT(8) graphics_hWnd;
		unsigned int graphics_window_size_x;
		unsigned int graphics_window_size_y;
		unsigned int thread_count;
		unsigned int task_manager_max_dynamic_tasks = ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD;
		MemoryManager* ECS_OMIT_FIELD_REFLECT(8) internal_allocator = nullptr;
		EntityPool* ECS_OMIT_FIELD_REFLECT(8) entity_pool = nullptr;
		unsigned int entity_manager_max_dynamic_archetype_count = ECS_ENTITY_MANAGER_MAX_DYNAMIC_ARCHETYPES;
		unsigned int entity_manager_max_static_archetype_count = ECS_ENTITY_MANAGER_MAX_STATIC_ARCHETYPES;
		unsigned int entity_pool_power_of_two;
		unsigned int entity_pool_arena_count;
		unsigned int entity_pool_block_count;
		unsigned int system_manager_max_systems;
		size_t memory_manager_size; 
		size_t memory_manager_maximum_pool_count;
		size_t memory_manager_new_allocation_size;
	};

	struct ECSENGINE_API World
	{
		World();
		World(GlobalMemoryManager* _memory, EntityManager* _entity_manager, SystemManager* _system_manager, ResourceManager* _resource_manager);
		World(const WorldDescriptor& descriptor);

		World(const World& other) = default;
		World& operator = (const World& other) = default;

		// should be called only if the world was created using the descriptor
		void ReleaseResources();

		GlobalMemoryManager* memory;
		ResourceManager* resource_manager;
		TaskManager* task_manager;
		EntityManager* entity_manager;
		SystemManager* system_manager;

	private:
		// these are held only to be realesed at the end; they do not contain valid pointers if they were not created using 
		// operator new
		Graphics* graphics;
		EntityPool* entity_pool;
		MemoryManager* internal_memory;
	};

}

