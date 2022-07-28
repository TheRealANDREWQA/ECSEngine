#include "ecspch.h"
#include "ArchetypeQueryCache.h"
#include "EntityManager.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

#define ARENA_BLOCK_COUNT (2048)

#define EXCLUDE_HANDLE_OFFSET 0x80000000

#define RESIZE_FACTOR (1.5f)

	// -----------------------------------------------------------------------------------------------

	ArchetypeQueryCache::ArchetypeQueryCache(AllocatorPolymorphic allocator, unsigned int initial_capacity) : allocator(allocator)
	{
		// Resize the non exclude with the value given and the exclude with a quarter
		if (initial_capacity > 0) {
			Resize(initial_capacity);
			ResizeExclude(initial_capacity / 4);
		}
	}

	// -----------------------------------------------------------------------------------------------

	unsigned int ArchetypeQueryCache::AddQuery(ArchetypeQuery query)
	{
		// Just lock, add or resize and unlock
		query_results.lock.lock();

		if (query_results.count == query_results.capacity) {
			Resize(RESIZE_FACTOR * query_results.capacity);
		}
		unsigned int index = query_results.count;
		query_results.components[index] = query;

		ECS_STACK_CAPACITY_STREAM(unsigned short, results, ECS_KB * 8);
		entity_manager->GetArchetypes(query, results);
		// Allocate a new chunk
		query_results.results->InitializeAndCopy(allocator, results);
		query_results.count++;

		query_results.lock.unlock();

		// Can return the index as is, only the exclude one will be offseted
		return index;
	}

	// -----------------------------------------------------------------------------------------------

	unsigned int ArchetypeQueryCache::AddQuery(ArchetypeQueryExclude query)
	{
		// Just lock, add or resize and unlock
		exclude_query_results.lock.lock();

		if (exclude_query_results.count == exclude_query_results.capacity) {
			ResizeExclude(RESIZE_FACTOR * exclude_query_results.capacity);
		}
		unsigned int index = exclude_query_results.count;
		exclude_query_results.components[index] = query;

		ECS_STACK_CAPACITY_STREAM(unsigned short, results, ECS_KB * 8);
		entity_manager->GetArchetypesExclude(query, results);
		// Allocate a new chunk
		exclude_query_results.results->InitializeAndCopy(allocator, results);
		exclude_query_results.count++;

		exclude_query_results.lock.unlock();
		return index + EXCLUDE_HANDLE_OFFSET;
	}

	// -----------------------------------------------------------------------------------------------

	Stream<unsigned short> ArchetypeQueryCache::GetResults(unsigned int handle) const
	{
		// This stream is used by the crash functions to "return" a value.
		Stream<unsigned short> result = { nullptr, 0 };
		ECS_CRASH_RETURN_VALUE(handle != -1, result, "Handle is empty for archetype query cache.");

		if (handle >= EXCLUDE_HANDLE_OFFSET) {
			// Check the exclude results
			handle -= EXCLUDE_HANDLE_OFFSET;
			ECS_CRASH_RETURN_VALUE(
				handle > exclude_query_results.count, 
				result, 
				"Invalid handle for exclude query. Requested index {#} when count is {#}.", 
				handle,
				exclude_query_results.count
			);

			return exclude_query_results.results[handle];
		}
		else {
			ECS_CRASH_RETURN_VALUE(
				handle > query_results.count,
				result,
				"Invalid handle for normal query. Requested index {#} when count is {#}.",
				handle,
				query_results.count
			);

			return query_results.results[handle];
		}
	}

	// -----------------------------------------------------------------------------------------------

	AllocatorPolymorphic ArchetypeQueryCache::DefaultAllocator(GlobalMemoryManager* initial_allocator)
	{
		// Get an allocator for about 512 queries
		return DetermineAllocator(initial_allocator, 512);
	}

	// -----------------------------------------------------------------------------------------------

	AllocatorPolymorphic ArchetypeQueryCache::DetermineAllocator(GlobalMemoryManager* initial_allocator, unsigned int total_query_count)
	{
		// Use a memory manager as the type of the allocator

		// Give a rough estimate of a median number of entries per query shorts
		// and use it for the initial allocation
		size_t total_size = total_query_count * 64 * sizeof(unsigned short);
		total_size += (sizeof(Stream<unsigned short>) * 2 + sizeof(ArchetypeQuery) + sizeof(ArchetypeQueryExclude)) * total_query_count;

		MemoryManager* arena = (MemoryManager*)initial_allocator->Allocate(sizeof(MemoryManager));
		*arena = MemoryManager(total_size, ARENA_BLOCK_COUNT, total_size, initial_allocator);

		return GetAllocatorPolymorphic(arena);
	}

	// -----------------------------------------------------------------------------------------------

	void ArchetypeQueryCache::Resize(unsigned int new_capacity)
	{
		Stream<unsigned short>* old_results = query_results.results;
		ArchetypeQuery* old_components = query_results.components;

		size_t allocation_size = (sizeof(Stream<unsigned short>) + sizeof(ArchetypeQuery)) * new_capacity;
		void* allocation = Allocate(allocator, allocation_size);

		query_results.results = (Stream<unsigned short>*)allocation;
		allocation = function::OffsetPointer(allocation, sizeof(Stream<unsigned short>) * new_capacity);

		query_results.components = (ArchetypeQuery*)allocation;

		if (query_results.count > 0) {
			memcpy(query_results.results, old_results, sizeof(Stream<unsigned short>) * query_results.count);
			memcpy(query_results.components, old_components, sizeof(ArchetypeQuery) * query_results.count);

			Deallocate(allocator, old_results);
		}

		query_results.capacity = new_capacity;
	}

	// -----------------------------------------------------------------------------------------------

	void ArchetypeQueryCache::ResizeExclude(unsigned int new_capacity)
	{
		Stream<unsigned short>* old_results = exclude_query_results.results;
		ArchetypeQueryExclude* old_components = exclude_query_results.components;

		size_t allocation_size = (sizeof(Stream<unsigned short>) + sizeof(ArchetypeQueryExclude)) * new_capacity;
		void* allocation = Allocate(allocator, allocation_size);

		exclude_query_results.results = (Stream<unsigned short>*)allocation;
		allocation = function::OffsetPointer(allocation, sizeof(Stream<unsigned short>) * new_capacity);

		exclude_query_results.components = (ArchetypeQueryExclude*)allocation;

		if (exclude_query_results.count > 0) {
			memcpy(exclude_query_results.results, old_results, sizeof(Stream<unsigned short>) * exclude_query_results.count);
			memcpy(exclude_query_results.components, old_components, sizeof(ArchetypeQuery) * exclude_query_results.count);

			Deallocate(allocator, old_results);
		}

		exclude_query_results.capacity = new_capacity;
	}

	// -----------------------------------------------------------------------------------------------

	void ArchetypeQueryCache::UpdateAdd(unsigned short new_archetype_index)
	{
		// Get the query
		ArchetypeQuery query(entity_manager->GetArchetypeUniqueComponents(new_archetype_index), entity_manager->GetArchetypeSharedComponents(new_archetype_index));

		const size_t STACK_CAPACITY = ECS_KB * 8;
		ECS_STACK_CAPACITY_STREAM(unsigned short, temporary_values, STACK_CAPACITY);

		auto allocate_new_value = [&](Stream<unsigned short>& results) {
			// Add it to the stream
			// Copy first into a stack stream in order to deallocate first and then allocate
			// It will help with fragmentation
			ECS_ASSERT(results.size < STACK_CAPACITY);

			temporary_values.Copy(results);
			Deallocate(allocator, results.buffer);

			results.Initialize(allocator, results.size + 1);
			results.Copy(temporary_values);
			results.Add(new_archetype_index);
		};

		for (unsigned int index = 0; index < query_results.count; index++) {
			if (query_results.components[index].Verifies(query.unique, query.shared)) {
				allocate_new_value(query_results.results[index]);
			}
		}

		for (unsigned int index = 0; index < exclude_query_results.count; index++) {
			if (exclude_query_results.components[index].Verifies(query.unique, query.shared)) {
				allocate_new_value(exclude_query_results.results[index]);
			}
		}
	}

	// -----------------------------------------------------------------------------------------------

	void ArchetypeQueryCache::UpdateRemove(unsigned short archetype_index)
	{
		// Can use a simd search for this
		const size_t STACK_CAPACITY = ECS_KB * 8;
		ECS_STACK_CAPACITY_STREAM(unsigned short, temporary_values, STACK_CAPACITY);

		auto loop_iteration = [&](Stream<unsigned short>& values) {
			size_t result_index = function::SearchBytes(values.buffer, values.size, &archetype_index, sizeof(unsigned short));
			if (result_index != -1) {
				values.RemoveSwapBack(result_index);

				// Reduce fragmentation by first deallocating and then allocating
				temporary_values.Copy(values);
				Deallocate(allocator, values.buffer);

				values.Initialize(allocator, values.size);
				values.Copy(temporary_values);
			}
		};

		for (unsigned int index = 0; index < query_results.count; index++) {
			loop_iteration(query_results.results[index]);
		}

		for (unsigned int index = 0; index < exclude_query_results.count; index++) {
			loop_iteration(exclude_query_results.results[index]);
		}
	}

	// -----------------------------------------------------------------------------------------------

	void ArchetypeQueryCache::Update(Stream<unsigned short> new_archetypes, Stream<unsigned short> remove_archetypes)
	{
		// The checks can be coallesced
		ECS_STACK_CAPACITY_STREAM(unsigned short, new_additions_for_query, ECS_KB);

		const size_t STACK_CAPACITY = ECS_KB * 8;
		ECS_STACK_CAPACITY_STREAM(unsigned short, temporary_values, STACK_CAPACITY);

		auto loop = [&](auto& query_results) {
			for (unsigned int index = 0; index < query_results.count; index++) {
				new_additions_for_query.size = 0;

				// First do the removal of old archetypes. This should be done one by one because otherwise
				// the indices become invalidated when RemoveSwapBack happens
				for (size_t removal_index = 0; removal_index < remove_archetypes.size; removal_index++) {
					size_t result_index = function::SearchBytes(
						query_results.results[index].buffer,
						query_results.results[index].size,
						remove_archetypes.buffer + removal_index,
						sizeof(unsigned short)
					);
					if (result_index != -1) {
						query_results.results[index].RemoveSwapBack(result_index);
					}
				}

				// If removals were done but no additions were made, it is fine. There will be a bit of memory
				// waste but whenever a new addition will be done then that memory will be freed appropriately

				// Determine which new_archetypes match and which old need to be removed
				for (size_t new_archetype_index = 0; new_archetype_index < new_archetypes.size; new_archetype_index++) {
					ArchetypeQuery query(
						entity_manager->GetArchetypeUniqueComponents(new_archetypes[new_archetype_index]),
						entity_manager->GetArchetypeSharedComponents(new_archetypes[new_archetype_index])
					);

					if (query_results.components[index].Verifies(query.unique, query.shared)) {
						new_additions_for_query.Add(new_archetypes[new_archetype_index]);
					}
				}

				// Now commit all of them at once
				if (new_additions_for_query.size > 0) {
					temporary_values.Copy(query_results.results[index]);
					Deallocate(allocator, query_results.results[index].buffer);

					query_results.results[index].Initialize(allocator, query_results.results[index].size + new_additions_for_query.size);
					query_results.results[index].Copy(temporary_values);

					for (size_t index = 0; index < new_additions_for_query.size; index++) {
						query_results.results[index].Add(new_additions_for_query[index]);
					}
				}
			}
		};
		
		loop(query_results);
		loop(exclude_query_results);
	}

	// -----------------------------------------------------------------------------------------------

}