#pragma once
#include "../../Core.h"
#include "../../Internal/Multithreading/TaskDependencyGraph.h"
#include "../../Allocators/AllocatorTypes.h"

#define ECS_MODULE_FUNCTION_NAME "ModuleFunction"
#define ECS_MODULE_EXTENSION L".dll"


namespace ECSEngine {

	struct World;

	using ModuleFunction = void (*)(World* world, containers::Stream<TaskGraphElement>& module_stream);

	// Module function missing is returned for either graphics function missing
	enum ModuleStatus : unsigned char {
		ECS_GET_MODULE_OK,
		ECS_GET_MODULE_FAULTY_PATH,
		ECS_GET_MODULE_FUNCTION_MISSING
	};

	struct Module {
		ModuleStatus code;
		ModuleFunction function;
		containers::Stream<TaskGraphElement> tasks;
		HMODULE os_module_handle;
	};

	// Returns true if it has the associated DLL function
	ECSENGINE_API bool IsModule(containers::Stream<wchar_t> path);

	// It does not load the tasks because it misses the allocator for string allocation
	// The function LoadModuleTasks can be used to load them at a later time
	ECSENGINE_API Module LoadModule(containers::Stream<wchar_t> path);

	// It loads the thread tasks
	ECSENGINE_API Module LoadModule(containers::Stream<wchar_t> path, World* world, AllocatorPolymorphic allocator);

	// It will not release the OS Handler - it must be kept around as long as the tasks are loaded;
	// It does a single coallesced allocation
	ECSENGINE_API void LoadModuleTasks(
		World* world,
		Module* module,
		AllocatorPolymorphic allocator
	);

	// It will not release the OS Handler - it must be kept around as long as the tasks are loaded;
	// It does a single coallesced allocation
	ECSENGINE_API containers::Stream<TaskGraphElement> LoadModuleTasks(
		World* world,
		Module module,
		AllocatorPolymorphic allocator
	);

	// Frees the OS handle to the valid module function but it does not deallocate the tasks
	// if they were loaded before because it lacks the allocator
	ECSENGINE_API void ReleaseModule(Module module);

	// Frees the OS handle to the valid module function and deallocates the tasks
	ECSENGINE_API void ReleaseModule(Module module, AllocatorPolymorphic allocator);

}