#include "ecspch.h"
#include "PhysicalMemoryProfiler.h"
#include "../OS/PhysicalMemory.h"
#include "../ECS/World.h"

#define PHYSICAL_REGION_INITIAL_CAPACITY 32
#define GUARD_REGIONS_INITIAL_CAPACITY 32

namespace ECSEngine {

	void PhysicalMemoryProfiler::StartFrame()
	{
		
	}

	void PhysicalMemoryProfiler::StartSimulation(const World* world)
	{
		
	}

	void PhysicalMemoryProfiler::AddEntry(Stream<void> region)
	{
		// Check to see if this region is contained in the existing one, in order to skip it
		unsigned int index = 0;
		for (; index < region_entries.size; index++) {
			if (region_entries[index].region.Contains(region)) {
				// This region is already monitored, no need to add it
				break;
			}
		}
		if (index == region_entries.size) {
			region_entries.ReserveNewElement();
			region_entries[index].region = region;
			region_entries[index].physical_regions.Initialize(Allocator(), PHYSICAL_REGION_INITIAL_CAPACITY);
			region_entries[index].guard_pages_hit.Initialize(Allocator(), GUARD_REGIONS_INITIAL_CAPACITY);
			region_entries.size++;
		}
	}

	void PhysicalMemoryProfiler::Clear()
	{
		memory_usage.Clear();
	}

	void PhysicalMemoryProfiler::EndSimulation()
	{
		// Nothing to be done at the moment
	}

	void PhysicalMemoryProfiler::EndFrame()
	{
		size_t total_usage = 0;
		for (unsigned int index = 0; index < region_entries.size; index++) {
			total_usage += region_entries[index].current_usage;
		}
		memory_usage.Add(total_usage);
	}

	void PhysicalMemoryProfiler::Initialize(AllocatorPolymorphic allocator, unsigned int entry_capacity)
	{
		memory_usage.Initialize(allocator, entry_capacity);
	}

	size_t PhysicalMemoryProfilerAllocatorSize(unsigned int entry_capacity)
	{
		return Statistic<size_t>::MemoryOf(entry_capacity);
	}

}
