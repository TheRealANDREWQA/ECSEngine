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

// This is a value in seconds describing the case when the simulation is stopped
// And then restarted some time later, which results in a very large value (like when a 
// debug breakpoint was met or when the world is stopped and restarted later)
// which breaks the simulation. In order to avoid that, use a threshold value above which
// the delta time will not be changed if the value crosses it
#define ECS_WORLD_DELTA_TIME_REUSE_THRESHOLD 1.0f

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
		// The resource manager is optional. If you want to give one from the outside, there is the option.
		// If it is nullptr then it will create an internal one
		ResourceManager* resource_manager = nullptr; ECS_SKIP_REFLECTION()
		// This is optional
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

		ECS_INLINE float RealDeltaTime() const {
			return delta_time / speed_up_factor;
		}

		ECS_INLINE void SetDeltaTime(float seconds) {
			delta_time = seconds;
			inverse_delta_time = 1.0f / delta_time;
		}

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

		// The timer is used to keep track of frame duration
		// It can be queried to find out the frame duration at that point
		Timer timer;
		// The delta time is expressed in seconds
		float delta_time;
		// The inverse of delta time
		float inverse_delta_time;
		// This factor is used to increase/decrease the simulation speed
		// If the simulation speed is too high, things might start to brake
		float speed_up_factor;
		// Measures the number of seconds that have elapsed since the simulation start
		float elapsed_seconds;
		// Records the number of frames that have been executed since the beginning of the simulation
		size_t elapsed_frames;
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
	// before calling PrepareWorld. You can optionally pass some transfer data if there is such data
	ECSENGINE_API void PrepareWorldConcurrency(
		World* world, 
		const TaskSchedulerSetManagerOptions* options,
		Stream<TaskSchedulerElement> scheduler_elements = { nullptr, 0 }	
	);

	// This performs all the necessary steps that are not related to the concurrency structures
	ECSENGINE_API void PrepareWorldBase(World* world);

	// Initializes all the necessary one time actions needed before the actual run of the world
	// If the scheduler_elements are not specified, they need to be added to the world's task scheduler
	// before calling PrepareWorld. This is a combination of PrepareWorldBase and PrepareWorldConcurrency
	ECSENGINE_API void PrepareWorld(World* world, const TaskSchedulerSetManagerOptions* options = nullptr, Stream<TaskSchedulerElement> scheduler_elements = { nullptr, 0 });

	// This will clear the system manager out of everything and the entity manager will also lose everything
	// The debug drawer will also be cleared, if one is specified. If the boolean is set to true,
	// Then it will release the physical pages used to reduce the working set
	ECSENGINE_API void ClearWorld(World* world, bool release_physical_memory = true);

	// Can choose whether or not to wait for the frame to finish
	ECSENGINE_API void DoFrame(World* world, bool wait_for_frame = true);

	// It will wait to finish all the current dynamic tasks in flight
	ECSENGINE_API void PauseWorld(World* world);

	// These are functions that can be used from C++ to tell the editor to stop
	// The simulation
	
	ECSENGINE_API void StopSimulation(SystemManager* system_manager);

	ECSENGINE_API void StopSimulation(World* world);

	ECSENGINE_API bool GetStopSimulationStatus(const SystemManager* system_manager);

	ECSENGINE_API void SetStopSimulationStatus(SystemManager* system_manager, bool status);

}

