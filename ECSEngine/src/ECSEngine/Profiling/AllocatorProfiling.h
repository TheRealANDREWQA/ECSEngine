#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "Statistic.h"
#include "AllocatorProfilingGlobal.h"

namespace ECSEngine {

	// Returns an estimate size needed for the allocator given to this function
	ECSENGINE_API size_t AllocatorProfilingEstimateAllocatorSize(unsigned int thread_count, unsigned int entry_capacity);

	struct ECSENGINE_API AllocatorProfiling {
		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return allocator;
		}

		// Use allocator_type custom in case you have a custom allocator with a custom function for the usage
		// Returns true if it already exists, else false. It it exists, it will simply concatenate the name
		bool AddEntry(
			void* address, 
			ECS_ALLOCATOR_TYPE allocator_type, 
			Stream<char> name, 
			AllocatorProfilingCustomAllocatorUsage custom_usage_function = nullptr,
			AllocatorProfilingCustomExitFunction custom_exit_function = nullptr
		);

		// We need the current usage to record the peak usage for the memory and
		// Optionally the peak block count for allocators like multipool or based on it
		void AddAllocation(const void* address, size_t current_usage, size_t block_count = 0, unsigned char suballocator_count = 0);

		void AddDeallocation(const void* address);

		// This will clear everything
		// And remove all allocators (and exit them from the profiling mode)
		void Clear();

		// This will exit the allocators from the profiling mode only - but it will
		// Keep the statistics data
		void ExitAllocators();

		void EndFrame();

		// Returns -1 if it doesn't find it
		size_t Find(const void* address) const;

		void Initialize(AllocatorPolymorphic allocator, unsigned int entry_capacity);

		void RemoveEntry(const void* address);

		void StartFrame();

		struct ECSENGINE_API EntryData {
			static size_t MemoryOf(unsigned int entry_count);

			ECS_INLINE EntryData& operator = (const EntryData& other) {
				memcpy(this, &other, sizeof(other));
				return *this;
			}

			AllocatorProfilingCustomAllocatorUsage custom_usage;
			AllocatorProfilingCustomExitFunction custom_exit;
			// We keep this value separate since this is updated
			// Per allocation such that we can truly catch the maximum
			// Usage. For example, for linear allocators this is critical
			// To determine how much memory they use
			size_t peak_memory_usage;
			// This is needed only for multipool based allocators
			size_t peak_block_count;
			// This is needed only for resizable allocators to determine
			// If the guestimates were accurate
			unsigned char peak_suballocator_count;
			ECS_ALLOCATOR_TYPE allocator_type;
			// This lock is used for updating the values when adding a new allocation
			SpinLock lock;
			unsigned int current_frame_allocations;
			std::atomic<unsigned int> current_frame_deallocations;

			// All these values are per frame
			Statistic<unsigned int> allocations;
			Statistic<unsigned int> deallocations;
			Statistic<size_t> current_usage;

			Stream<char> name;
		};

		AllocatorPolymorphic allocator;
		void** addresses;
		EntryData* entry_data;
		unsigned int address_size;
		unsigned int address_capacity;
		unsigned int entry_data_capacity;
	};

}