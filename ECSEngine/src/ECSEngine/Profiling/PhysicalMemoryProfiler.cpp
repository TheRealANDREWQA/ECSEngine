#include "ecspch.h"
#include "PhysicalMemoryProfiler.h"
#include "../OS/PhysicalMemory.h"
#include "../ECS/World.h"

#define PHYSICAL_REGION_INITIAL_CAPACITY 32
#define GUARD_REGIONS_INITIAL_CAPACITY 32

namespace ECSEngine {

	// Returns the index where the entry is located
	// The GetRegion functor must return a Stream<void>&
	template<typename GetRegion, typename RegionType>
	static unsigned int AddPageToRegions(ResizableStream<RegionType>& regions, Stream<void> new_region, GetRegion&& get_region) {
		unsigned int previous_region_index = -1;
		unsigned int next_region_index = -1;

		void* next_page = OffsetPointer(new_region);
		for (unsigned int index = 0; index < regions.size; index++) {
			Stream<void> current_region = get_region(index);
			if (OffsetPointer(current_region) == new_region.buffer) {
				previous_region_index = index;
			}
			else if (current_region.buffer == next_page) {
				next_region_index = -1;
			}
		}

		unsigned int entry_index = -1;
		if (previous_region_index == -1 && next_region_index == -1) {
			// No block is found, insert it as a new region
			entry_index = regions.ReserveNewElement();
			Stream<void>& entry_region = get_region(entry_index);
			entry_region = new_region;
		}
		else if (previous_region_index != -1 && next_region_index != -1) {
			Stream<void>& previous_region = get_region(previous_region_index);
			Stream<void> next_region = get_region(next_region_index);

			// Both blocks are found - can coalesce these 2
			Stream<void> coalesced_region = { previous_region.buffer, previous_region.size + new_region.size + next_region.size };
			// Replace the previous value and replace swap back the next region
			previous_region = coalesced_region;
			regions.RemoveSwapBack(next_region_index);
			entry_index = previous_region_index;
		}
		else if (previous_region_index != -1) {
			// Only previous region
			// Can increase its size
			Stream<void>& previous_region = get_region(previous_region_index);
			previous_region.size += new_region.size;
			entry_index = previous_region_index;
		}
		else {
			// Only next block, can bump back the pointer and increase the size
			Stream<void>& next_region = get_region(next_region_index);
			next_region.buffer = OffsetPointer(next_region.buffer, -(int64_t)new_region.size);
			next_region.size += new_region.size;
			entry_index = next_region_index;
		}	
	
		return entry_index;
	}

	void PhysicalMemoryProfiler::StartFrame()
	{
		
	}

	void PhysicalMemoryProfiler::StartSimulation()
	{
		// Here, change the virtual allocation pages to the GUARD status
		// In order to have them be registered afterwards into the physical memory pool
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
			region_entries[index].guard_pages_hit.Initialize(Allocator(), thread_count);
			AllocatorPolymorphic multithreaded_allocator = Allocator();
			multithreaded_allocator.allocation_type = ECS_ALLOCATION_MULTI;
			for (unsigned int thread_index = 0; thread_index < thread_count; thread_index++) {
				region_entries[index].guard_pages_hit[thread_index].value.Initialize(multithreaded_allocator, GUARD_REGIONS_INITIAL_CAPACITY);
			}
			region_entries.size++;
		}
	}

	void PhysicalMemoryProfiler::Clear()
	{
		memory_usage.Clear();
		for (unsigned int index = 0; index < region_entries.size; index++) {
			region_entries[index].physical_regions.FreeBuffer();
			for (unsigned int thread_index = 0; thread_index < thread_count; thread_index++) {
				region_entries[index].guard_pages_hit[thread_index].value.FreeBuffer();
			}
			region_entries[index].guard_pages_hit.Deallocate(Allocator());
		}
		region_entries.FreeBuffer();
	}

	void PhysicalMemoryProfiler::CommitGuardPagesIntoPhysical()
	{
		for (unsigned int region_index = 0; region_index < region_entries.size; region_index++) {
			for (unsigned int thread_index = 0; thread_index < thread_count; thread_index++) {
				ResizableStream<Stream<void>>& thread_pages = region_entries[region_index].guard_pages_hit[thread_index].value;
				for (unsigned int index = 0; index < thread_pages.size; index++) {
					// We need the size in order to check later one if this is a new entry to correctly
					// Attribute the physical memory usage
					unsigned int physical_region_size = region_entries[region_index].physical_regions.size;
					unsigned int entry_index = AddPageToRegions(region_entries[region_index].physical_regions, thread_pages[index], [&](unsigned int entry_index) {
						return region_entries[region_index].physical_regions[entry_index].region;
					});

					// Determine how much physical memory usage this thread region has. It should be close
					// To the region size
					size_t physical_memory = OS::GetPhysicalMemoryBytesForAllocation(thread_pages[index].buffer, thread_pages[index].size);
					if (physical_region_size == region_entries[region_index].physical_regions.size) {
						region_entries[region_index].physical_regions[entry_index].physical_memory_usage += physical_memory;
					}
					else {
						region_entries[region_index].physical_regions[entry_index].physical_memory_usage = physical_memory;
					}
				}
				thread_pages.Reset();
			}
		}
	}

	void PhysicalMemoryProfiler::EndSimulation()
	{
		// Nothing to be done at the moment
	}

	void PhysicalMemoryProfiler::EndFrame()
	{
		// Commit the pages first
		CommitGuardPagesIntoPhysical();

		size_t total_usage = 0;
		for (unsigned int index = 0; index < region_entries.size; index++) {
			total_usage += region_entries[index].current_usage;
		}
		memory_usage.Add(total_usage);
	}

	unsigned int PhysicalMemoryProfiler::FindRegion(const void* page) const
	{
		for (unsigned int index = 0; index < region_entries.size; index++) {
			if (IsPointerRange(region_entries[index].region.buffer, region_entries[index].region.size, page)) {
				return index;
			}
		}
		return -1;
	}

	bool PhysicalMemoryProfiler::HandlePageGuardEnter(unsigned int thread_id, const void* page)
	{
		unsigned int region_index = FindRegion(page);
		if (region_index != -1) {
			size_t page_size = OS::GetPhysicalMemoryPageSize();
			ResizableStream<Stream<void>>& thread_pages = region_entries[region_index].guard_pages_hit[thread_id].value;
			AddPageToRegions(thread_pages, { page, page_size }, [thread_pages](unsigned int index) {
				return thread_pages[index];
			});
			return true;
		}
		return false;
	}

	void PhysicalMemoryProfiler::Initialize(AllocatorPolymorphic _allocator, unsigned int _thread_count, unsigned int entry_capacity)
	{
		memory_usage.Initialize(_allocator, entry_capacity);
		region_entries.Initialize(_allocator, 0);
		thread_count = _thread_count;
	}

	size_t PhysicalMemoryProfilerAllocatorSize(unsigned int entry_capacity)
	{
		return Statistic<size_t>::MemoryOf(entry_capacity);
	}

	void AddAllocatorToPhysicalMemoryProfiling(AllocatorPolymorphic allocator, PhysicalMemoryProfiler* profiler)
	{
		const size_t CAPACITY = 512;
		ECS_STACK_CAPACITY_STREAM(void*, region_pointers, CAPACITY);
		ECS_STACK_CAPACITY_STREAM(size_t, region_size, CAPACITY);
		size_t region_count = GetAllocatorRegions(allocator, region_pointers.buffer, region_size.buffer, CAPACITY);
		ECS_ASSERT(region_count <= CAPACITY);
		for (size_t index = 0; index < region_count; index++) {
			profiler->AddEntry({ region_pointers[index], region_size[index] });
		}
	}

}
