#pragma once
#include "../Core.h"
#include "InternalStructures.h"
#include "VectorComponentSignature.h"

namespace ECSEngine {
	
	struct EntityManager;

	// Keeps two separate resizable streams in an SoA fashion. One for queries which do not have exclude
	// requirements and one for those that need exclude components. In this way the memory bandwidth is 
	// reduced for the vast majority which do not require exclude.
	// It uses a spin lock for synchronizing between multiple threads. It shouldn't be that necessary tho
	struct ECSENGINE_API ArchetypeQueryCache {
		ArchetypeQueryCache() = default;
		ArchetypeQueryCache(EntityManager* entity_manager, AllocatorPolymorphic allocator, unsigned int initial_capacity = 0);

		// Thread safe
		// Returns a handle that will be used for the lifetime of the scene
		// to be used to access the results. Needs to be stored. It already retrieves all archetypes that match
		// the given query.
		unsigned int AddQuery(ArchetypeQuery query);

		// Thread safe
		// Returns a handle that will be used for the lifetime of the scene
		// to be used to access the results. Needs to be stored. It already retrieves all archetypes that match
		// the given query.
		unsigned int AddQuery(ArchetypeQueryExclude query);

		void CopyOther(const ArchetypeQueryCache* other);

		// It asserts that the handle is valid
		Stream<unsigned int> GetResults(unsigned int handle) const;

		// Returns the components stored for that handle
		ArchetypeQuery ECS_VECTORCALL GetComponents(unsigned int handle) const;

		ArchetypeQueryResult ECS_VECTORCALL GetResultsAndComponents(unsigned int handle) const;

		void Reset();

		// Resizes the non exclude results to the new capacity (the SoA structure)
		void Resize(unsigned int new_capacity);

		// Resizes the exclude results to the new capacity (the SoA structure)
		void ResizeExclude(unsigned int new_capacity);

		void UpdateAdd(unsigned int new_archetype_index);

		void UpdateRemove(unsigned int archetype_index);

		void Update(Stream<unsigned int> new_archetypes, Stream<unsigned int> remove_archetypes);

		// Returns an appropriately sized allocator for a default use case.
		// It is the same as calling DetermineAllocator with a size of 512
		static MemoryManager DefaultAllocator(GlobalMemoryManager* initial_allocator);

		// The total query count will adjust the allocator such that it will be the most suitable
		static MemoryManager DetermineAllocator(GlobalMemoryManager* initial_allocator, unsigned int total_query_count);

		// SoA split of a resizable stream in order
		// to have better cache utilization when updating the query_results
		struct QueryResults {
			Stream<unsigned int>* results;
			ArchetypeQuery* components;
			unsigned int count;
			unsigned int capacity;
			SpinLock lock;
		};

		struct ExcludeQueryResults {
			Stream<unsigned int>* results;
			ArchetypeQueryExclude* components;
			unsigned int count;
			unsigned int capacity;
			SpinLock lock;
		};

		EntityManager* entity_manager;
		AllocatorPolymorphic allocator;

		QueryResults query_results;
		ExcludeQueryResults exclude_query_results;
	};

}