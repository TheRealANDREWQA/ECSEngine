#pragma once
#include "../Core.h"

namespace ECSEngine {

	struct PhysicalMemoryProfiler;

	// Global variable such that performance measuring macros / functions can simply
	// Refer to this one without having to provide the instance manually for each function
	ECSENGINE_API extern PhysicalMemoryProfiler* ECS_PHYSICAL_MEMORY_PROFILER_GLOBAL;

	ECSENGINE_API void ChangePhysicalMemoryProfilerGlobal(PhysicalMemoryProfiler* profiler);

	// Returns true if the guard page came from the monitored regions, else false
	ECSENGINE_API bool HandlePhysicalMemoryProfilerGuardPage(unsigned int thread_id, const void* page);

}