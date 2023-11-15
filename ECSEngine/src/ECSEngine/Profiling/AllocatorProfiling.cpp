#include "ecspch.h"
#include "AllocatorProfiling.h"
#include "../Containers/SoA.h"

namespace ECSEngine {

	size_t AllocatorProfilingEstimateAllocatorSize(unsigned int thread_count, unsigned int entry_capacity)
	{
		// Estimate that each thread will have 1 allocator and there will be another 10 standalone ones
		// Add some small number of kilobytes for the names and the streams needed
		return ((size_t)thread_count + 10) * AllocatorProfiling::EntryData::MemoryOf(entry_capacity) + ECS_KB * 32;
	}

	size_t AllocatorProfiling::EntryData::MemoryOf(unsigned int entry_count)
	{
		return Statistic<unsigned int>::MemoryOf(entry_count) * 2 + Statistic<size_t>::MemoryOf(entry_count);
	}

	void AllocatorProfiling::AddEntry(const void* address, ECS_ALLOCATOR_TYPE allocator_type, Stream<char> name, AllocatorProfilingCustomAllocatorUsage custom_usage)
	{
		SoAResizeIfFull(allocator, address_size, address_capacity, &addresses, &entry_data);
		unsigned int index = address_size;
		addresses[index] = address;
		entry_data[index].name = name.Copy(allocator);
		entry_data[index].allocator_type = allocator_type;
		entry_data[index].peak_memory_usage = 0;
		entry_data[index].peak_block_count = 0;
		entry_data[index].peak_suballocator_count = 0;
		entry_data[index].custom_usage = custom_usage;
		entry_data[index].allocations.Initialize(allocator, entry_data_capacity);
		entry_data[index].deallocations.Initialize(allocator, entry_data_capacity);
		entry_data[index].current_usage.Initialize(allocator, entry_data_capacity);

		address_size++;
	}

	void AllocatorProfiling::AddAllocation(const void* address, size_t current_usage, size_t block_count, unsigned char suballocator_count)
	{
		size_t index = Find(address);
		// Here don't use an assert since we are sure that this function will be called
		// Only when it is truly existent
		entry_data[index].peak_memory_usage = std::max(entry_data[index].peak_memory_usage, current_usage);
		entry_data[index].peak_block_count = std::max(entry_data[index].peak_block_count, block_count);
		entry_data[index].peak_suballocator_count = std::max(entry_data[index].peak_suballocator_count, suballocator_count);
	}

	void AllocatorProfiling::AddDeallocation(const void* address)
	{
	}

	void AllocatorProfiling::Clear()
	{
		for (unsigned int index = 0; index < address_size; index++) {
			entry_data[index].name.Deallocate(allocator);
			entry_data[index].current_usage.Deallocate(allocator);
			entry_data[index].allocations.Deallocate(allocator);
			entry_data[index].deallocations.Deallocate(allocator);
		}
		// Don't deallocate the SoA buffer, just reset it
		address_size = 0;
	}

	void AllocatorProfiling::EndFrame()
	{
		for (unsigned int index = 0; index < address_size; index++) {
			// If it has a custom function for usage, use it
			size_t current_usage = 0;
			if (entry_data[index].custom_usage != nullptr) {
				current_usage = entry_data[index].custom_usage(addresses[index]);
			}
			else {

			}
		}
	}

	size_t AllocatorProfiling::Find(const void* address) const
	{
		return SearchBytes(addresses, address_size, (size_t)address, sizeof(address));
	}

	void AllocatorProfiling::Initialize(AllocatorPolymorphic _allocator, unsigned int _entry_capacity)
	{
		const size_t INITIAL_COUNT = 8;

		allocator = _allocator;
		entry_data_capacity = _entry_capacity;
		// Initialize with a small count
		SoAInitialize(allocator, INITIAL_COUNT, &addresses, &entry_data);
		address_size = 0;
		address_capacity = INITIAL_COUNT;
	}

	void AllocatorProfiling::RemoveEntry(const void* address)
	{
		size_t index = Find(address);
		ECS_ASSERT(index != -1, "Trying to remove allocator profiling address that was not previously added");
		SoARemoveSwapBack(address_size, index, addresses, entry_data);
	}

	void AllocatorProfiling::StartFrame()
	{
		// Zero out the frame allocation/deallocation counts
		for (unsigned int index = 0; index < address_size; index++) {
			entry_data[index].current_frame_allocations = 0;
			entry_data[index].current_frame_deallocations = 0;
		}
	}

}