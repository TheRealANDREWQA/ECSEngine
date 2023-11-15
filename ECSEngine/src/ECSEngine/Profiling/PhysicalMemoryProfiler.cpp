#include "ecspch.h"
#include "PhysicalMemoryProfiler.h"
#include "../OS/PhysicalMemory.h"

namespace ECSEngine {

	void PhysicalMemoryProfiler::Begin()
	{
		// Record the current process memory usage
		last_process_usage = OS::ProcessPhysicalMemoryUsage();
	}

	void PhysicalMemoryProfiler::Clear()
	{
		memory_usage.Clear();
		last_process_usage = 0;
	}

	void PhysicalMemoryProfiler::End()
	{
		size_t current_process_usage = OS::ProcessPhysicalMemoryUsage();
		size_t difference = current_process_usage - last_process_usage;
		last_process_usage = current_process_usage;
		memory_usage.Add(difference);
	}

	void PhysicalMemoryProfiler::Initialize(AllocatorPolymorphic allocator, unsigned int entry_capacity)
	{
		memory_usage.Initialize(allocator, entry_capacity);
		last_process_usage = 0;
	}

}
