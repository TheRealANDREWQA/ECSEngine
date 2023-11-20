#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct AllocatorProfiling;

	// This function pointer is defined here such that we can use it in the AddEntry
	// Function from this file

	// For custom allocators, we can use this callback to update their current frame usage
	typedef size_t (*AllocatorProfilingCustomAllocatorUsage)(const void* allocator);

	typedef void (*AllocatorProfilingCustomExitFunction)(void* allocator);

	// Global variable such that performance measuring macros / functions can simply
	// Refer to this one without having to provide the instance manually for each function
	ECSENGINE_API extern AllocatorProfiling* ECS_ALLOCATOR_PROFILING_GLOBAL;

	ECSENGINE_API void ChangeAllocatorProfilingGlobal(AllocatorProfiling* profiler);

	ECSENGINE_API void AllocatorProfilingAddEntry(
		void* address,
		ECS_ALLOCATOR_TYPE allocator_type,
		Stream<char> name,
		AllocatorProfilingCustomAllocatorUsage custom_usage_function = nullptr,
		AllocatorProfilingCustomExitFunction custom_exit_function = nullptr
	);

	ECSENGINE_API void AllocatorProfilingAddAllocation(const void* address, size_t current_usage, size_t block_count = 0, unsigned char suballocator_count = 0);

	ECSENGINE_API void AllocatorProfilingAddDeallocation(const void* address);

}