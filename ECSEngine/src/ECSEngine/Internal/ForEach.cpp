#include "ecspch.h"
#include "ForEach.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	void ECS_VECTORCALL ForEachEntityImplementation(EntityManager* entity_manager, Query query, void* data, ForEachEntityFunctor functor) {
		// TODO: Determine if the get archetypes call is more efficient than simply iterating and calling the functor 
		// in place. Doing the archetype search first will help in using a very tight SIMD loop without I-Cache thrashing
		// and probably the micro ops cache can help with that too

		// Use the get archetypes call. 
		unsigned short* _matched_archetypes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * entity_manager->m_archetypes.size);
		CapacityStream<unsigned short> matched_archetypes(_matched_archetypes, 0, entity_manager->m_archetypes.size);

		if constexpr (std::is_same_v<Query, ArchetypeQuery>) {
			entity_manager->GetArchetypes(query, matched_archetypes);
		}
		else if constexpr (std::is_same_v<Query, ArchetypeQueryExclude>) {
			entity_manager->GetArchetypesExclude(query, matched_archetypes);
		}

		// Get the count of unique and shared components
		unsigned char unique_count = query.unique.Count();
		unsigned char shared_count = query.shared.Count();

		// Call the functor for each entity now
		for (size_t index = 0; index < matched_archetypes.size; index++) {
			// Get the indices of the requested components
			unsigned char unique_component_indices[ECS_ARCHETYPE_MAX_COMPONENTS];
			unsigned char shared_component_indices[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

			entity_manager->FindArchetypeUniqueComponentVector(matched_archetypes[index], query.unique, unique_component_indices);
			entity_manager->FindArchetypeSharedComponentVector(matched_archetypes[index], query.shared, shared_component_indices);

			void* unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
			void* shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

			const Archetype* archetype = entity_manager->GetArchetype(matched_archetypes[index]);
			ComponentSignature archetype_shared_signature = archetype->GetSharedSignature();
			// Iterate through the base archetypes now
			for (size_t base_index = 0; base_index < archetype->m_base_archetypes.size; base_index++) {
				ArchetypeBase* base = (ArchetypeBase*)archetype->GetBase(base_index);
				const SharedInstance* base_instances = archetype->GetBaseInstances(base_index);

				for (size_t shared_index = 0; shared_index < shared_count; shared_index++) {
					SharedInstance instance = base_instances[shared_component_indices[shared_index]];
					shared_components[shared_index] = entity_manager->GetSharedData(
						archetype_shared_signature.indices[shared_component_indices[shared_index]],
						instance
					);
				}

				// Now every entity
				for (size_t entity_index = 0; entity_index < base->m_size; entity_index++) {
					// Fill in the unique buffers first
					for (size_t unique_index = 0; unique_index < unique_count; unique_index++) {
						unique_components[unique_index] = base->GetComponentByIndex(entity_index, unique_component_indices[unique_index]);
					}

					// Call the functor now
					functor(base->m_entities[entity_index], unique_components, shared_components, data);
				}
			}
		}
	}

	void ECS_VECTORCALL ForEachEntity(EntityManager* entity_manager, ArchetypeQuery query, void* data, ForEachEntityFunctor functor) {
		ForEachEntityImplementation(entity_manager, query, data, functor);
	}

	void ECS_VECTORCALL ForEachEntity(EntityManager* entity_manager, ArchetypeQueryExclude query, void* data, ForEachEntityFunctor functor) {
		ForEachEntityImplementation(entity_manager, query, data, functor);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	void ECS_VECTORCALL ForEachBaseArchetypeImplementation(EntityManager* entity_manager, Query query, void* data, ForEachBaseArchetypeFunctor functor) {
		// TODO: Determine if the get archetypes call is more efficient than simply iterating and calling the functor 
		// in place. Doing the archetype search first will help in using a very tight SIMD loop without I-Cache thrashing
		// and probably the micro ops cache can help with that too

		// Use the get archetypes call. 
		unsigned short* _matched_archetypes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * entity_manager->m_archetypes.size);
		CapacityStream<unsigned short> matched_archetypes(_matched_archetypes, 0, entity_manager->m_archetypes.size);

		if constexpr (std::is_same_v<Query, ArchetypeQuery>) {
			entity_manager->GetArchetypes(query, matched_archetypes);
		}
		else if constexpr (std::is_same_v<Query, ArchetypeQueryExclude>) {
			entity_manager->GetArchetypesExclude(query, matched_archetypes);
		}

		// Get the count of unique and shared components
		unsigned char unique_count = query.unique.Count();
		unsigned char shared_count = query.shared.Count();

		// Call the functor for each entity now
		for (size_t index = 0; index < matched_archetypes.size; index++) {
			// Get the indices of the requested components
			unsigned char unique_component_indices[ECS_ARCHETYPE_MAX_COMPONENTS];
			unsigned char shared_component_indices[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

			entity_manager->FindArchetypeUniqueComponentVector(matched_archetypes[index], query.unique, unique_component_indices);
			entity_manager->FindArchetypeSharedComponentVector(matched_archetypes[index], query.shared, shared_component_indices);

			void* unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
			void* shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

			Archetype* archetype = entity_manager->GetArchetype(matched_archetypes[index]);
			ComponentSignature archetype_shared_signature = archetype->GetSharedSignature();

			// Iterate through the base archetypes now
			for (size_t base_index = 0; base_index < archetype->m_base_archetypes.size; base_index++) {
				ArchetypeBase* base = archetype->GetBase(base_index);
				const SharedInstance* base_instances = archetype->GetBaseInstances(base_index);

				// Get the shared components
				SharedInstance shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
				for (size_t shared_index = 0; shared_index < shared_count; shared_index++) {
					shared_instances[shared_index] = base_instances[shared_component_indices[shared_index]];
					shared_components[shared_index] = entity_manager->GetSharedData(
						archetype_shared_signature.indices[shared_component_indices[shared_index]],
						shared_instances[shared_index]
					);
				}

				for (size_t unique_index = 0; unique_index < unique_count; unique_index++) {
					unique_components[unique_index] = base->m_buffers[unique_component_indices[unique_index]];
				}

				// Call the functor now
				functor(base, unique_components, shared_components, data);
			}
		}
	}

	void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQuery query, void* data, ForEachBaseArchetypeFunctor functor) {
		ForEachBaseArchetypeImplementation(entity_manager, query, data, functor);
	}

	void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQueryExclude query, void* data, ForEachBaseArchetypeFunctor functor) {
		ForEachBaseArchetypeImplementation(entity_manager, query, data, functor);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ForEachSharedInstance(EntityManager* entity_manager, SharedComponentSignature shared_signature, void* data, ForEachSharedInstanceFunctor functor) {
		// Use the get archetypes call. 
		unsigned short* _matched_archetypes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * entity_manager->m_archetypes.size);
		CapacityStream<unsigned short> matched_archetypes(_matched_archetypes, 0, entity_manager->m_archetypes.size);

		VectorComponentSignature vector_component;
		VectorComponentSignature vector_instances;
		vector_component.ConvertComponents({ shared_signature.indices, shared_signature.count });
		vector_instances.ConvertInstances(shared_signature.instances, shared_signature.count);

		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			if (entity_manager->GetArchetypeSharedComponents(index).HasComponents(vector_component)) {
				matched_archetypes.AddSafe(index);
			}
		}

		// For every archetype that matched the components, walk through the base archetypes now
		for (size_t index = 0; index < matched_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(matched_archetypes[index]);
			unsigned char shared_component_indices[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
			VectorComponentSignature archetype_components = entity_manager->GetArchetypeSharedComponents(matched_archetypes[index]);

			size_t base_count = archetype->GetBaseCount();
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				VectorComponentSignature archetype_instances = archetype->GetVectorInstances(base_index);

				if (SharedComponentSignatureHasInstances(archetype_components, archetype_instances, vector_component, vector_instances)) {
					functor(matched_archetypes[index], base_index, data);
				}
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

}