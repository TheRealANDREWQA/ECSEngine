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

		bool AddPageGuard(void*);

		void Clear();

		void EndSimulation();

		void EndFrame();

		void StartFrame();
		
		// This needs to be called after the entries have been inserted
		void StartSimulation();

		// Returns the value of bytes
		ECS_INLINE size_t GetUsage(ECS_STATISTIC_VALUE_TYPE value_type, ECS_BYTE_UNIT_TYPE unit_type) const {
			return ConvertToByteUnit(memory_usage.GetValue(value_type), unit_type);
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int entry_capacity);

		struct Entry {
			Stream<void> region;
			// This is an array with all the contiguous physical pages that we have detected
			// Up until now
			ResizableStream<Stream<void>> physical_regions;
			// When we get a guard page exception, we record it here
			// In order to have it verified that it actually contains
			// Physical memory
			ResizableStream<Stream<void>> guard_pages_hit;
			size_t current_usage;
		};

		// The values are expressed in bytes
		Statistic<size_t> memory_usage;
		ResizableStream<Entry> region_entries;
	};

	ECSENGINE_API void AddAllocatorToPhysicalMemoryProfiling(AllocatorPolymorphic allocator, PhysicalMemoryProfiler* profiler);

}