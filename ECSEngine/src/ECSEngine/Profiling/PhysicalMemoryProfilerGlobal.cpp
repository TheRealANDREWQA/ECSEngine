#include "ecspch.h"
#include "PhysicalMemoryProfilerGlobal.h"
#include "PhysicalMemoryProfiler.h"

namespace ECSEngine {

	PhysicalMemoryProfiler* ECS_PHYSICAL_MEMORY_PROFILER_GLOBAL;

	void ChangePhysicalMemoryProfilerGlobal(PhysicalMemoryProfiler* profiler) {
		ECS_PHYSICAL_MEMORY_PROFILER_GLOBAL = profiler;
	}

	bool HandlePhysicalMemoryProfilerGuardPage(unsigned int thread_id, const void* page) {
		return ECS_PHYSICAL_MEMORY_PROFILER_GLOBAL->HandlePageGuardEnter(thread_id, page);
	}

}