#pragma once
#include "../Core.h"
#include "Statistic.h"
#include "../Utilities/ByteUnits.h"

namespace ECSEngine {

	struct World;

	// A default value for the allocator to fit the given entry count
	ECSENGINE_API size_t PhysicalMemoryProfilerAllocatorSize(unsigned int entry_capacity);

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

		void StartFrame();
		
		// This needs to be called after the entries have been inserted
		void StartSimulation();

		// Returns the value of bytes
		ECS_INLINE size_t GetUsage(ECS_STATISTIC_VALUE_TYPE value_type, ECS_BYTE_UNIT_TYPE unit_type) const {
			return ConvertToByteUnit(memory_usage.GetValue(value_type), unit_type);
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int thread_count, unsigned int entry_capacity);

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
	};

	ECSENGINE_API void AddAllocatorToPhysicalMemoryProfiling(AllocatorPolymorphic allocator, PhysicalMemoryProfiler* profiler);

}