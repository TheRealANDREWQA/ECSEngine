#include "ecspch.h"
#include "ForEach.h"
#include "World.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

	struct ForEachEntityBatchImplementationTaskData {
		ushort2 archetype_indices;
		unsigned char component_map[ECS_ARCHETYPE_MAX_COMPONENTS];
		unsigned char shared_component_map[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		unsigned char component_map_count;
		unsigned char shared_component_map_count;
		EntityManagerCommandStream* command_stream;

		unsigned int entity_offset;
		unsigned int count;

		void* functor_data;
		void* thread_function;
	};

	void ForEachEntityBatchImplementation(
		unsigned int thread_id,
		World* world,
		void* thread_function, 
		const char* functor_name,
		void* data, 
		size_t data_size, 
		unsigned int deferred_calls_capacity,
		ThreadFunction task_function
	) {
		const TaskSchedulerInfo* scheduler_info = world->task_scheduler->GetCurrentQueryInfo();
		unsigned int batch_size = scheduler_info->batch_size;
		
		Stream<unsigned int> archetypes;
		ArchetypeQuery query_components;

		world->entity_manager->GetQueryResultsAndComponents(scheduler_info->query_handle, archetypes, query_components);
		if (archetypes.size > 0) {
			if (data_size > 0) {
				void* allocation = world->task_manager->AllocateTempBuffer(thread_id, data_size);
				memcpy(allocation, data, data_size);
				data = allocation;
			}

			ForEachEntityBatchImplementationTaskData task_data;
			task_data.thread_function = thread_function;
			task_data.functor_data = data;

			size_t deferred_call_allocation_size = sizeof(DeferredAction) * deferred_calls_capacity + sizeof(EntityManagerCommandStream);

			for (size_t index = 0; index < archetypes.size; index++) {
				Archetype* archetype = world->entity_manager->GetArchetype(archetypes[index]);
				unsigned int base_count = archetype->GetBaseCount();

				// Get the component and shared component map
				world->entity_manager->FindArchetypeUniqueComponentVector(archetypes[index], query_components.unique, task_data.component_map);
				world->entity_manager->FindArchetypeSharedComponentVector(archetypes[index], query_components.shared, task_data.shared_component_map);
				task_data.archetype_indices.x = archetypes[index];
				task_data.component_map_count = query_components.unique.Count();
				task_data.shared_component_map_count = query_components.shared.Count();

				// Check that there is no component index with -1
				// If it happens, then there is a memory corruption
				for (unsigned char component_index = 0; component_index < task_data.component_map_count; component_index++) {
					ECS_CRASH_RETURN(
						task_data.component_map[component_index] != -1,
						"An archetype was corrupted. During query {#} execution a component was not found.",
						functor_name
					);
				}

				for (unsigned char shared_component_index = 0; shared_component_index < task_data.shared_component_map_count; shared_component_index++) {
					ECS_CRASH_RETURN(
						task_data.shared_component_map[shared_component_index] != -1,
						"An archetype was corrupted. During query {#} execution a shared component was not found.",
						functor_name
					);
				}

				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_index);
					unsigned int entity_count = base->EntityCount();
					task_data.archetype_indices.y = base_index;

					EntityManagerCommandStream* command_stream = nullptr;

					// Coallesce the remainder and the last batch into a single batch
					unsigned int batch_count = entity_count / batch_size;
					if (batch_count > 1) {
						task_data.count = batch_size;
						// Hoist outside the loop the entity manager command stream check
						if (deferred_calls_capacity > 0) {
							for (unsigned int batch_index = 0; batch_index < batch_count - 1; batch_index++) {
								command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);

								command_stream->InitializeFromBuffer(function::OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_calls_capacity);
								task_data.command_stream = command_stream;

								task_data.entity_offset = batch_index * batch_size;
								world->task_manager->AddDynamicTask({ task_function, &task_data, sizeof(task_data), functor_name });
							}
						}
						else {
							for (unsigned int batch_index = 0; batch_index < batch_count - 1; batch_index++) {
								task_data.entity_offset = batch_index * batch_size;
								task_data.command_stream = nullptr;
								world->task_manager->AddDynamicTask({ task_function, &task_data, sizeof(task_data), functor_name });
							}
						}

						// For the last one do the coallescing if necessary
						unsigned int last_batch_start = (batch_count - 1) * batch_size;
						unsigned int remainder = entity_count - last_batch_start;
						task_data.entity_offset = last_batch_start;
						task_data.count = remainder;
						if (deferred_calls_capacity > 0) {
							command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);

							command_stream->InitializeFromBuffer(function::OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_calls_capacity);
							task_data.command_stream = command_stream;
						}
						else {
							task_data.command_stream = nullptr;
						}

						world->task_manager->AddDynamicTask({ task_function, &task_data, sizeof(task_data), functor_name });
					}
					else {
						// Single batch here
						task_data.entity_offset = 0;
						task_data.count = entity_count;
						if (deferred_calls_capacity > 0) {
							command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);

							command_stream->InitializeFromBuffer(function::OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_calls_capacity);
							task_data.command_stream = command_stream;
						}
						else {
							task_data.command_stream = nullptr;
						}

						world->task_manager->AddDynamicTask({ task_function, &task_data, sizeof(task_data), functor_name });
					}
				}
			}
		}
		// After launching all tasks, if any, increment the query index for the scheduler
		world->task_scheduler->IncrementQueryIndex();
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	// Extracts the pointers for the unique and shared buffers alongside the entity buffer
	Entity* ForEachEntityThreadTaskHelper(
		ForEachEntityBatchImplementationTaskData* data, 
		World* world,
		void** functor_uniques, 
		void** functor_shareds,
		unsigned short* component_sizes
	) {
		const Archetype* archetype = world->entity_manager->GetArchetype(data->archetype_indices.x);
		ArchetypeBase* base = world->entity_manager->GetBase(data->archetype_indices.x, data->archetype_indices.y);
		Entity* entities = base->m_entities;

		ComponentSignature archetype_signature = archetype->GetUniqueSignature();
		for (unsigned char component_index = 0; component_index < data->component_map_count; component_index++) {
			functor_uniques[component_index] = base->GetComponentByIndex(data->entity_offset, data->component_map[component_index]);
			component_sizes[component_index] = world->entity_manager->ComponentSize(archetype_signature.indices[component_index]);
		}

		const Component* shared_components = archetype->GetSharedSignature().indices;
		const SharedInstance* shared_instances = archetype->GetBaseInstances(data->archetype_indices.y);
		for (unsigned char shared_component_index = 0; shared_component_index < data->shared_component_map_count; shared_component_index++) {
			functor_shareds[shared_component_index] = world->entity_manager->GetSharedData(
				shared_components[data->shared_component_map[shared_component_index]],
				shared_instances[data->shared_component_map[shared_component_index]]
			);
		}

		return entities;
	}

	ECS_THREAD_TASK(ForEachEntityThreadTask) {
		ForEachEntityBatchImplementationTaskData* data = (ForEachEntityBatchImplementationTaskData*)_data;

		void* functor_uniques[ECS_ARCHETYPE_MAX_COMPONENTS];
		void* functor_shareds[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		unsigned short component_sizes[ECS_ARCHETYPE_MAX_COMPONENTS];
		const Entity* entities = ForEachEntityThreadTaskHelper(data, world, functor_uniques, functor_shareds, component_sizes);

		for (unsigned short index = 0; index < data->count; index++) {
			ForEachEntityFunctor functor = (ForEachEntityFunctor)data->thread_function;
			functor(
				thread_id,
				world,
				entities[data->entity_offset + index],
				functor_uniques,
				functor_shareds,
				data->functor_data,
				data->command_stream
			);

			// Offset the functor uniques now
			for (unsigned char component_index = 0; component_index < data->component_map_count; component_index++) {
				functor_uniques[component_index] = function::OffsetPointer(functor_uniques[component_index], component_sizes[component_index]);
			}
		}
	}

	void ForEachEntity(
		unsigned int thread_id,
		World* world, 
		ForEachEntityFunctor functor,
		const char* functor_name,
		void* data, 
		size_t data_size,
		unsigned int deferred_calls_capacity
	) {
		ForEachEntityBatchImplementation(thread_id, world, functor, functor_name, data, data_size, deferred_calls_capacity, ForEachEntityThreadTask);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ForEachBatchThreadTask) {
		ForEachEntityBatchImplementationTaskData* data = (ForEachEntityBatchImplementationTaskData*)_data;

		void* functor_uniques[ECS_ARCHETYPE_MAX_COMPONENTS];
		void* functor_shareds[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		unsigned short component_sizes[ECS_ARCHETYPE_MAX_COMPONENTS];
		Entity* entities = ForEachEntityThreadTaskHelper(data, world, functor_uniques, functor_shareds, component_sizes);

		ForEachBatchFunctor functor = (ForEachBatchFunctor)data->thread_function;
		functor(
			thread_id,
			world,
			entities,
			data->count,
			functor_uniques,
			functor_shareds,
			data->functor_data,
			data->command_stream
		);
	}

	void ForEachBatch(
		unsigned int thread_id, 
		World* world, 
		ForEachBatchFunctor functor,
		const char* functor_name,
		void* data, 
		size_t data_size, 
		unsigned int deferred_calls_capacity
	)
	{
		ForEachEntityBatchImplementation(thread_id, world, functor, functor_name, data, data_size, deferred_calls_capacity, ForEachBatchThreadTask);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	//template<typename Query>
	//void ECS_VECTORCALL ForEachBaseArchetypeImplementation(EntityManager* entity_manager, Query query, void* data, ForEachBaseArchetypeFunctor functor) {
	//	// TODO: Determine if the get archetypes call is more efficient than simply iterating and calling the functor 
	//	// in place. Doing the archetype search first will help in using a very tight SIMD loop without I-Cache thrashing
	//	// and probably the micro ops cache can help with that too

	//	// Use the get archetypes call. 
	//	unsigned short* _matched_archetypes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * entity_manager->m_archetypes.size);
	//	CapacityStream<unsigned short> matched_archetypes(_matched_archetypes, 0, entity_manager->m_archetypes.size);

	//	if constexpr (std::is_same_v<Query, ArchetypeQuery>) {
	//		entity_manager->GetArchetypes(query, matched_archetypes);
	//	}
	//	else if constexpr (std::is_same_v<Query, ArchetypeQueryExclude>) {
	//		entity_manager->GetArchetypesExclude(query, matched_archetypes);
	//	}

	//	// Get the count of unique and shared components
	//	unsigned char unique_count = query.unique.Count();
	//	unsigned char shared_count = query.shared.Count();

	//	// Call the functor for each entity now
	//	for (size_t index = 0; index < matched_archetypes.size; index++) {
	//		// Get the indices of the requested components
	//		unsigned char unique_component_indices[ECS_ARCHETYPE_MAX_COMPONENTS];
	//		unsigned char shared_component_indices[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

	//		entity_manager->FindArchetypeUniqueComponentVector(matched_archetypes[index], query.unique, unique_component_indices);
	//		entity_manager->FindArchetypeSharedComponentVector(matched_archetypes[index], query.shared, shared_component_indices);

	//		void* unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
	//		void* shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

	//		Archetype* archetype = entity_manager->GetArchetype(matched_archetypes[index]);
	//		ComponentSignature archetype_shared_signature = archetype->GetSharedSignature();

	//		// Iterate through the base archetypes now
	//		for (size_t base_index = 0; base_index < archetype->m_base_archetypes.size; base_index++) {
	//			ArchetypeBase* base = archetype->GetBase(base_index);
	//			const SharedInstance* base_instances = archetype->GetBaseInstances(base_index);

	//			// Get the shared components
	//			SharedInstance shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
	//			for (size_t shared_index = 0; shared_index < shared_count; shared_index++) {
	//				shared_instances[shared_index] = base_instances[shared_component_indices[shared_index]];
	//				shared_components[shared_index] = entity_manager->GetSharedData(
	//					archetype_shared_signature.indices[shared_component_indices[shared_index]],
	//					shared_instances[shared_index]
	//				);
	//			}

	//			for (size_t unique_index = 0; unique_index < unique_count; unique_index++) {
	//				unique_components[unique_index] = base->m_buffers[unique_component_indices[unique_index]];
	//			}

	//			// Call the functor now
	//			functor(base, unique_components, shared_components, data);
	//		}
	//	}
	//}

	//void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQuery query, void* data, ForEachBaseArchetypeFunctor functor) {
	//	ForEachBaseArchetypeImplementation(entity_manager, query, data, functor);
	//}

	//void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQueryExclude query, void* data, ForEachBaseArchetypeFunctor functor) {
	//	ForEachBaseArchetypeImplementation(entity_manager, query, data, functor);
	//}

	//// -------------------------------------------------------------------------------------------------------------------------------

	//void ForEachSharedInstance(EntityManager* entity_manager, SharedComponentSignature shared_signature, void* data, ForEachSharedInstanceFunctor functor) {
	//	// Use the get archetypes call. 
	//	unsigned short* _matched_archetypes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * entity_manager->m_archetypes.size);
	//	CapacityStream<unsigned short> matched_archetypes(_matched_archetypes, 0, entity_manager->m_archetypes.size);

	//	VectorComponentSignature vector_component;
	//	VectorComponentSignature vector_instances;
	//	vector_component.ConvertComponents({ shared_signature.indices, shared_signature.count });
	//	vector_instances.ConvertInstances(shared_signature.instances, shared_signature.count);

	//	for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
	//		if (entity_manager->GetArchetypeSharedComponents(index).HasComponents(vector_component)) {
	//			matched_archetypes.AddSafe(index);
	//		}
	//	}

	//	// For every archetype that matched the components, walk through the base archetypes now
	//	for (size_t index = 0; index < matched_archetypes.size; index++) {
	//		const Archetype* archetype = entity_manager->GetArchetype(matched_archetypes[index]);
	//		unsigned char shared_component_indices[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
	//		VectorComponentSignature archetype_components = entity_manager->GetArchetypeSharedComponents(matched_archetypes[index]);

	//		size_t base_count = archetype->GetBaseCount();
	//		for (size_t base_index = 0; base_index < base_count; base_index++) {
	//			VectorComponentSignature archetype_instances = archetype->GetVectorInstances(base_index);

	//			if (SharedComponentSignatureHasInstances(archetype_components, archetype_instances, vector_component, vector_instances)) {
	//				functor(matched_archetypes[index], base_index, data);
	//			}
	//		}
	//	}
	//}

	// -------------------------------------------------------------------------------------------------------------------------------

}