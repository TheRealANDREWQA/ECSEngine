#pragma once
#include "../../Core.h"
#include "ModuleDefinition.h"

#define ECS_MODULE_EXTENSION L".dll"
#define ECS_MODULE_EXTENSION_ASCII ".dll"

namespace ECSEngine {

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

	// Returns nullptr if there is none loaded
	ECSENGINE_API void* GetModuleHandleFromPath(Stream<char> module_path);

	// Returns nullptr if there is none loaded
	ECSENGINE_API void* GetModuleHandleFromPath(Stream<wchar_t> module_path);

	// It does not load any stream from the functions. Use the corresponding load functions for that
	// The function LoadModuleTasks can be used to load them at a later time. If the boolean pointer
	// is specified, it will attempt to load the debugging symbols for this module and it will set the
	// boolean value with the success of that load.
	ECSENGINE_API Module LoadModule(Stream<wchar_t> path, bool* load_debug_symbols = nullptr);

	// Loads the streams from the given module
	ECSENGINE_API void LoadAppliedModule(AppliedModule* module, AllocatorPolymorphic allocator, CapacityStream<char>* error_message = nullptr);

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

	ECSENGINE_API Stream<ModuleDebugDrawElement> LoadModuleDebugDrawElements(const Module* module, AllocatorPolymorphic allocator);

	ECSENGINE_API Stream<ModuleDebugDrawElement> LoadModuleDebugDrawElements(ModuleRegisterDebugDrawFunction function, AllocatorPolymorphic allocator);

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

	// It will move all the valid tasks in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of the valid tasks
	ECSENGINE_API size_t ValidateModuleTasks(Stream<TaskSchedulerElement>& tasks, CapacityStream<char>* error_message = nullptr);

	// It will move all the valid build types in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of the valid build types
	ECSENGINE_API size_t ValidateModuleBuildAssetTypes(Stream<ModuleBuildAssetType>& build_types, CapacityStream<char>* error_message = nullptr);

	// It will move all the valid link targets in front of the stream while keeping the invalid ones at the end
	// Returns the previous size, the new size will reflect the count of the valid link targets
	ECSENGINE_API size_t ValidateModuleLinkComponentTargets(Stream<ModuleLinkComponentTarget>& link_targets, CapacityStream<char>* error_message = nullptr);

}