//ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "EntityManager.h"
#include "../Multithreading/TaskManager.h"
#include "../Rendering/Graphics.h"
#include "InternalStructures.h"
#include "../Resources/ResourceManager.h"
#include "../Input/Mouse.h"
#include "../Input/Keyboard.h"
#include "SystemManager.h"
#include "../Multithreading/TaskScheduler.h"
#include "../Tools/Debug Draw/DebugDraw.h"

namespace ECSEngine {

	struct DebugDrawer;

	struct ECS_REFLECT WorldDescriptor {
		size_t global_memory_size; 
		size_t global_memory_pool_count;
		size_t global_memory_new_allocation_size;
		size_t entity_manager_memory_size;
		size_t entity_manager_memory_pool_count;
		size_t entity_manager_memory_new_allocation_size;
		unsigned int entity_pool_power_of_two;
		// If this is specified and the debug drawer is not given, then it will assume
		// that you want to create a debug drawer with the given allocation_size
		unsigned int debug_drawer_allocator_size = 0;

		// If these values are 0, then the default from the task manager will be used
		size_t per_thread_temporary_memory_size = 0;

		// If the graphics descriptor is specified, then it will construct a Graphics object
		// from the given descriptor
		Graphics* graphics; ECS_SKIP_REFLECTION()
		Mouse* mouse; ECS_SKIP_REFLECTION()
		Keyboard* keyboard; ECS_SKIP_REFLECTION()
		TaskScheduler* task_scheduler; ECS_SKIP_REFLECTION()
		// This is optional. If you want to reuse the task manager among different worlds
		// without having to reinitialize it, this is the way to do it
		TaskManager* task_manager = nullptr; ECS_SKIP_REFLECTION()
		GraphicsDescriptor* graphics_descriptor = nullptr; ECS_SKIP_REFLECTION()
		// The resource manager is optional. If you want to give one from the outside can do.
		// If it is nullptr then it will create an internal one
		ResourceManager* resource_manager = nullptr; ECS_SKIP_REFLECTION()
		// This is optional. If you want 
		DebugDrawer* debug_drawer = nullptr; ECS_SKIP_REFLECTION()
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
			Mouse* mouse,
			Keyboard* keyboard,
			DebugDrawer* debug_drawer
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
		Mouse* mouse;
		Keyboard* keyboard;
		DebugDrawer* debug_drawer;
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

	// This function sets up the necessary task_scheduler and task manager functions
	// If the scheduler_elements are not specified, they need to be added to the world's task scheduler
	// before calling PrepareWorld
	ECSENGINE_API void PrepareWorldConcurrency(World* world, Stream<TaskSchedulerElement> scheduler_elements = { nullptr, 0 });

	// Initializes all the necessary one time actions needed before the actual run of the world
	// If the scheduler_elements are not specified, they need to be added to the world's task scheduler
	// before calling PrepareWorld
	ECSENGINE_API void PrepareWorld(World* world, Stream<TaskSchedulerElement> scheduler_elements = { nullptr, 0 });

	ECSENGINE_API void ClearWorld(World* world);

	// Can choose whether or not to wait for the frame to finish
	ECSENGINE_API void DoFrame(World* world, bool wait_for_frame = true);

	// It will wait to finish all the current dynamic tasks in flight
	ECSENGINE_API void PauseWorld(World* world);

}

