#include "ecspch.h"
#include "PhysicalMemoryProfiler.h"
#include "../OS/PhysicalMemory.h"
#include "../ECS/World.h"
#include "PhysicalMemoryProfilerGlobal.h"

#define PHYSICAL_REGION_INITIAL_CAPACITY 16
#define GUARD_REGIONS_INITIAL_CAPACITY 16

// This value should be low enough to not induce too much overhead
// For each frame
#define CYCLIC_VERIFY_BYTES_PER_FRAME ECS_MB * 10

namespace ECSEngine {

	// Returns the index where the entry is located
	// The GetRegion functor must return a Stream<void>*
	template<typename GetRegion, typename RegionType>
	static unsigned int AddPageToRegions(ResizableStream<RegionType>& regions, Stream<void> new_region, GetRegion&& get_region) {
		unsigned int previous_region_index = -1;
		unsigned int next_region_index = -1;

		void* next_page = OffsetPointer(new_region);
		for (unsigned int index = 0; index < regions.size; index++) {
			Stream<void> current_region = *get_region(index);
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
			entry_index = regions.ReserveRange();
			Stream<void>* entry_region = get_region(entry_index);
			*entry_region = new_region;
		}
		else if (previous_region_index != -1 && next_region_index != -1) {
			Stream<void>* previous_region = get_region(previous_region_index);
			Stream<void> next_region = *get_region(next_region_index);

			// Both blocks are found - can coalesce these 2
			Stream<void> coalesced_region = { previous_region->buffer, previous_region->size + new_region.size + next_region.size };
			// Replace the previous value and replace swap back the next region
			*previous_region = coalesced_region;
			regions.RemoveSwapBack(next_region_index);
			entry_index = previous_region_index;
		}
		else if (previous_region_index != -1) {
			// Only previous region
			// Can increase its size
			Stream<void>* previous_region = get_region(previous_region_index);
			previous_region->size += new_region.size;
			entry_index = previous_region_index;
		}
		else {
			// Only next block, can bump back the pointer and increase the size
			Stream<void>* next_region = get_region(next_region_index);
			next_region->buffer = OffsetPointer(next_region->buffer, -(int64_t)new_region.size);
			next_region->size += new_region.size;
			entry_index = next_region_index;
		}	
	
		return entry_index;
	}

	void PhysicalMemoryProfiler::StartFrame()
	{
		
	}

	void PhysicalMemoryProfiler::StartSimulation()
	{
		// Also change the task manager simulation exception handler
		SetTaskManagerPhysicalMemoryProfilingExceptionHandler(task_manager);

		// Here, change the virtual allocation pages to the GUARD status
		// In order to have them be registered afterwards into the physical memory pool
		for (unsigned int entry_index = 0; entry_index < region_entries.size; entry_index++) {
			// Assert in order to make sure that we don't miss a call when this fails
			ECS_ASSERT(OS::GuardPages(region_entries[entry_index].region.buffer, region_entries[entry_index].region.size));
		}
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
			region_entries.Reserve();

			size_t page_size = OS::GetPhysicalMemoryPageSize();
			// We should monitor only the pages which are fully contained in this region
			// Since those outer ones, when we protect them, it can make code that accesses nearby addresses
			// Fault since it does not know that this comes from a protected page
			void* region_start = AlignPointer(region.buffer, page_size);
			void* region_end = (void*)(AlignPointerStack((uintptr_t)OffsetPointer(region.buffer, region.size), page_size) - page_size);
			region_entries[index].region = { region_start, PointerDifference(region_end, region_start) };
			region_entries[index].physical_regions.Initialize(Allocator(), PHYSICAL_REGION_INITIAL_CAPACITY);
			region_entries[index].guard_pages_hit.Initialize(Allocator(), thread_count);
			AllocatorPolymorphic multithreaded_allocator = Allocator();
			multithreaded_allocator.allocation_type = ECS_ALLOCATION_MULTI;
			for (unsigned int thread_index = 0; thread_index < thread_count; thread_index++) {
				region_entries[index].guard_pages_hit[thread_index].value.Initialize(multithreaded_allocator, GUARD_REGIONS_INITIAL_CAPACITY);
			}
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
		cyclic_initial_region = {};
		cyclic_region_verify = {};
		cyclic_region_index = 0;
		cyclic_subregion_index = 0;
		cyclic_region_usage = 0;
	}

	void PhysicalMemoryProfiler::Detach()
	{
		EndSimulation();
	}

	void PhysicalMemoryProfiler::CommitGuardPagesIntoPhysical()
	{
		for (unsigned int region_index = 0; region_index < region_entries.size; region_index++) {
			for (unsigned int thread_index = 0; thread_index < thread_count; thread_index++) {
				ResizableStream<Stream<void>>& thread_pages = region_entries[region_index].guard_pages_hit[thread_index].value;
				ResizableStream<Entry::PhysicalRegion>& physical_regions = region_entries[region_index].physical_regions;
				for (unsigned int index = 0; index < thread_pages.size; index++) {
					// We need the size in order to check later one if this is a new entry to correctly
					// Attribute the physical memory usage
					unsigned int physical_region_size = physical_regions.size;
					unsigned int entry_index = AddPageToRegions(physical_regions, thread_pages[index], [&](unsigned int entry_index) {
						return &physical_regions[entry_index].region;
					});

					// Determine how much physical memory usage this thread region has. It should be close
					// To the region size
					size_t physical_memory = OS::GetPhysicalMemoryBytesForAllocation(thread_pages[index].buffer, thread_pages[index].size);
					if (physical_region_size == physical_regions.size) {
						physical_regions[entry_index].physical_memory_usage += physical_memory;
					}
					else {
						physical_regions[entry_index].physical_memory_usage = physical_memory;
					}
					region_entries[region_index].current_usage += physical_memory;
				}
				if (thread_pages.capacity > GUARD_REGIONS_INITIAL_CAPACITY) {
					// Resize to that size such that we can reduce memory consumption in case we get an overload
					// Of pages for some threads
					//thread_pages.ResizeNoCopy(GUARD_REGIONS_INITIAL_CAPACITY);
				}
				thread_pages.Clear();
			}
		}
	}

	void PhysicalMemoryProfiler::EndSimulation()
	{
		// Unguard the pages just in case the next simulation doesn't have these
		for (unsigned int index = 0; index < region_entries.size; index++) {
			ECS_ASSERT(OS::UnguardPages(region_entries[index].region.buffer, region_entries[index].region.size));
		}
		RemoveTaskManagerPhysicalMemoryProfilingExceptionHandler(task_manager);
	}

	void PhysicalMemoryProfiler::EndFrame()
	{
		// Update the usage before that since it has a higher chance of not having mismatches
		// Since the commit is done afterwards
		//Timer my_timer;
		UpdateExistingRegionsUtilizationIteration();
		//float update_duration = my_timer.GetDurationFloat(ECS_TIMER_DURATION_MS);

		// Commit the pages into the main buffers
		CommitGuardPagesIntoPhysical();

		//float commit_duation = my_timer.GetDurationFloat(ECS_TIMER_DURATION_MS);

		size_t total_usage = 0;
		for (unsigned int index = 0; index < region_entries.size; index++) {
			total_usage += region_entries[index].current_usage;
		}
		memory_usage.Add(total_usage);
		//float duration = my_timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
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
			// We also need to align the pointer to the beginning of the page such that we can coalesce these
			// Blocks later on
			void* page_start = (void*)(AlignPointerStack((uintptr_t)page, page_size) - page_size);
			ResizableStream<Stream<void>>* thread_pages = &region_entries[region_index].guard_pages_hit[thread_id].value;
			AddPageToRegions(*thread_pages, { page_start, page_size }, [thread_pages](unsigned int index) {
				return &thread_pages->buffer[index];
			});
			return true;
		}
		return false;
	}

	void PhysicalMemoryProfiler::UpdateExistingRegionsUtilizationIteration()
	{
		// There must be at least a region inserted - otherwise it complicated the code too much
		// We can simply return in this case
		if (region_entries.size == 0) {
			return;
		}

		// Verify if the current entry index is still in bounds - if it is not
		// It means that entries have been removed
		if (cyclic_region_index >= region_entries.size) {
			cyclic_region_index = 0;
			cyclic_subregion_index = 0;
			// Look for a region with physical pages
			while (cyclic_region_index < region_entries.size && region_entries[cyclic_region_index].physical_regions.size == 0) {
				cyclic_region_index++;
			}
			
			if (cyclic_region_index == region_entries.size) {
				cyclic_initial_region = {};
				cyclic_region_index = 0;
			}
			else {
				cyclic_initial_region = region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].region;
			}
			cyclic_region_verify = cyclic_initial_region;
			cyclic_region_usage = 0;
		}

		if (cyclic_subregion_index >= region_entries[cyclic_region_index].physical_regions.size) {
			// Reset
			cyclic_subregion_index = 0;
			cyclic_region_usage = 0;
			if (region_entries[cyclic_region_index].physical_regions.size > 0) {
				cyclic_initial_region = region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].region;
			}
			else {
				unsigned int initial_loop_index = cyclic_region_index;
				while (cyclic_region_index < region_entries.size && region_entries[cyclic_region_index].physical_regions.size == 0) {
					cyclic_region_index++;
				}
				if (cyclic_region_index == region_entries.size) {
					// Continue looking
					cyclic_region_index = 0;
					while (cyclic_region_index < initial_loop_index && region_entries[cyclic_region_index].physical_regions.size == 0) {
						cyclic_region_index++;
					}
					if (cyclic_region_index == initial_loop_index) {
						cyclic_region_index = 0;
						cyclic_initial_region = {};
					}
				}
				else {
					cyclic_initial_region = region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].region;
				}
			}
			cyclic_region_verify = cyclic_initial_region;
		}


		// Verify if the initial region still matches with the current region
		if (region_entries[cyclic_region_index].physical_regions.size > 0) {
			if (cyclic_initial_region != region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].region) {
				cyclic_initial_region = region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].region;
				cyclic_region_verify = cyclic_initial_region;
				cyclic_region_usage = 0;
			}

			auto handle_finished_region_case = [this]() {
				if (cyclic_region_verify.size == 0) {
					// Commit the delta change
					size_t usage_delta = cyclic_region_usage - region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].physical_memory_usage;
					region_entries[cyclic_region_index].current_usage += usage_delta;
					region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].physical_memory_usage = cyclic_region_usage;

					// Go to the next region/subregion
					if (region_entries[cyclic_region_index].physical_regions.size + 1 <= cyclic_subregion_index) {
						cyclic_region_index = (cyclic_region_index + 1) % region_entries.size;
						// Try to find a region with physical regions - do this only until the end of the entries array
						// We could loop back, but probably not worth it
						while (cyclic_region_index < region_entries.size && region_entries[cyclic_region_index].physical_regions.size == 0) {
							cyclic_region_index++;
						}
						if (cyclic_region_index == region_entries.size) {
							cyclic_region_index = 0;
						}
						cyclic_subregion_index = 0;
					}
					else {
						cyclic_subregion_index++;
					}
					if (region_entries[cyclic_region_index].physical_regions.size > cyclic_subregion_index) {
						cyclic_initial_region = region_entries[cyclic_region_index].physical_regions[cyclic_subregion_index].region;
					}
					else {
						cyclic_initial_region = {};
					}
					cyclic_region_verify = cyclic_initial_region;
					cyclic_region_usage = 0;
				}
			};

			handle_finished_region_case();

			// Keep verifying regions until we either run out of frame check capacity
			// Or we loop back to the same region
			size_t frame_max_verify_count = CYCLIC_VERIFY_BYTES_PER_FRAME;
			unsigned int iteration_starting_region_index = cyclic_region_index;

			auto handle_iteration_check = [&]() {
				// Determine the amount that we can verify in this iteration
				size_t iteration_check_amount = ClampMax(cyclic_region_verify.size, frame_max_verify_count);
				size_t current_subregion_usage = OS::GetPhysicalMemoryBytesForAllocation(cyclic_region_verify.buffer, iteration_check_amount);
				cyclic_region_usage += current_subregion_usage;

				// Let the next iteration deal with the 0 case - when we exhaust this subregion
				cyclic_region_verify.size -= iteration_check_amount;
				frame_max_verify_count -= iteration_check_amount;
				cyclic_region_verify.buffer = OffsetPointer(cyclic_region_verify.buffer, iteration_check_amount);

				if (cyclic_region_verify.size == 0) {
					handle_finished_region_case();
				}
			};

			while (cyclic_region_index < region_entries.size && frame_max_verify_count > 0 && cyclic_region_verify.size > 0) {
				handle_iteration_check();
			}
			if (frame_max_verify_count > 0) {
				cyclic_region_index = 0;
				while (cyclic_region_index < iteration_starting_region_index && frame_max_verify_count > 0 && cyclic_region_verify.size > 0) {
					handle_iteration_check();
				}
			}
		}
	}

	void PhysicalMemoryProfiler::Reattach()
	{
		StartSimulation();
	}

	void PhysicalMemoryProfiler::Initialize(AllocatorPolymorphic _allocator, TaskManager* _task_manager, unsigned int entry_capacity)
	{
		// Memset this for the cyclic information to be zeroed out
		memset(this, 0, sizeof(*this));

		task_manager = _task_manager;
		memory_usage.Initialize(_allocator, entry_capacity);
		region_entries.Initialize(_allocator, 0);
		thread_count = task_manager->GetThreadCount();
	}

	size_t PhysicalMemoryProfilerAllocatorSize(unsigned int thread_count, unsigned int entry_capacity)
	{
		// Estimate 20 region entries, with 10'000 entries for the physical regions and the default count
		// For the page guards (these are transitory anyways)
		const size_t REGION_ENTRY_COUNT = 20;
		const size_t TOTAL_PHYSICAL_REGIONS = 10'000;
		return Statistic<size_t>::MemoryOf(entry_capacity) + ResizableStream<PhysicalMemoryProfiler::Entry>::MemoryOf(REGION_ENTRY_COUNT)
			+ thread_count * REGION_ENTRY_COUNT * Stream<CacheAligned<ResizableStream<Stream<void>>>>::MemoryOf(GUARD_REGIONS_INITIAL_CAPACITY)
			+ ResizableStream<PhysicalMemoryProfiler::Entry::PhysicalRegion>::MemoryOf(TOTAL_PHYSICAL_REGIONS);
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

	static OS::ECS_OS_EXCEPTION_CONTINUE_STATUS PhysicalMemoryGuardPageExceptionHandler(TaskManagerExceptionHandlerData* data) {
		if (data->exception_information.error_code == OS::ECS_OS_EXCEPTION_PAGE_GUARD) {
			bool was_handled = HandlePhysicalMemoryProfilerGuardPage(data->thread_id, data->exception_information.faulting_page);
			return was_handled ? OS::ECS_OS_EXCEPTION_CONTINUE_RESOLVED : OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
		}
		else {
			return OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
		}
	}

	void SetTaskManagerPhysicalMemoryProfilingExceptionHandler(TaskManager* task_manager)
	{
		task_manager->PushExceptionHandler(PhysicalMemoryGuardPageExceptionHandler, nullptr, 0);
	}

	void RemoveTaskManagerPhysicalMemoryProfilingExceptionHandler(TaskManager* task_manager)
	{
		task_manager->PopExceptionHandler();
	}

}
