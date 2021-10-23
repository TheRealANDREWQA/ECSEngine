#pragma once
#include "../../Core.h"
#include "../../Internal/Multithreading/TaskDependencyGraph.h"
#include "../../Allocators/AllocatorTypes.h"

#define ECS_MODULE_FUNCTION_NAME "ModuleFunction"

#define ECS_MODULE_GRAPHICS_DRAW_FUNCTION_NAME "ModuleGraphicsDraw"
#define ECS_MODULE_GRAPHICS_INITIALIZE_FUNCTION_NAME "ModuleGraphicsInitialize"
#define ECS_MODULE_GRAPHICS_RELEASE_FUNCTION_NAME "ModuleGraphicsRelease"

#define ECS_MODULE_EXTENSION L".dll"


namespace ECSEngine {

	struct World;

	using ModuleFunction = void (*)(World* world, containers::Stream<TaskGraphElement>& module_stream);
	using ModuleGraphicsDraw = void (*)(World* world, void* data);
	using ModuleGraphicsInitialize = void* (*)(World* world, void* data_reflection);
	using ModuleGraphicsRelease = void (*)(World* world, void* data_reflection);

	// Module function missing is returned for either graphics function missing
	enum GetModuleFunctionReturnCode : unsigned char {
		ECS_GET_MODULE_OK,
		ECS_GET_MODULE_FAULTY_PATH,
		ECS_GET_MODULE_FUNCTION_MISSING
	};

	struct GetModuleFunctionData {
		GetModuleFunctionReturnCode code;
		ModuleFunction function;
		HMODULE os_module_handle;
	};

	struct GetModuleGraphicsFunctionData {
		GetModuleFunctionReturnCode code;
		ModuleGraphicsDraw draw;
		ModuleGraphicsInitialize initialize;
		ModuleGraphicsRelease release;
		void* initialized_data;
		HMODULE os_module_handle;
	};

	// Frees the memory allocated during GetModuleTasks
	ECSENGINE_API void DeallocateModuleTasks(containers::Stream<TaskGraphElement> tasks, AllocatorPolymorphic allocator);

	// Frees the OS handle to the valid module function
	ECSENGINE_API void FreeModuleFunction(GetModuleFunctionData data);

	// Frees the OS handle to the valid module graphics function
	ECSENGINE_API void FreeModuleGraphicsFunction(const GetModuleGraphicsFunctionData& data);

	ECSENGINE_API GetModuleFunctionData GetModuleFunction(containers::Stream<wchar_t> path);

	// The returned OS module handle should stay alive while the functions are being used
	ECSENGINE_API GetModuleGraphicsFunctionData GetModuleGraphicsFunction(containers::Stream<wchar_t> path);

	// It will release the OS Handler; It does a single coallesced allocation
	ECSENGINE_API containers::Stream<TaskGraphElement> GetModuleTasks(
		World* world,
		GetModuleFunctionData function_data,
		AllocatorPolymorphic allocator
	);

	// Returns nullptr and 0 if the module function cannot be loaded
	// It will release the OS handler if it succeeds
	ECSENGINE_API containers::Stream<TaskGraphElement> GetModuleTasks(
		World* world,
		containers::Stream<wchar_t> module_path,
		AllocatorPolymorphic allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API bool HasModuleFunction(containers::Stream<wchar_t> path);

	ECSENGINE_API bool HasModuleGraphicsFunction(containers::Stream<wchar_t> path);

	ECSENGINE_API void InitializeGraphicsModule(GetModuleGraphicsFunctionData& data, World* world, void* data_reflection);

	// It will release the OS handle
	ECSENGINE_API void ReleaseGraphicsModule(GetModuleGraphicsFunctionData& data, World* world);

}