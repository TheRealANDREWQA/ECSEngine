#include "ecspch.h"
#include "ForEach.h"
#include "World.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

#define FOR_EACH_EMBEDDED_CAPACITY 8

	// The maximum allowed batch component buffers size from the stack
#define FOR_EACH_SELECTION_BUFFERS_STACK_CAPACITY ECS_KB * 128

	// -------------------------------------------------------------------------------------------------------------------------------

	// This is used by both the normal for each, but also by the selection iteration
	// Certain fields are not needed for the selection iteration, like archetype_indices
	struct ForEachEntityBatchImplementationTaskData {
		uint2 archetype_indices;
		unsigned char component_map[FOR_EACH_EMBEDDED_CAPACITY];
		unsigned char shared_component_map[FOR_EACH_EMBEDDED_CAPACITY];

		unsigned char component_map_count;
		unsigned char shared_component_map_count;
		unsigned char optional_component_map_count;
		unsigned char optional_shared_component_map_count;

		unsigned short component_sizes[FOR_EACH_EMBEDDED_CAPACITY];

		// Not used by the selection for each
		void** archetype_buffers;
		void* shared_data[FOR_EACH_EMBEDDED_CAPACITY];

		const Entity* entities;
		unsigned int entity_offset;
		unsigned int count;

		void* functor_data;
		void* thread_function;
		EntityManagerCommandStream* command_stream;
	};

	// Structure that allows writing components directly after it and retrieving them. Must be placed at the end of a structure in order to be used
	struct InPlaceSignature {
		ECS_INLINE ComponentSignature UniqueSignature() const {
			return { (Component*)OffsetPointer(this, sizeof(*this)), unique_count };
		}

		ECS_INLINE ComponentSignature OptionalUniqueSignature() const {
			return { (Component*)OffsetPointer(this, sizeof(*this)) + unique_count, optional_unique_count };
		}

		ECS_INLINE ComponentSignature SharedSignature() const {
			return { (Component*)OffsetPointer(this, sizeof(*this)) + unique_count + optional_unique_count, shared_count };
		}

		ECS_INLINE ComponentSignature OptionalSharedSignature() const {
			return { (Component*)OffsetPointer(this, sizeof(*this)) + unique_count + optional_unique_count + shared_count, optional_shared_count };
		}

		ECS_INLINE ArchetypeQueryDescriptor GetQueryDescriptor() const {
			ArchetypeQueryDescriptor descriptor;

			descriptor.unique = UniqueSignature();
			descriptor.unique_optional = OptionalUniqueSignature();
			descriptor.shared = SharedSignature();
			descriptor.shared_optional = OptionalSharedSignature();

			return descriptor;
		}

		void WriteComponents(const ArchetypeQueryDescriptor& query_descriptor) {
			unique_count = query_descriptor.unique.count;
			optional_unique_count = query_descriptor.unique_optional.count;
			shared_count = query_descriptor.shared.count;
			optional_shared_count = query_descriptor.shared_optional.count;

			query_descriptor.unique.WriteTo(UniqueSignature().indices);
			query_descriptor.unique_optional.WriteTo(OptionalUniqueSignature().indices);
			query_descriptor.shared.WriteTo(SharedSignature().indices);
			query_descriptor.shared_optional.WriteTo(OptionalSharedSignature().indices);
		}

		ECS_INLINE static size_t MemoryOf(const ArchetypeQueryDescriptor& query_descriptor) {
			return sizeof(Component) * (query_descriptor.unique.count + query_descriptor.unique_optional.count
				+ query_descriptor.shared.count + query_descriptor.shared_optional.count);
		}

		unsigned char unique_count;
		unsigned char optional_unique_count;
		unsigned char shared_count;
		unsigned char optional_shared_count;
	};

	template<typename Structure>
	struct ForEachDataWithInPlaceComponents {
		ECS_INLINE ArchetypeQueryDescriptor GetQueryDescriptor() const {
			return signature.GetQueryDescriptor();
		}

		ECS_INLINE void WriteComponents(const ArchetypeQueryDescriptor& query_descriptor) {
			signature.WriteComponents(query_descriptor);
		}

		ECS_INLINE static size_t MemoryOf(const ArchetypeQueryDescriptor& query_descriptor) {
			return sizeof(ForEachDataWithInPlaceComponents<Structure>) + InPlaceSignature::MemoryOf(query_descriptor);
		}

		Structure base;
		InPlaceSignature signature;
	};

	struct ForEachEntitySelectionImplementationTaskData_ {
		size_t index_offset;
		StableIterator<const Entity> stable_iterator;
		void* functor_data;
		ForEachEntitySelectionUntypedFunctor functor;
		EntityManagerCommandStream* command_stream;
		// This is set only if the initial iterator was stable. Will be passed to the functors
		IteratorInterface<const Entity>* initial_iterator;
	};

	// This structure should be allocated dinamically, such that components are written immediately after this structure.
	// In order to reduce memory footprint (which can ammount to significant gains), do not store the pointers
	typedef ForEachDataWithInPlaceComponents<ForEachEntitySelectionImplementationTaskData_> ForEachEntitySelectionImplementationTaskData;

	struct SharedGroupStatus {
		// The first 3 fields are used in conjunction to obtain the initialize synchronization. One flag is used to allow only a single thread
		// To perform the group functor, while the other one is used to let the other threads with the same group know that it has finished.
		SpinLock initialize_lock;
		AtomicFlag initialize_is_finalized;
		// In case the group is aborted, this flag will be set to indicate to the other batch functors to not continue
		bool is_group_aborted;
		// The last 3 fields are used in conjunction to obtain the finalize synchronization. Batch total count will be set by the main
		// Thread that performs the task creation to a non zero value after the total number of batches is known. Batch finish count
		// Will be incremented by each thread task batch and the last thread will perform the finalize functor call.
		AtomicFlag finalize_count_is_initialized;
		std::atomic<unsigned int> batch_finish_count;
		unsigned int batch_total_count;
	};

	struct ForEachEntitySelectionSharedGrouppingImplementationTaskData_ {
		// The shared instance value of the shared component for which the grouping is made
		SharedInstance shared_instance;
		// These are the entities of the entire group, which are needed for the group functor
		Stream<Entity> group_entities;
		// This is a pointer to a flag that needs to be acquired in order to run the group functor
		CacheAligned<SharedGroupStatus>* group_status;
		// These are the entities to be used for the entity iteration, which may be smaller than the
		// Total group count
		Stream<Entity> batch_entities;
		void* functor_data;
		ForEachEntitySelectionSharedGroupingUntypedFunctor group_initialize_functor;
		ForEachEntitySelectionSharedGroupingUntypedFunctor group_finalize_functor;
		ForEachEntitySelectionUntypedFunctor entity_functor;
		EntityManagerCommandStream* command_stream;
		IteratorInterface<const Entity>* initial_iterator;
	};

	// This structure should be allocated dinamically, such that components are written immediately after this structure.
	// In order to reduce memory footprint (which can ammount to significant gains), do not store the pointers
	typedef ForEachDataWithInPlaceComponents<ForEachEntitySelectionSharedGrouppingImplementationTaskData_> ForEachEntitySelectionSharedGroupingImplementationTaskData;

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
		data->archetype_buffers = base->m_buffers;

		const Component* shared_components = archetype->GetSharedSignature().indices;
		const SharedInstance* shared_instances = archetype->GetBaseInstances(data->archetype_indices.y);
		for (unsigned char shared_component_index = 0; shared_component_index < data->shared_component_map_count; shared_component_index++) {
			data->shared_data[shared_component_index] = world->entity_manager->GetSharedData(
				shared_components[data->shared_component_map[shared_component_index]],
				shared_instances[data->shared_component_map[shared_component_index]]
			);
		}
		unsigned char offset = data->shared_component_map_count;
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

	static void InitializeForEachDataUniqueData(
		ForEachEntityBatchImplementationTaskData* data,
		void** unique_components
	) {
		for (unsigned char component_index = 0; component_index < data->component_map_count; component_index++) {
			unique_components[component_index] = data->archetype_buffers[data->component_map[component_index]];
			unique_components[component_index] = OffsetPointer(unique_components[component_index], data->component_sizes[component_index] * data->entity_offset);
		}
		unsigned char offset = data->component_map_count;
		for (unsigned char component_index = 0; component_index < data->optional_component_map_count; component_index++) {
			unsigned char current_index = component_index + offset;
			if (data->component_map[current_index] != UCHAR_MAX) {
				unique_components[current_index] = data->archetype_buffers[data->component_map[current_index]];
				unique_components[current_index] = OffsetPointer(unique_components[current_index], data->component_sizes[current_index] * data->entity_offset);
			}
			else {
				unique_components[current_index] = nullptr;
			}
		}
	}

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

			ECS_ASSERT(aggregate_signature.count <= FOR_EACH_EMBEDDED_CAPACITY);
			ECS_ASSERT(aggregate_shared_signature.count <= FOR_EACH_EMBEDDED_CAPACITY);

			aggregate_signature = optional_signature.CombineInto(aggregate_signature);
			aggregate_shared_signature = optional_shared_signature.CombineInto(aggregate_shared_signature);

			VectorComponentSignature aggregate_vector_signature = aggregate_signature;
			VectorComponentSignature aggregate_vector_shared_signature = aggregate_shared_signature;

			for (unsigned char component_index = 0; component_index < aggregate_signature.count; component_index++) {
				task_data.component_sizes[component_index] = world->entity_manager->ComponentSize(aggregate_signature[component_index]);
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
					ECS_CRASH_CONDITION(
						task_data.component_map[component_index] != UCHAR_MAX,
						"ForEachEntity/Batch: An archetype was corrupted. During query {#} execution a component was not found.",
						functor_name
					);
				}

				for (unsigned char shared_component_index = 0; shared_component_index < task_data.shared_component_map_count; shared_component_index++) {
					ECS_CRASH_CONDITION(
						task_data.shared_component_map[shared_component_index] != UCHAR_MAX,
						"ForEachEntity/Batch: An archetype was corrupted. During query {#} execution a shared component was not found.",
						functor_name
					);
				}

				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_index);
					unsigned int entity_count = base->EntityCount();
					task_data.archetype_indices.y = base_index;
					InitializeForEachDataForArchetypeBase(&task_data, world);

					EntityManagerCommandStream* command_stream = nullptr;

					auto task_data_functor = [&](size_t batch_index, size_t current_count, auto has_command_stream) {
						task_data.count = current_count;
						if constexpr (has_command_stream) {
							command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);

							command_stream->InitializeFromBuffer(OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_calls_capacity);
							task_data.command_stream = command_stream;
						}
						task_data.entity_offset = batch_index * batch_size;
					};

					// Hoist outside the loop the entity manager command stream check
					if (deferred_calls_capacity > 0) {
						world->task_manager->AddDynamicTaskParallelFor(task_function, functor_name, entity_count, batch_size, &task_data, sizeof(task_data), true,
							[&](size_t batch_index, size_t current_count) {
								task_data_functor(batch_index, current_count, std::true_type{});
						});
					}
					else {
						task_data.command_stream = nullptr;
						world->task_manager->AddDynamicTaskParallelFor(task_function, functor_name, entity_count, batch_size, &task_data, sizeof(task_data), true,
							[&](size_t batch_index, size_t current_count) {
								task_data_functor(batch_index, current_count, std::false_type{});
						});
					}
				}
			}
		}
		// After launching all tasks, if any, increment the query index for the scheduler
		//world->task_scheduler->IncrementQueryIndex();
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(ForEachEntityThreadTask) {
		ForEachEntityBatchImplementationTaskData* data = (ForEachEntityBatchImplementationTaskData*)_data;

		void* unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
		InitializeForEachDataUniqueData(data, unique_components);

		ForEachEntityUntypedFunctorData functor_data;
		functor_data.base.thread_id = thread_id;
		functor_data.base.world = world;
		functor_data.unique_components = unique_components;
		functor_data.shared_components = data->shared_data;
		functor_data.base.user_data = data->functor_data;
		functor_data.base.command_stream = data->command_stream;

		ForEachEntityUntypedFunctor functor = (ForEachEntityUntypedFunctor)data->thread_function;
		for (unsigned short index = 0; index < data->count; index++) {
			functor_data.base.entity = data->entities[data->entity_offset + index];
			functor(&functor_data);

			// Offset the functor uniques now
			for (unsigned char component_index = 0; component_index < data->component_map_count; component_index++) {
				unique_components[component_index] = OffsetPointer(unique_components[component_index], data->component_sizes[component_index]);
			}
			unsigned char offset = data->component_map_count;
			for (unsigned char component_index = 0; component_index < data->optional_component_map_count; component_index++) {
				unsigned char optional_index = offset + component_index;
				if (unique_components[optional_index] != nullptr) {
					unique_components[optional_index] = OffsetPointer(unique_components[optional_index], data->component_sizes[optional_index]);
				}
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(ForEachBatchThreadTask) {
		ForEachEntityBatchImplementationTaskData* data = (ForEachEntityBatchImplementationTaskData*)_data;

		void* unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
		InitializeForEachDataUniqueData(data, unique_components);

		ForEachBatchUntypedFunctorData functor_data;
		functor_data.base.thread_id = thread_id;
		functor_data.base.world = world;
		functor_data.base.entities = data->entities;
		functor_data.base.count = data->count;
		functor_data.base.command_stream = data->command_stream;
		functor_data.unique_components = unique_components;
		functor_data.shared_components = data->shared_data;
		functor_data.base.user_data = data->functor_data;

		ForEachBatchUntypedFunctor functor = (ForEachBatchUntypedFunctor)data->thread_function;
		functor(&functor_data);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	static void ForEachEntitySelectionThreadTaskCommon(
		StableIterator<const Entity> stable_iterator,
		unsigned int thread_id,
		size_t iteration_start_index,
		World* world,
		EntityManagerCommandStream* command_stream,
		void* user_data,
		ForEachEntitySelectionUntypedFunctor functor,
		IteratorInterface<const Entity>* initial_iterator,
		const ArchetypeQueryDescriptor& query_descriptor
	) {
		void* unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
		void* shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		ForEachEntitySelectionUntypedFunctorData functor_data;
		functor_data.unique_components = unique_components;
		functor_data.shared_components = shared_components;
		functor_data.base.command_stream = command_stream;
		functor_data.base.user_data = user_data;
		functor_data.base.thread_id = thread_id;
		functor_data.base.world = world;
		functor_data.base.iterator = initial_iterator;

		size_t index = iteration_start_index;
		stable_iterator.iterator->ForEach([&index, world, &functor_data, &query_descriptor, functor](const Entity* entity) {
			functor_data.base.index = index;
			index++;

			// For the mandatory components, crash if the component is missing
			Entity current_entity = *entity;
			functor_data.base.entity = current_entity;

			for (size_t component_index = 0; component_index < (size_t)query_descriptor.unique.count; component_index++) {
				functor_data.unique_components[component_index] = world->entity_manager->GetComponent(current_entity, query_descriptor.unique[component_index]);
			}
			for (size_t component_index = 0; component_index < (size_t)query_descriptor.unique_optional.count; component_index++) {
				functor_data.unique_components[query_descriptor.unique.count + component_index] = world->entity_manager->TryGetComponent(current_entity, query_descriptor.unique_optional[component_index]);
			}
			for (size_t component_index = 0; component_index < (size_t)query_descriptor.shared.count; component_index++) {
				functor_data.shared_components[component_index] = world->entity_manager->GetSharedComponent(current_entity, query_descriptor.shared[component_index]);
			}
			for (size_t component_index = 0; component_index < (size_t)query_descriptor.shared_optional.count; component_index++) {
				functor_data.shared_components[query_descriptor.shared.count + component_index] = world->entity_manager->TryGetSharedComponent(current_entity, query_descriptor.shared_optional[component_index]);
			}
			functor(&functor_data);
		});
	}
	
	static ECS_THREAD_TASK(ForEachEntitySelectionThreadTask) {
		ForEachEntitySelectionImplementationTaskData* data = (ForEachEntitySelectionImplementationTaskData*)_data;
		ArchetypeQueryDescriptor query_descriptor = data->GetQueryDescriptor();
		ForEachEntitySelectionThreadTaskCommon(
			data->base.stable_iterator, 
			thread_id, 
			data->base.index_offset, 
			world, 
			data->base.command_stream, 
			data->base.functor_data, 
			data->base.functor, 
			data->base.initial_iterator, 
			query_descriptor
		);
	}

	// -------------------------------------------------------------------------------------------------------------------------------
	
	static ECS_THREAD_TASK(ForEachEntitySelectionSharedGroupingThreadTask) {
		ForEachEntitySelectionSharedGroupingImplementationTaskData* data = (ForEachEntitySelectionSharedGroupingImplementationTaskData*)_data;

		// Try to acquire the group status
		if (data->base.group_initialize_functor != nullptr) {
			if (data->base.group_status->value.initialize_lock.TryLock()) {
				ForEachEntitySelectionSharedGroupingUntypedFunctorData functor_data;
				functor_data.command_stream = data->base.command_stream;
				functor_data.entities = data->base.group_entities;
				functor_data.iterator = data->base.initial_iterator;
				functor_data.shared_instance = data->base.shared_instance;
				functor_data.thread_id = thread_id;
				functor_data.user_data = data->base.functor_data;
				functor_data.world = world;
				data->base.group_status->value.is_group_aborted = !data->base.group_initialize_functor(&functor_data);

				// Notify the other threads that we finished
				data->base.group_status->value.initialize_is_finalized.Signal();
			}
			else {
				// Wait for the other thread to finish
				data->base.group_status->value.initialize_is_finalized.Wait();
			}

			if (data->base.group_status->value.is_group_aborted) {
				// Do not continue
				return;
			}
		}

		// Perform the common part
		ArchetypeQueryDescriptor query_descriptor = data->GetQueryDescriptor();
		StreamIterator<const Entity> iterator = data->base.batch_entities.ConstIterator();
		ForEachEntitySelectionThreadTaskCommon(
			ToStableIterator(&iterator),
			thread_id,
			ULLONG_MAX - 1000, // This value is a placeholder, use a large value such that it will likely result in a crash if the user tries to use it
			world,
			data->base.command_stream,
			data->base.functor_data,
			data->base.entity_functor,
			data->base.initial_iterator,
			query_descriptor
		);

		// Do the finalize part
		if (data->base.group_finalize_functor != nullptr) {
			unsigned int finished_count = data->base.group_status->value.batch_finish_count.fetch_add(1, ECS_RELAXED);
			data->base.group_status->value.finalize_count_is_initialized.Wait();
			if (finished_count == data->base.group_status->value.batch_total_count - 1) {
				// Perform the finalize call
				ForEachEntitySelectionSharedGroupingUntypedFunctorData functor_data;
				functor_data.command_stream = data->base.command_stream;
				functor_data.entities = data->base.group_entities;
				functor_data.iterator = data->base.initial_iterator;
				functor_data.shared_instance = data->base.shared_instance;
				functor_data.thread_id = thread_id;
				functor_data.user_data = data->base.functor_data;
				functor_data.world = world;
				data->base.group_finalize_functor(&functor_data);
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	template<bool is_batch>
	static void ForEachEntityOrBatchCommitFunctor(
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
		ForEachEntityUntypedFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	) {
		ForEachEntityOrBatchCommitFunctor<false>(world, functor, data, query_descriptor);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ForEachBatchCommitFunctor(
		World* world,
		ForEachBatchUntypedFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	)
	{
		ForEachEntityOrBatchCommitFunctor<true>(world, functor, data, query_descriptor);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ForEachEntitySelectionCommitFunctor(
		IteratorInterface<const Entity>* entities,
		World* world,
		ForEachEntitySelectionUntypedFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	) {
		alignas(alignof(void*)) char task_data_storage[ECS_KB * 8];
		ForEachEntitySelectionImplementationTaskData* task_data = (ForEachEntitySelectionImplementationTaskData*)task_data_storage;
		size_t task_data_size = task_data->MemoryOf(query_descriptor);
		ECS_CRASH_CONDITION(task_data_size <= sizeof(task_data_storage), "ForEachEntitySelectionCommitFunctor: Exceeded task data storage capacity.");

		task_data->base.stable_iterator = ToStableIterator(entities);
		task_data->base.initial_iterator = entities;
		task_data->base.functor = functor;
		task_data->base.functor_data = data;
		task_data->base.index_offset = 0;
		task_data->base.command_stream = nullptr;
		task_data->WriteComponents(query_descriptor);
		ForEachEntitySelectionThreadTask(0, world, task_data);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ForEachEntitySelectionSharedGroupingCommitFunctor(
		IteratorInterface<const Entity>* entities,
		bool allow_missing_component_group,
		World* world,
		ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_initialize_functor,
		ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_finalize_functor,
		ForEachEntitySelectionUntypedFunctor entity_functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	) {
		// Do not use ForEachEntitySelectionSharedGroupingThreadTask because it incurs overhead with the atomic operations.
		// Use the for
		alignas(alignof(void*)) char task_data_storage[ECS_KB * 8];
		ForEachEntitySelectionImplementationTaskData* task_data = (ForEachEntitySelectionImplementationTaskData*)task_data_storage;
		size_t task_data_size = task_data->MemoryOf(query_descriptor);
		ECS_CRASH_CONDITION(task_data_size <= sizeof(task_data_storage), "ForEachEntitySelectionSharedGroupingCommitFunctor: Exceeded task data storage capacity.");

		ECS_CRASH_CONDITION((allow_missing_component_group && query_descriptor.shared_optional.count > 0) ||
			(!allow_missing_component_group && query_descriptor.shared.count > 0), "ForEachEntitySelectionSharedGrouping: a shared component must be specified!");
		
		ECS_CRASH_CONDITION(grouping_initialize_functor != nullptr || grouping_finalize_functor != nullptr, "ForEachEntitySelectionSharedGrouping: A group functor (initialize and/or finalize) must be specified!");

		// Allocate the entities into a temporary buffer
		Stream<Entity> entities_array;
		entities_array.buffer = (Entity*)entities->WriteTo(world->entity_manager->TemporaryAllocator(), &entities_array.size);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR_BACKUP(stack_allocator, ECS_KB * 64, ECS_MB * 32, world->entity_manager->TemporaryAllocator());
		ResizableStream<EntitySharedGroup> groups;
		groups.Initialize(&stack_allocator, 128);
		if (allow_missing_component_group) {
			world->entity_manager->GroupEntitiesBySharedInstanceWithMissing(entities_array, query_descriptor.shared_optional[0], &groups);
		}
		else {
			world->entity_manager->GroupEntitiesBySharedInstance(entities_array, query_descriptor.shared[0], &groups);
		}

		StreamIterator<const Entity> stream_iterator = entities_array.ConstIterator();
		StableIterator<const Entity> stable_iterator = ToStableIterator(&stream_iterator);
		task_data->base.command_stream = nullptr;
		task_data->base.functor = entity_functor;
		task_data->base.functor_data = data;
		task_data->base.index_offset = -1;
		task_data->base.initial_iterator = entities;
		
		ForEachEntitySelectionSharedGroupingUntypedFunctorData grouping_data;
		grouping_data.command_stream = nullptr;
		grouping_data.iterator = entities;
		grouping_data.thread_id = 0;
		grouping_data.user_data = data;
		grouping_data.world = world;
		for (unsigned int index = 0; index < groups.size; index++) {
			grouping_data.shared_instance = groups[index].instance;
			grouping_data.entities = { entities_array.buffer + groups[index].offset, groups[index].size };
			bool is_group_aborted = false;
			if (grouping_initialize_functor != nullptr) {
				is_group_aborted = !grouping_initialize_functor(&grouping_data);
			}

			if (is_group_aborted) {
				// Skip this iteration
				continue;
			}
			
			task_data->base.stable_iterator = stable_iterator.GetSubrange(&stack_allocator, groups[index].size);
			ForEachEntitySelectionThreadTask(0, world, task_data);

			if (grouping_finalize_functor != nullptr) {
				grouping_finalize_functor(&grouping_data);
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	namespace Internal {

		void ForEachEntity(
			unsigned int thread_id,
			World* world,
			ForEachEntityUntypedFunctor functor,
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
			ForEachBatchUntypedFunctor functor,
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

		void ForEachEntitySelection(
			IteratorInterface<const Entity>* entities,
			bool are_entities_stable,
			unsigned int thread_id,
			World* world,
			ForEachEntitySelectionUntypedFunctor functor,
			const char* functor_name,
			void* data,
			size_t data_size,
			const ArchetypeQueryDescriptor& query_descriptor,
			unsigned int deferred_action_capacity,
			unsigned short batch_size
		) {
			ECS_CRASH_CONDITION(query_descriptor.unique_exclude.count == 0 && query_descriptor.shared_exclude.count == 0,
				"ForEachEntitySelection: Exclude queries are not supported.");

			if (data_size > 0) {
				void* allocation = world->task_manager->AllocateTempBuffer(thread_id, data_size);
				memcpy(allocation, data, data_size);
				data = allocation;
			}

			size_t entity_count = entities->remaining_count;
			AllocatorPolymorphic temporary_allocator = world->entity_manager->TemporaryAllocator();
			// Lock it until the task creation is finished 
			// in order to not produce many unnecessary lock and unlocks
			LockAllocator(temporary_allocator);

			temporary_allocator.allocation_type = ECS_ALLOCATION_SINGLE;
			StableIterator<const Entity> stable_iterator = GetStableIterator(entities, are_entities_stable, temporary_allocator);

			alignas(void*) char task_data_storage[ECS_KB];
			size_t task_data_size = ForEachEntitySelectionImplementationTaskData::MemoryOf(query_descriptor);
			ECS_CRASH_CONDITION(task_data_size <= sizeof(task_data_storage), "ForEachEntitySelection: Stack buffer limit was exceeded.");

			ForEachEntitySelectionImplementationTaskData* task_data = (ForEachEntitySelectionImplementationTaskData*)task_data_storage;
			task_data->base.functor = functor;
			task_data->base.functor_data = data;
			task_data->base.stable_iterator = stable_iterator;
			task_data->base.initial_iterator = are_entities_stable ? entities : nullptr;
			task_data->WriteComponents(query_descriptor);

			size_t deferred_call_allocation_size = sizeof(DeferredAction) * deferred_action_capacity + sizeof(EntityManagerCommandStream);
			EntityManagerCommandStream* command_stream = nullptr;

			size_t index_offset = 0;
			auto task_data_functor = [&](size_t batch_index, size_t current_count) {
				task_data->base.stable_iterator = task_data->base.stable_iterator.GetSubrange(temporary_allocator, current_count);
				task_data->base.index_offset = index_offset;

				if (deferred_action_capacity > 0) {
					command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);
					command_stream->InitializeFromBuffer(OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_action_capacity);
					task_data->base.command_stream = command_stream;
				}
				index_offset += current_count;
			};

			// Calculate the batch size. If it is 0, then compute a batch size such that 2x thread count of tasks are spawned
			size_t size_t_batch_size = batch_size;
			if (batch_size == 0) {
				size_t_batch_size = SlotsFor(entity_count, (size_t)world->task_manager->GetThreadCount());
			}
			world->task_manager->AddDynamicTaskParallelFor(ForEachEntitySelectionThreadTask, functor_name, entities->remaining_count, size_t_batch_size, task_data, task_data_size, true, task_data_functor);
			UnlockAllocator(temporary_allocator);
		}

		void ForEachEntitySelectionSharedGrouping(
			IteratorInterface<const Entity>* entities,
			unsigned int thread_id,
			World* world,
			ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_initialize_functor,
			ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_finalize_functor,
			ForEachEntitySelectionUntypedFunctor entity_functor,
			const char* functor_name,
			void* data,
			size_t data_size,
			const ArchetypeQueryDescriptor& query_descriptor,
			bool allow_missing_component_group,
			unsigned int deferred_action_capacity,
			unsigned short batch_size
		) {
			ECS_CRASH_CONDITION(query_descriptor.unique_exclude.count == 0 && query_descriptor.shared_exclude.count == 0, 
				"ForEachEntitySelectionSharedGrouping: Exclude queries are not supported.");
			ECS_CRASH_CONDITION((allow_missing_component_group && query_descriptor.shared_optional.count > 0) || 
				(!allow_missing_component_group && query_descriptor.shared.count > 0), "ForEachEntitySelectionSharedGrouping: a shared component must be specified!");

			ECS_CRASH_CONDITION(grouping_initialize_functor != nullptr || grouping_finalize_functor != nullptr, "ForEachEntitySelectionSharedGrouping: A group functor (initialize and/or finalize) must be specified!");

			if (data_size > 0) {
				void* allocation = world->task_manager->AllocateTempBuffer(thread_id, data_size);
				memcpy(allocation, data, data_size);
				data = allocation;
			}

			alignas(void*) char task_data_storage[ECS_KB];
			size_t task_data_size = ForEachEntitySelectionSharedGroupingImplementationTaskData::MemoryOf(query_descriptor);
			ECS_CRASH_CONDITION(task_data_size <= sizeof(task_data_storage), "ForEachEntitySelectionSharedGrouping: Stack buffer limit was exceeded.");

			// Allocate the entities into a temporary buffer
			Stream<Entity> entities_array;
			entities_array.buffer = (Entity*)entities->WriteTo(world->entity_manager->TemporaryAllocator(), &entities_array.size);

			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR_BACKUP(stack_allocator, ECS_KB * 64, ECS_MB * 32, world->entity_manager->TemporaryAllocator());
			ResizableStream<EntitySharedGroup> groups;
			// Initialize this with a small size already
			groups.Initialize(&stack_allocator, 128);

			// Retrieve the groups on the main thread
			if (allow_missing_component_group) {
				world->entity_manager->GroupEntitiesBySharedInstanceWithMissing(entities_array, query_descriptor.shared_optional[0], &groups);
			}
			else {
				world->entity_manager->GroupEntitiesBySharedInstance(entities_array, query_descriptor.shared[0], &groups);
			}

			// Allocate the group status structures. Use cache aligned structures to avoid false sharing.
			CacheAligned<SharedGroupStatus>* statuses = (CacheAligned<SharedGroupStatus>*)world->entity_manager->AllocateTemporaryBuffer(sizeof(CacheAligned<SharedGroupStatus>) * groups.size);
			memset(statuses, 0, sizeof(CacheAligned<SharedGroupStatus>) * groups.size);

			ForEachEntitySelectionSharedGroupingImplementationTaskData* task_data = (ForEachEntitySelectionSharedGroupingImplementationTaskData*)task_data_storage;
			task_data->base.entity_functor = entity_functor;
			task_data->base.group_initialize_functor = grouping_initialize_functor;
			task_data->base.group_finalize_functor = grouping_finalize_functor;
			task_data->base.functor_data = data;
			task_data->base.initial_iterator = entities;
			task_data->WriteComponents(query_descriptor);

			size_t deferred_call_allocation_size = sizeof(DeferredAction) * deferred_action_capacity + sizeof(EntityManagerCommandStream);
			EntityManagerCommandStream* command_stream = nullptr;

			size_t batch_size_long = batch_size;
			if (batch_size == 0) {
				batch_size_long = SlotsFor(entities_array.size, (size_t)world->task_manager->GetThreadCount());
			}
			
			// Ensure that each task runs with the same shared instance. For this, we must perform the partition ourselves, without using AddDynamicTaskParallelFor.
			size_t last_batch_entity_offset = 0;
			size_t group_index = 0;
			size_t current_group_batch_count = 0;

			// This is the index at which to add the task to. We maintain a stack value such that we avoid using AddDynamicTask which needs to perform
			// An atomic increment each time.
			unsigned int thread_affinity = 0;
			unsigned int thread_count = world->task_manager->GetThreadCount();
			while (last_batch_entity_offset < entities_array.size) {
				task_data->base.shared_instance = groups[group_index].instance;
				task_data->base.group_status = statuses + group_index;
				size_t group_start = groups[group_index].offset;
				size_t group_size = groups[group_index].size;
				current_group_batch_count++;

				// Determine if the batch straddles instance boundaries
				size_t current_batch_size = batch_size_long;
				if (last_batch_entity_offset + batch_size_long > (size_t)groups[group_index].size) {
					// We must use only the count until the group is exhausted
					current_batch_size = (size_t)groups[group_index].size - (last_batch_entity_offset - (size_t)groups[group_index].offset);
					statuses[group_index].value.batch_total_count = current_group_batch_count;
					statuses[group_index].value.finalize_count_is_initialized.Signal();

					group_index++;
					current_group_batch_count = 0;
				}

				if (deferred_action_capacity > 0) {
					command_stream = (EntityManagerCommandStream*)world->task_manager->AllocateTempBuffer(thread_id, deferred_call_allocation_size);
					command_stream->InitializeFromBuffer(OffsetPointer(command_stream, sizeof(EntityManagerCommandStream)), 0, deferred_action_capacity);
					task_data->base.command_stream = command_stream;
				}

				task_data->base.group_entities = { entities_array.buffer + group_start, group_size };
				task_data->base.batch_entities = { entities_array.buffer + last_batch_entity_offset, current_batch_size };

				world->task_manager->AddDynamicTaskWithAffinity(ECS_THREAD_TASK_NAME(ForEachEntitySelectionSharedGroupingThreadTask, task_data, task_data_size), thread_affinity);
				thread_affinity++;
				thread_affinity = thread_affinity == thread_count ? 0 : thread_affinity;
				last_batch_entity_offset += current_batch_size;
			}
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