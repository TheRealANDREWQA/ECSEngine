#include "ecspch.h"
#include "AllocatorProfilingGlobal.h"
#include "AllocatorProfiling.h"

namespace ECSEngine {

	AllocatorProfiling* ECS_ALLOCATOR_PROFILING_GLOBAL;

	void ChangeAllocatorProfilingGlobal(AllocatorProfiling* profiler)
	{
		ECS_ALLOCATOR_PROFILING_GLOBAL = profiler;
	}

	void AllocatorProfilingAddEntry(
		void* address, 
		ECS_ALLOCATOR_TYPE allocator_type, 
		Stream<char> name, 
		AllocatorProfilingCustomAllocatorUsage custom_usage_function,
		AllocatorProfilingCustomExitFunction custom_exit_function
	)
	{
		ECS_ALLOCATOR_PROFILING_GLOBAL->AddEntry(address, allocator_type, name, custom_usage_function, custom_exit_function);
	}

	void AllocatorProfilingAddAllocation(const void* address, size_t current_usage, size_t block_count, unsigned char suballocator_count)
	{
		ECS_ALLOCATOR_PROFILING_GLOBAL->AddAllocation(address, current_usage, block_count, suballocator_count);
	}

	void AllocatorProfilingAddDeallocation(const void* address)
	{
		ECS_ALLOCATOR_PROFILING_GLOBAL->AddDeallocation(address);
	}

}