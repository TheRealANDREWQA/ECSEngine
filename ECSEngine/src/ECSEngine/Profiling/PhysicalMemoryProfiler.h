#pragma once
#include "../Core.h"
#include "Statistic.h"
#include "../Utilities/ByteUnits.h"

namespace ECSEngine {

	struct World;
	struct TaskManager;

	// A default value for the allocator to fit the given entry count
	ECSENGINE_API size_t PhysicalMemoryProfilerAllocatorSize(unsigned int thread_count, unsigned int entry_capacity);

	struct ECSENGINE_API PhysicalMemoryProfiler {
		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return region_entries.allocator;
		}

		void AddEntry(Stream<void> region);

		void Clear();

		void CommitGuardPagesIntoPhysical();

		void EndSimulation();

		void EndFrame();

		// Returns -1 it doesn't find
		unsigned int FindRegion(const void* page) const;

		// Returns true if the page guard came from one of these regions, else false
		bool HandlePageGuardEnter(unsigned int thread_id, const void* page);

		void UpdateExistingRegionsUtilizationIteration();

		void StartFrame();
		
		// This needs to be called after the entries have been inserted
		void StartSimulation();

		// Returns the value of bytes
		ECS_INLINE size_t GetUsage(ECS_STATISTIC_VALUE_TYPE value_type, ECS_BYTE_UNIT_TYPE unit_type) const {
			return ConvertToByteUnit(memory_usage.GetValue(value_type), unit_type);
		}

		void Initialize(AllocatorPolymorphic allocator, TaskManager* task_manager, unsigned int entry_capacity);

		struct Entry {
			struct PhysicalRegion {
				Stream<void> region;
				// We need to keep a 
				size_t physical_memory_usage;
			};

			Stream<void> region;
			// This is an array in which the guard pages which have been triggered are stored
			// These are the regions which are of interest of. In these we will cycle through
			// Periodically to refresh the physical memory usage
			ResizableStream<PhysicalRegion> physical_regions;
			// When we get a guard page exception, we record it here
			// In order to have it verified that it actually contains
			// Physical memory. Have one entry for each thread such
			// We avoid atomic operations and have each resizable
			// Stream on a separate cache line. We also need to make
			// The allocator for these pages be multithreaded. We don't
			// Need to do that for the physical regions, since those
			// Are handled on a single thread
			Stream<CacheAligned<ResizableStream<Stream<void>>>> guard_pages_hit;
			size_t current_usage;
		};

		// The values are expressed in bytes
		Statistic<size_t> memory_usage;
		ResizableStream<Entry> region_entries;
		unsigned int thread_count;
		// This index to cycle through the entries in the region_entries
		// And re-verify the physical memory usage
		unsigned int cyclic_region_index;
		// This is the index inside the physical regions
		unsigned int cyclic_subregion_index;
		// If we detect a mismatch in the initial region and the current region value,
		// This means that, either the a region swap has happened, or the current region
		// Was extended. In either case, drop the information and restart again for this index
		Stream<void> cyclic_initial_region;
		Stream<void> cyclic_region_verify;
		// This is used to aggregate the entire verification
		// For that region
		size_t cyclic_region_usage;
		// When the simulation starts it will set the exception handler and when 
		// the simulation ends it will remove it
		TaskManager* task_manager;
	};

	ECSENGINE_API void AddAllocatorToPhysicalMemoryProfiling(AllocatorPolymorphic allocator, PhysicalMemoryProfiler* profiler);

	ECSENGINE_API void SetTaskManagerPhysicalMemoryProfilingExceptionHandler(TaskManager* task_manager);

	ECSENGINE_API void RemoveTaskManagerPhysicalMemoryProfilingExceptionHandler(TaskManager* task_manager);

}