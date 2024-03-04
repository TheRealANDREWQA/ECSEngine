#pragma once
#include "../../Core.h"
#include "ModuleDefinition.h"

#define ECS_MODULE_EXTENSION L".dll"
#define ECS_MODULE_EXTENSION_ASCII ".dll"

namespace ECSEngine {

	struct TaskScheduler;
	struct TaskManager;

	// Returns true if it has the associated DLL task function
	// The other are optional and do not need to be present in order for the module to be valid
	ECSENGINE_API bool FindModule(Stream<wchar_t> path);

	// Determines all the dependencies (imports) for the given dll
	// For deallocation only the .buffer of the stream of strings is needed
	// Can optionally tell the function to omit known system .dlls
	ECSENGINE_API Stream<Stream<char>> GetModuleDLLDependencies(Stream<wchar_t> path, AllocatorPolymorphic allocator, bool omit_system_dlls = false);

	// Retrieves the DLL dependencies from the .vcxproj file
	// Can optionally tell the function to omit known system .dlls
	ECSENGINE_API Stream<Stream<char>> GetModuleDLLDependenciesFromVCX(Stream<wchar_t> path, AllocatorPolymorphic allocator, bool omit_system_dlls = false);

	// Retrieves the DLL dependencies from the source files #include directives
	// An array of existing module names needs to be provided, otherwise the module
	// Names cannot be identified
	ECSENGINE_API Stream<Stream<char>> GetModuleDLLDependenciesFromSourceIncludes(
		Stream<wchar_t> src_folder, 
		Stream<Stream<char>> existing_module_names, 
		AllocatorPolymorphic allocator
	);

	// It does not load any stream from the functions. Use the corresponding load functions for that
	// The function LoadModuleTasks can be used to load them at a later time. If the boolean pointer
	// is specified, it will attempt to load the debugging symbols for this module and it will set the
	// boolean value with the success of that load.
	ECSENGINE_API Module LoadModule(Stream<wchar_t> path, bool* load_debug_symbols = nullptr);

	// Loads the streams from the given module
	ECSENGINE_API void LoadAppliedModule(AppliedModule* module, AllocatorPolymorphic allocator, Stream<Stream<char>> component_names = {}, CapacityStream<char>*error_message = nullptr);

	// It will not release the OS Handle - it must be kept around as long as the tasks are loaded;
	// It does a single coalesced allocation. Deallocate the buffer to free the memory
	ECSENGINE_API Stream<TaskSchedulerElement> LoadModuleTasks(
		const Module* module,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	// It will not release the OS Handle - it must be kept around as long as the descriptors are loaded;
	// It does a single coalesced allocation. Deallocate the buffer to free the memory
	// Returns { nullptr, 0 } if the function does not exist
	ECSENGINE_API Stream<Tools::UIWindowDescriptor> LoadModuleUIDescriptors(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// It will not release the OS Handle - it must be kept around as long as the build asset types are loaded;
	// It does a single coalesced allocation. Deallocate the buffer to free the memory
	// Returns { nullptr, 0 } if the function does not exist
	ECSENGINE_API Stream<ModuleBuildAssetType> LoadModuleBuildAssetTypes(
		const Module* module,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	// It will not release the OS Handle - it must be kept around as long as the functors are loaded;
	// It does a single coalesced allocation with the allocated buffer being retrieved
	// using the member function GetAllocatedBuffer()
	// Returns everything { nullptr, 0 } if the function does not exist
	ECSENGINE_API ModuleSerializeComponentStreams LoadModuleSerializeComponentFunctors(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// It does a single coalesced allocation. Deallocate the buffer to free the memory
	ECSENGINE_API Stream<ModuleLinkComponentTarget> LoadModuleLinkComponentTargets(
		ModuleRegisterLinkComponentFunction register_function,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	// It will not release the OS Handle - it must be kept around as long as the link components are loaded;
	// It does a single coalesced allocation. Deallocate the buffer to free the memory
	// Returns { nullptr, 0 } if the function does not exist
	ECSENGINE_API Stream<ModuleLinkComponentTarget> LoadModuleLinkComponentTargets(
		const Module* module,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	// It will not release the OS Handle
	// It does a single coalesced allocation. Deallocate the buffer to free the memory
	// Returns { nullptr, 0 } if the function does not exist
	ECSENGINE_API ModuleExtraInformation LoadModuleExtraInformation(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// It does a single coalesced allocation. Dealloate the buffer to free the memory
	ECSENGINE_API ModuleExtraInformation LoadModuleExtraInformation(
		ModuleRegisterExtraInformationFunction function,
		AllocatorPolymorphic allocator
	);

	ECSENGINE_API Stream<ModuleDebugDrawTaskElement> LoadModuleDebugDrawTaskElements(
		const Module* module, 
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API Stream<ModuleDebugDrawTaskElement> LoadModuleDebugDrawTaskElements(
		ModuleRegisterDebugDrawTaskElementsFunction function, 
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	// The component names can be made empty. They are used to validate that the functions
	// Have valid targets
	ECSENGINE_API Stream<ModuleComponentFunctions> LoadModuleComponentFunctions(
		const Module* module,
		AllocatorPolymorphic allocator,
		Stream<Stream<char>> component_names = {},
		CapacityStream<char>* error_message = nullptr
	);

	// The component names can be made empty. They are used to validate that the functions
	// Have valid targets
	ECSENGINE_API Stream<ModuleComponentFunctions> LoadModuleComponentFunctions(
		ModuleRegisterComponentFunctionsFunction function,
		AllocatorPolymorphic allocator,
		Stream<Stream<char>> component_names = {},
		CapacityStream<char>* error_message = nullptr
	);

	// Frees the OS handle to the valid module function but it does not deallocate the tasks
	// or any other stream that was previously allocated. They must be manually deallocated
	// If the bool pointer is specified, it will attempt to unload the debugging symbols
	// and set the value with the success of that action
	ECSENGINE_API void ReleaseModule(Module* module, bool* unload_debugging_symbols = nullptr);

	// If the bool pointer is specified, it will attempt to unload the debugging symbols
	// and set the value with the success of that action
	ECSENGINE_API void ReleaseModuleHandle(void* handle, bool* unload_debugging_symbols = nullptr);

	ECSENGINE_API void ReleaseAppliedModuleStreams(AppliedModule* module, AllocatorPolymorphic allocator);

	// If the bool pointer is specified, it will attempt to unload the debugging symbols
	// and set the value with the success of that action
	ECSENGINE_API void ReleaseAppliedModule(AppliedModule* module, AllocatorPolymorphic allocator, bool* unload_debugging_symbols = nullptr);

	ECSENGINE_API bool ValidateModuleTask(const TaskSchedulerElement& element, CapacityStream<char>* error_message = nullptr);

	// It will move all the valid tasks in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of the valid tasks
	ECSENGINE_API size_t ValidateModuleTasks(Stream<TaskSchedulerElement>& tasks, CapacityStream<char>* error_message = nullptr);

	// It will move all the valid build types in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of the valid build types
	ECSENGINE_API size_t ValidateModuleBuildAssetTypes(Stream<ModuleBuildAssetType>& build_types, CapacityStream<char>* error_message = nullptr);

	// It will move all the valid link targets in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of the valid link targets
	ECSENGINE_API size_t ValidateModuleLinkComponentTargets(Stream<ModuleLinkComponentTarget>& link_targets, CapacityStream<char>* error_message = nullptr);

	// It will move all the valid link targets in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of the valid link targets
	ECSENGINE_API size_t ValidateModuleDebugDrawTaskElements(Stream<ModuleDebugDrawTaskElement>& elements, CapacityStream<char>* error_message = nullptr);

	// It will move all the valid link targets in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of valid component functions
	ECSENGINE_API size_t ValidateModuleComponentFunctions(
		Stream<ModuleComponentFunctions>& elements, 
		Stream<Stream<char>> component_names, 
		CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API ModuleComponentBuildFunction GetModuleComponentBuildFunction(const AppliedModule* applied_module, Stream<char> component_name);

	ECSENGINE_API ModuleComponentBuildEntry* GetModuleComponentBuildEntry(AppliedModule* applied_module, Stream<char> component_name);

	ECSENGINE_API const ModuleComponentBuildEntry* GetModuleComponentBuildEntry(const AppliedModule* applied_module, Stream<char> component_name);

	ECSENGINE_API void AddModuleDebugDrawTaskElementsToScheduler(TaskScheduler* scheduler, Stream<ModuleDebugDrawTaskElement> elements, bool scene_order);

	// It will copy the initialize data from the source manager to the tasks that want it in the target manager
	ECSENGINE_API void RetrieveModuleDebugDrawTaskElementsInitializeData(const TaskScheduler* scheduler, TaskManager* target_manager, const TaskManager* source_manager);

	ECSENGINE_API void DisableModuleDebugDrawTaskElement(TaskManager* task_manager, Stream<char> task_name);

	ECSENGINE_API void EnableModuleDebugDrawTaskElement(TaskManager* task_manager, Stream<char> task_name);

	// Returns the current state
	ECSENGINE_API bool FlipModuleDebugDrawTaskElement(TaskManager* task_manager, Stream<char> task_name);

	ECSENGINE_API bool IsModuleDebugDrawTaskElementEnabled(const TaskManager* task_manager, Stream<char> task_name);

	// It will set to enabled the given tasks
	ECSENGINE_API void SetEnabledModuleDebugDrawTaskElements(TaskManager* task_manager, Stream<Stream<char>> task_names);

}