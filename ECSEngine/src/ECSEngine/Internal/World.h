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
#include "../Utilities/Mouse.h"
#include "../Utilities/Keyboard.h"
#include "SystemManager.h"
#include "Multithreading/TaskScheduler.h"

namespace ECSEngine {

	struct ECS_REFLECT WorldDescriptor {
		size_t global_memory_size; 
		size_t global_memory_pool_count;
		size_t global_memory_new_allocation_size;
		size_t entity_manager_memory_size;
		size_t entity_manager_memory_pool_count;
		size_t entity_manager_memory_new_allocation_size;
		unsigned int entity_pool_power_of_two;

		// If these values are 0, then the default from the task manager will be used
		size_t per_thread_temporary_memory_size = 0;

		// If the graphics descriptor is specified, then it will construct a Graphics object
		// from the given descriptor
		Graphics* graphics; ECS_SKIP_REFLECTION()
		HID::Mouse* mouse; ECS_SKIP_REFLECTION()
		HID::Keyboard* keyboard; ECS_SKIP_REFLECTION()
		TaskScheduler* task_scheduler; ECS_SKIP_REFLECTION()
		// This is optional. If you want to reuse the task manager among different worlds
		// without having to reinitialize it, this is the way to do it
		TaskManager* task_manager = nullptr; ECS_SKIP_REFLECTION()
		GraphicsDescriptor* graphics_descriptor = nullptr; ECS_SKIP_REFLECTION()
		// The resource manager is optional. If you want to give one from the outside can do.
		// If it is nullptr then it will create an internal one
		ResourceManager* resource_manager = nullptr; ECS_SKIP_REFLECTION()
	};

	struct ECSENGINE_API World
	{
		World();
		World(
			GlobalMemoryManager* _memory, 
			EntityManager* _entity_manager, 
			ResourceManager* _resource_manager, 
			TaskManager* _task_manager,
			TaskScheduler* _task_scheduler,
			HID::Mouse* mouse,
			HID::Keyboard* keyboard
		);
		World(const WorldDescriptor& descriptor);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(World);

		GlobalMemoryManager* memory;
		ResourceManager* resource_manager;
		TaskManager* task_manager;
		TaskScheduler* task_scheduler;
		EntityManager* entity_manager;
		Graphics* graphics;
		SystemManager* system_manager;
		HID::Mouse* mouse;
		HID::Keyboard* keyboard;
	};

	// Destroys the graphics object if it was created internally, deallocates the global memory manager
	// And terminates the threads if the task manager was created internally
	ECSENGINE_API void DestroyWorld(World* world);

	// It does not set the graphics, mouse, keyboard, task manager or task scheduler
	ECSENGINE_API WorldDescriptor GetDefaultWorldDescriptor();

	// Min bounds for fillable fields
	ECSENGINE_API WorldDescriptor GetWorldDescriptorMinBounds();

	// Max bounds for fillable fields
	ECSENGINE_API WorldDescriptor GetWorldDescriptorMaxBounds();

	// Makes sure that the global allocator can fit the memory manager for the entities
	// and the other systems
	ECSENGINE_API bool ValidateWorldDescriptor(const WorldDescriptor* world_descriptor);

	// Starts the simulation of this world
	ECSENGINE_API void PrepareWorld(World* world);

	ECSENGINE_API void DoFrame(World* world);

	// It will wait to finish all the current dynamic tasks in flight
	ECSENGINE_API void PauseWorld(World* world);

}

