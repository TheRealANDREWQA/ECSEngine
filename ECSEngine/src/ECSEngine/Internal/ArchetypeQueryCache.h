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
		ArchetypeQueryCache(AllocatorPolymorphic allocator, unsigned int initial_capacity = 0);

		// Returns a handle that will be used for the lifetime of the scene
		// to be used to access the results. Needs to be stored.
		// Thread safe
		unsigned int AddQuery(ArchetypeQuery query);

		// Returns a handle that will be used for the lifetime of the scene
		// to be used to access the results. Needs to be stored.
		// Thread safe
		unsigned int AddQuery(ArchetypeQueryExclude query);

		// It asserts that the handle is valid
		Stream<unsigned short> GetResults(unsigned int handle) const;

		// Resizes the non exclude results to the new capacity (the SoA structure)
		void Resize(unsigned int new_capacity);

		// Resizes the exclude results to the new capacity (the SoA structure)
		void ResizeExclude(unsigned int new_capacity);

		void UpdateAdd(unsigned short new_archetype_index);

		void UpdateRemove(unsigned short archetype_index);

		void Update(Stream<unsigned short> new_archetypes, Stream<unsigned short> remove_archetypes);

		// Returns an appropriately sized allocator for a default use case.
		// It is the same as calling DetermineAllocator with a size of 512
		static AllocatorPolymorphic DefaultAllocator(GlobalMemoryManager* initial_allocator);

		// Can help in making a bigger or g
		static AllocatorPolymorphic DetermineAllocator(GlobalMemoryManager* initial_allocator, unsigned int total_query_count);

		// SoA split of a resizable stream in order
		// to have better cache utilization when updating the query_results
		struct QueryResults {
			Stream<unsigned short>* results;
			ArchetypeQuery* components;
			unsigned int count;
			unsigned int capacity;
			SpinLock lock;
		};

		struct ExcludeQueryResults {
			Stream<unsigned short>* results;
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