#include "ecspch.h"
#include "ForEach.h"
#include "World.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------------------

	struct ForEachEntityBatchImplementationTaskData {
		uint2 archetype_indices;
		unsigned char component_map[ECS_ARCHETYPE_MAX_COMPONENTS];
		unsigned char shared_component_map[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		unsigned char component_map_count;
		unsigned char shared_component_map_count;
		unsigned char optional_component_map_count;
		unsigned char optional_shared_component_map_count;

		unsigned short component_sizes[ECS_ARCHETYPE_MAX_COMPONENTS];
		unsigned short shared_component_sizes[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

		void* unique_data[ECS_ARCHETYPE_MAX_COMPONENTS];
		void* shared_data[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		const Entity* entities;

		unsigned int entity_offset;
		unsigned int count;

		void* functor_data;
		void* thread_function;
		EntityManagerCommandStream* command_stream;
	};

	// For non-commit usage
	void ForEachEntityBatchImplementation(
		unsigned int thread_id,
		World* world,
		void* thread_function,
		const char* functor_name,
		void* data,
		size_t data_size,
		ComponentSignature optional_signature,
		ComponentSignature optional_shared_signature,
		unsigned int deferred_calls_capacity,
		ThreadFunction task_function
	) {
		const TaskSchedulerInfo* scheduler_info = world->task_scheduler->GetCurrentQueryInfo();
		unsigned int batch_size = scheduler_info->batch_size;

		ArchetypeQueryResult query_result = world->entity_manager->GetQueryResultsAndComponents(scheduler_info->query_handle);
		if (query_result.archetypes.size > 0) {
			if (data_size > 0) {
				void* allocation = world->task_manager->AllocateTempBuffer(thread_id, data_size);
				memcpy(allocation, data, data_size);
				data = allocation;
			}

			ForEachEntityBatchImplementationTaskData task_data;
			task_data.thread_function = thread_function;
			task_data.functor_data = data;

			Component aggregate_components[ECS_ARCHETYPE_MAX_COMPONENTS];
			Component aggregate_shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

			ComponentSignature aggregate_signature = query_result.components.unique.ToNormalSignature(aggregate_components);
			ComponentSignature aggregate_shared_signature = query_result.components.shared.ToNormalSignature(aggregate_shared_components);

			task_data.component_map_count = aggregate_signature.count;
			task_data.shared_component_map_count = aggregate_shared_signature.count;
			task_data.optional_component_map_count = optional_signature.count;
			task_data.optional_shared_component_map_count = optional_shared_signature.count;

			aggregate_signature = optional_signature.CombineInto(aggregate_signature);
			aggregate_shared_signature = optional_shared_signature.CombineInto(aggregate_shared_signature);

			VectorComponentSignature aggregate_vector_signature = aggregate_signature;
			VectorComponentSignature aggregate_vector_shared_signature = aggregate_shared_signature;

			for (unsigned char component_index = 0; component_index < aggregate_signature.count; component_index++) {
				task_data.component_sizes[component_index] = world->entity_manager->ComponentSize(aggregate_signature[component_index]);
			}
			for (unsigned char shared_index = 0; shared_index < aggregate_shared_signature.count; shared_index++) {
				task_data.shared_component_sizes[shared_index] = world->entity_manager->SharedComponentSize(aggregate_shared_signature[shared_index]);
			}

			size_t deferred_call_allocation_size = sizeof(DeferredAction) * deferred_calls_capacity + sizeof(EntityManagerCommandStream);

			for (size_t index = 0; index < query_result.archetypes.size; index++) {
				Archetype* archetype = world->entity_manager->GetArchetype(query_result.archetypes[index]);
				unsigned int base_count = archetype->GetBaseCount();

				// Get the component and shared component map
				world->entity_manager->FindArchetypeUniqueComponentVector(query_result.archetypes[index], aggregate_vector_signature, task_data.component_map);
				world->entity_manager->FindArchetypeSharedComponentVector(query_result.archetypes[index], aggregate_vector_shared_signature, task_data.shared_component_map);
				task_data.archetype_indices.x = query_result.archetypes[index];

				// Check that there is no component index with -1
				// If it happens, then there is a memory corruption
				for (unsigned char component_index = 0; component_index < task_data.component_map_count; component_index++) {
					ECS_CRASH_RETURN(
						task_data.component_map[component_index] != -1,
						"ForEachEntity/Batch: An archetype was corrupted. During query {#} execution a component was not found.",
						functor_name
					);
				}

				for (unsigned char shared_component_index = 0; shared_component_index < task_data.shared_component_map_count; shared_component_index++) {
					ECS_CRASH_RETURN(
						task_data.shared_component_map[shared_component_index] != -1,
						"ForEachEntity/Batch: An archetype was corrupted. During query {#} execution a shared component was not found.",
						functor_name
					);
				}

				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_index);
					unsigned int entity_count = base->EntityCount();
					task_data.archetype_indices.y = base_index;
					task_data.entities = base->m_entities;

					for (unsigned char component_index = 0; component_index < task_data.component_map_count; component_index++) {
						task_data.unique_data[component_index] = base->GetComponentByIndex(0, task_data.component_map[component_index]);
					}
					unsigned char offset = task_data.component_map_count;
					for (unsigned char component_index = 0; component_index < task_data.optional_component_map_count; component_index++) {
						unsigned char current_index = component_index + offset;
						if (task_data.component_map[current_index] != UCHAR_MAX) {
							task_data.unique_data[current_index] = base->GetComponentByIndex(0, task_data.component_map[current_index]);
						}
						else {
							task_data.unique_data[current_index] = nullptr;
						}
					}

					const SharedInstance* shared_instances = archetype->GetBaseInstances(base_index);
					for (unsigned char shared_index = 0; shared_index < task_data.shared_component_map_count; shared_index++) {
						task_data.shared_data[shared_index] = world->entity_manager->GetSharedData(
							aggregate_shared_signature[shared_index],
							shared_instances[task_data.shared_component_map[shared_index]]
						);
					}
					offset = task_data.shared_component_map_count;
					for (unsigned char shared_index = 0; shared_index < task_data.optional_shared_component_map_count; shared_index++) {
						unsigned char current_index = shared_index + offset;
						if (task_data.shared_component_map[current_index] != UCHAR_MAX) {
							task_data.shared_data[current_index] = world->entity_manager->GetSharedData(
								aggregate_shared_signature[current_index],
								shared_instances[task_data.shared_component_map[current_index]]
							);
						}
						else {
							task_data.shared_data[current_index] = nullptr;
						}
					}

					EntityManagerCommandStream* command_stream = nullptr;

					// Coalesce the remainder and the last batch into a single batch
					unsigned int batch_count = entity_count / batch_size;
					if (batch_count > 1) {
						task_data.count = batch_size;
						// Hoist outside the loop the entity manager command stream check
						if (deferred_calls_capacity > 0) {
							for (unsigned int batch_index = 0; batch_index < batch_count - 1; batch_index++) {
								command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);

								command_stream->InitializeFromBuffer(OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_calls_capacity);
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

						// For the last one do the coalescing if necessary
						unsigned int last_batch_start = (batch_count - 1) * batch_size;
						unsigned int remainder = entity_count - last_batch_start;
						task_data.entity_offset = last_batch_start;
						task_data.count = remainder;
						if (deferred_calls_capacity > 0) {
							command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);

							command_stream->InitializeFromBuffer(OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_calls_capacity);
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

							command_stream->InitializeFromBuffer(OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_calls_capacity);
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
		//world->task_scheduler->IncrementQueryIndex();
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	// Use a stack allocator to make allocation for stuff that we need
	// Such that this function can be called from multiple places
	static ArchetypeQuery InitializeForEachData(
		ForEachEntityBatchImplementationTaskData* task_data,
		World* world,
		void* functor,
		void* functor_data,
		const ArchetypeQueryDescriptor& query_descriptor,
		LinearAllocator* temporary_allocator,
		CapacityStream<unsigned int>* archetype_indices
	) {
		const EntityManager* entity_manager = world->entity_manager;
		entity_manager->GetArchetypes(query_descriptor, *archetype_indices);

		task_data->functor_data = functor_data;
		task_data->thread_function = functor;
		task_data->component_map_count = query_descriptor.unique.count;
		task_data->shared_component_map_count = query_descriptor.shared.count;
		task_data->optional_component_map_count = query_descriptor.unique_optional.count;
		task_data->optional_shared_component_map_count = query_descriptor.shared_optional.count;

		// Aggregate the normal components with the optional ones
		unsigned char* component_indices = (unsigned char*)temporary_allocator->Allocate(sizeof(unsigned char) * ECS_ARCHETYPE_MAX_COMPONENTS);
		unsigned char* shared_indices = (unsigned char*)temporary_allocator->Allocate(sizeof(unsigned char) * ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
		Component* aggregated_unique_components = (Component*)temporary_allocator->Allocate(sizeof(Component) * ECS_ARCHETYPE_MAX_COMPONENTS);
		Component* aggregated_shared_components = (Component*)temporary_allocator->Allocate(sizeof(Component) * ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);

		ComponentSignature aggregate_unique = query_descriptor.AggregateUnique(aggregated_unique_components);
		ComponentSignature aggregate_shared = query_descriptor.AggregateShared(aggregated_shared_components);

		for (unsigned int index = 0; index < aggregate_unique.count; index++) {
			task_data->component_sizes[index] = entity_manager->ComponentSize(aggregate_unique[index]);
		}
		for (unsigned int index = 0; index < aggregate_shared.count; index++) {
			task_data->shared_component_sizes[index] = entity_manager->SharedComponentSize(aggregate_shared[index]);
		}

		return { aggregate_unique, aggregate_shared };
	}

	// Initializes data related to unique_data and shared_data for an archetype base
	// The component map must have been called before hand
	static void InitializeForEachDataForArchetypeBase(
		ForEachEntityBatchImplementationTaskData* data,
		World* world
	) {
		const Archetype* archetype = world->entity_manager->GetArchetype(data->archetype_indices.x);
		ArchetypeBase* base = world->entity_manager->GetBase(data->archetype_indices.x, data->archetype_indices.y);
		data->entities = base->m_entities;

		ComponentSignature archetype_signature = archetype->GetUniqueSignature();
		for (unsigned char component_index = 0; component_index < data->component_map_count; component_index++) {
			data->unique_data[component_index] = base->GetComponentByIndex(data->entity_offset, data->component_map[component_index]);
		}
		unsigned char offset = data->component_map_count;
		for (unsigned char component_index = 0; component_index < data->optional_component_map_count; component_index++) {
			unsigned char current_index = offset + component_index;
			data->unique_data[current_index] = data->component_map[current_index] != UCHAR_MAX ?
				base->GetComponentByIndex(data->entity_offset, data->component_map[current_index]) : nullptr;
		}

		const Component* shared_components = archetype->GetSharedSignature().indices;
		const SharedInstance* shared_instances = archetype->GetBaseInstances(data->archetype_indices.y);
		for (unsigned char shared_component_index = 0; shared_component_index < data->shared_component_map_count; shared_component_index++) {
			data->shared_data[shared_component_index] = world->entity_manager->GetSharedData(
				shared_components[data->shared_component_map[shared_component_index]],
				shared_instances[data->shared_component_map[shared_component_index]]
			);
		}
		offset = data->shared_component_map_count;
		for (unsigned char shared_index = 0; shared_index < data->optional_shared_component_map_count; shared_index++) {
			unsigned char current_index = offset + shared_index;
			if (data->shared_component_map[current_index] != UCHAR_MAX) {
				data->shared_data[current_index] = world->entity_manager->GetSharedData(
					shared_components[data->shared_component_map[current_index]],
					shared_instances[data->shared_component_map[current_index]]
				);
			}
			else {
				data->shared_data[current_index] = nullptr;
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ForEachEntityThreadTask) {
		ForEachEntityBatchImplementationTaskData* data = (ForEachEntityBatchImplementationTaskData*)_data;

		ForEachEntityFunctorData functor_data;
		functor_data.thread_id = thread_id;
		functor_data.world = world;
		functor_data.unique_components = data->unique_data;
		functor_data.shared_components = data->shared_data;
		functor_data.data = data->functor_data;
		functor_data.command_stream = data->command_stream;

		ForEachEntityFunctor functor = (ForEachEntityFunctor)data->thread_function;
		for (unsigned short index = 0; index < data->count; index++) {
			functor_data.entity = data->entities[data->entity_offset + index];
			functor(&functor_data);

			// Offset the functor uniques now
			for (unsigned char component_index = 0; component_index < data->component_map_count; component_index++) {
				data->unique_data[component_index] = OffsetPointer(data->unique_data[component_index], data->component_sizes[component_index]);
			}
			unsigned char offset = data->component_map_count;
			for (unsigned char component_index = 0; component_index < data->optional_component_map_count; component_index++) {
				if (data->unique_data[offset + component_index] != nullptr) {
					data->unique_data[offset + component_index] = OffsetPointer(data->unique_data[offset + component_index], data->component_sizes[component_index]);
				}
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ForEachBatchThreadTask) {
		ForEachEntityBatchImplementationTaskData* data = (ForEachEntityBatchImplementationTaskData*)_data;

		ForEachBatchFunctorData functor_data;
		functor_data.thread_id = thread_id;
		functor_data.world = world;
		functor_data.entities = data->entities;
		functor_data.count = data->count;
		functor_data.command_stream = data->command_stream;
		functor_data.unique_components = data->unique_data;
		functor_data.shared_components = data->shared_data;
		functor_data.data = data->functor_data;

		ForEachBatchFunctor functor = (ForEachBatchFunctor)data->thread_function;
		functor(&functor_data);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	template<bool is_batch>
	void ForEachEntityOrBatchCommitFunctor(
		World* world,
		void* functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	)
	{
		ECS_STACK_CAPACITY_STREAM(unsigned int, archetype_indices, ECS_MAIN_ARCHETYPE_MAX_COUNT);
		ECS_STACK_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 8);
		EntityManager* entity_manager = world->entity_manager;
		ForEachEntityBatchImplementationTaskData task_data;
		ArchetypeQuery query = InitializeForEachData(&task_data, world, functor, data, query_descriptor, &stack_allocator, &archetype_indices);

		for (unsigned int index = 0; index < archetype_indices.size; index++) {
			Archetype* archetype = entity_manager->GetArchetype(archetype_indices[index]);
			unsigned int base_count = archetype->GetBaseCount();

			task_data.archetype_indices.x = archetype_indices[index];

			entity_manager->FindArchetypeUniqueComponentVector(task_data.archetype_indices.x, query.unique, task_data.component_map);
			entity_manager->FindArchetypeSharedComponentVector(task_data.archetype_indices.x, query.shared, task_data.shared_component_map);

			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				ArchetypeBase* base = archetype->GetBase(base_index);
				task_data.command_stream = nullptr;
				task_data.archetype_indices.y = base_index;
				task_data.count = base->EntityCount();
				task_data.entity_offset = 0;

				InitializeForEachDataForArchetypeBase(&task_data, world);
				if constexpr (is_batch) {
					ForEachBatchThreadTask(0, world, &task_data);
				}
				else {
					ForEachEntityThreadTask(0, world, &task_data);
				}
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ForEachEntityCommitFunctor(
		World* world,
		ForEachEntityFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	) {
		ForEachEntityOrBatchCommitFunctor<false>(world, functor, data, query_descriptor);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ForEachBatchCommitFunctor(
		World* world,
		ForEachBatchFunctor functor,
		void* data,
		unsigned short batch_size,
		const ArchetypeQueryDescriptor& query_descriptor
	)
	{
		// Ignore the batch size at the moment - the commit version will be called
		// With the entire count of the entities for a base archetype
		ForEachEntityOrBatchCommitFunctor<true>(world, functor, data, query_descriptor);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	namespace Internal {

		void ForEachEntity(
			unsigned int thread_id,
			World* world,
			ForEachEntityFunctor functor,
			const char* functor_name,
			void* data,
			size_t data_size,
			ComponentSignature optional_signature,
			ComponentSignature optional_shared_signature,
			unsigned int deferred_calls_capacity
		) {
			ForEachEntityBatchImplementation(
				thread_id, 
				world, 
				functor, 
				functor_name, 
				data, 
				data_size, 
				optional_signature, 
				optional_shared_signature, 
				deferred_calls_capacity, 
				ForEachEntityThreadTask
			);
		}

		void ForEachBatch(
			unsigned int thread_id,
			World* world,
			ForEachBatchFunctor functor,
			const char* functor_name,
			void* data,
			size_t data_size,
			ComponentSignature optional_signature,
			ComponentSignature optional_shared_signature,
			unsigned int deferred_calls_capacity
		)
		{
			ForEachEntityBatchImplementation(
				thread_id, 
				world, 
				functor, 
				functor_name, 
				data, 
				data_size,
				optional_signature,
				optional_shared_signature,
				deferred_calls_capacity,
				ForEachBatchThreadTask
			);
		}

		void IncrementWorldQueryIndex(World* world)
		{
			if (world->task_scheduler != nullptr) {
				world->task_scheduler->IncrementQueryIndex();
			}
		}

	}

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
	//			matched_archetypes.AddAssert(index);
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