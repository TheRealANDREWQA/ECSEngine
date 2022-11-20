#pragma once
#include "../../Core.h"
#include "ModuleDefinition.h"

#define ECS_MODULE_EXTENSION L".dll"

namespace ECSEngine {

	// Returns true if it has the associated DLL task function
	// The other are optional and do not need to be present in order for the module to be valid
	ECSENGINE_API bool FindModule(Stream<wchar_t> path);

	// It does not load any stream from the functions. Use the corresponding load functions for that
	// The function LoadModuleTasks can be used to load them at a later time
	ECSENGINE_API Module LoadModule(Stream<wchar_t> path);

	// Loads the streams from the given module
	ECSENGINE_API void LoadAppliedModule(AppliedModule* module, AllocatorPolymorphic allocator);

	// It will not release the OS Handle - it must be kept around as long as the tasks are loaded;
	// It does a single coallesced allocation. Deallocate the buffer to free the memory
	ECSENGINE_API Stream<TaskSchedulerElement> LoadModuleTasks(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// It will not releaase the OS Handle - it must be kept around as long as the descriptors are loaded;
	// It does a single coallesced allocation. Deallocate the buffer to free the memory
	// Returns { nullptr, 0 } if the function does not exist
	ECSENGINE_API Stream<Tools::UIWindowDescriptor> LoadModuleUIDescriptors(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// It will not releaase the OS Handle - it must be kept around as long as the build asset types are loaded;
	// It does a single coallesced allocation. Deallocate the buffer to free the memory
	// Returns { nullptr, 0 } if the function does not exist
	ECSENGINE_API Stream<ModuleBuildAssetType> LoadModuleBuildAssetTypes(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// It will not releaase the OS Handle - it must be kept around as long as the functors are loaded;
	// It does a single coallesced allocation with the allocated buffer being retrieved
	// using the member function GetAllocatedBuffer()
	// Returns everything { nullptr, 0 } if the function does not exist
	ECSENGINE_API ModuleSerializeComponentStreams LoadModuleSerializeComponentFunctors(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// It will not releaase the OS Handle - it must be kept around as long as the link components are loaded;
	// It does a single coallesced allocation. Deallocate the buffer to free the memory
	// Returns { nullptr, 0 } if the function does not exist
	ECSENGINE_API Stream<ModuleLinkComponentTarget> LoadModuleLinkComponentTargets(
		const Module* module,
		AllocatorPolymorphic allocator
	);

	// Frees the OS handle to the valid module function but it does not deallocate the tasks
	// or any other stream that was previously allocated. They must be manually deallocated.
	ECSENGINE_API void ReleaseModule(Module* module);

	ECSENGINE_API void ReleaseAppliedModuleStreams(AppliedModule* module, AllocatorPolymorphic allocator);

	ECSENGINE_API void ReleaseAppliedModule(AppliedModule* module, AllocatorPolymorphic allocator);

	// It will move all the valid types in the front of the stream while keeping the invalid ones
	// at the end
	ECSENGINE_API void ValidateModuleBuildAssetTypes(Stream<ModuleBuildAssetType>& build_types);

	// It will move all the valid types in the front of the stream while keeping the invalid ones
	// at the end
	ECSENGINE_API void ValidateModuleLinkComponentTargets(Stream<ModuleLinkComponentTarget>& link_targets);

}