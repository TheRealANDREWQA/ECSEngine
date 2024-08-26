#pragma once
#include "InternalStructures.h"
#include "VectorComponentSignature.h"
#include "../Tools/Modules/ModuleDefinition.h"
#include "../Utilities/Iterator.h"

namespace ECSEngine {

	struct World;

	// -------------------------------------------------------------------------------------------------------------------------------

	// Tag type for component access
	template<typename T>
	struct QueryRead {
		using Type = T;

		ECS_INLINE constexpr static bool IsShared() {
			return T::IsShared();
		}

		ECS_INLINE constexpr static bool IsExclude() {
			return false;
		}

		ECS_INLINE constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_READ;
		}

		ECS_INLINE constexpr static bool IsOptional() {
			return false;
		}
	};

	// Tag type for component access
	template<typename T>
	struct QueryWrite {
		using Type = T;

		ECS_INLINE constexpr static bool IsShared() {
			return T::IsShared();
		}

		ECS_INLINE constexpr static bool IsExclude() {
			return false;
		}

		ECS_INLINE constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_WRITE;
		}

		ECS_INLINE constexpr static bool IsOptional() {
			return false;
		}
	};

	// Tag type for component access
	template<typename T>
	struct QueryReadWrite {
		using Type = T;

		ECS_INLINE constexpr static bool IsShared() {
			return T::IsShared();
		}

		ECS_INLINE constexpr static bool IsExclude() {
			return false;
		}

		ECS_INLINE constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_READ_WRITE;
		}

		ECS_INLINE constexpr static bool IsOptional() {
			return false;
		}
	};
	
	// Tag type for component exclusion
	template<typename T>
	struct QueryExclude {
		using Type = T;

		ECS_INLINE constexpr static bool IsShared() {
			return T::IsShared();
		}

		ECS_INLINE constexpr static bool IsExclude() {
			return true;
		}

		ECS_INLINE constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_ACCESS_TYPE_COUNT;
		}

		ECS_INLINE constexpr static bool IsOptional() {
			return false;
		}
	};

	// Tag type for component access - T should be a QueryRead<T>, QueryWrite<T> or QueryReadWrite<T>
	template<typename T>
	struct QueryOptional {
		using Type = typename T::Type;

		ECS_INLINE constexpr static bool IsShared() {
			return T::IsShared();
		}

		ECS_INLINE constexpr static bool IsExclude() {
			return false;
		}

		ECS_INLINE constexpr static ECS_ACCESS_TYPE Access() {
			return T::Access();
		}

		ECS_INLINE constexpr static bool IsOptional() {
			return true;
		}
	};

	struct ForEachEntityData {
		unsigned int thread_id;
		Entity entity;
		World* world;
		EntityManagerCommandStream* command_stream;
		void* user_data;
	};

	struct ForEachBatchData {
		unsigned int thread_id;
		unsigned short count;
		World* world;
		const Entity* entities;
		EntityManagerCommandStream* command_stream;
		void* user_data;
	};

	struct ForEachEntitySelectionData {
		unsigned int thread_id;
		// This is the index of the current entry inside the selection
		unsigned int index;
		// This is used by the for each entity selection of the shared grouping. This is the index
		// Of the entry inside the current group, while the other index is a global index.
		unsigned int in_group_index;
		Entity entity;
		World* world;
		EntityManagerCommandStream* command_stream;
		void* user_data;
		// This pointer is going to be nullptr if the given data for the ForEachEntitySelection
		// Is not stable. It will be set only if the iterator that was passed to the function is stable.
		// Useful if you want to extract other information out of the iterator.
		IteratorInterface<const Entity>* iterator;
	};

	struct ForEachEntitySelectionSharedGroupingData {
		unsigned int thread_id;
		// The index of the current group inside the ordered groups
		unsigned int group_index;
		// This is the current shared instance of the group
		SharedInstance shared_instance;
		// The entities that comprise this group
		Stream<Entity> entities;
		World* world;
		EntityManagerCommandStream* command_stream;
		void* user_data;
		// This is the initial interface pointer. It can be used to extract extra data.
		IteratorInterface<const Entity>* iterator;
	};

	struct ForEachEntityUntypedFunctorData {
		ForEachEntityData base;
		void** unique_components;
		void** shared_components;
	};

	typedef void (*ForEachEntityUntypedFunctor)(ForEachEntityUntypedFunctorData* data);

	struct ForEachBatchUntypedFunctorData {
		ForEachBatchData base;
		void** unique_components;
		void** shared_components;
	};

	typedef void (*ForEachBatchUntypedFunctor)(ForEachBatchUntypedFunctorData* data);
	
	struct ForEachEntitySelectionUntypedFunctorData {
		ForEachEntitySelectionData base;
		void** unique_components;
		void** shared_components;
	};
	
	typedef void (*ForEachEntitySelectionUntypedFunctor)(ForEachEntitySelectionUntypedFunctorData* data);

	typedef ForEachEntitySelectionSharedGroupingData ForEachEntitySelectionSharedGroupingUntypedFunctorData;

	// For initialize functors, the return is used to know whether or not the entity iteration for the group should continue
	// In case you want to abort the group. In that case, the finalize functor will not be called.
	typedef bool (*ForEachEntitySelectionSharedGroupingUntypedFunctor)(ForEachEntitySelectionSharedGroupingUntypedFunctorData* data);

	// This is the same as the ForEachEntity with the difference that it can be outside the ECS runtime. This version
	// doesn't use the archetype query acceleration. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachEntityCommitFunctor(
		World* world,
		ForEachEntityUntypedFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	);

	// This is the same as the ForEachEntity with the difference that it can be outside the ECS runtime. This version
	// doesn't use the archetype query acceleration. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachBatchCommitFunctor(
		World* world,
		ForEachBatchUntypedFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	);

	// It doesn't bring too much benefit against using a handrolled version, but it is added to allow the type safe wrapper
	// work in a commit fashion. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachEntitySelectionCommitFunctor(
		IteratorInterface<const Entity>* entities,
		World* world,
		ForEachEntitySelectionUntypedFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	);

	// It doesn't bring too much benefit against using a handrolled version, but it is added to allow the type safe wrapper
	// work in a commit fashion. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachEntitySelectionSharedGroupingCommitFunctor(
		IteratorInterface<const Entity>* entities,
		bool allow_missing_component_group,
		World* world,
		ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_initialize_functor,
		ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_finalize_functor,
		ForEachEntitySelectionUntypedFunctor entity_functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	);

	template<typename Functor>
	ECS_INLINE void ForEachEntityCommitFunctor(
		World* world,
		const ArchetypeQueryDescriptor& query_descriptor,
		Functor&& functor
	) {
		auto functor_wrapper = [](ForEachEntityUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->base.user_data;
			(*functor)(for_each_data);
		};

		ForEachEntityCommitFunctor(world, functor_wrapper, &functor, query_descriptor);
	}

	template<typename Functor>
	ECS_INLINE void ForEachBatchCommitFunctor(
		World* world,
		const ArchetypeQueryDescriptor& query_descriptor,
		Functor&& functor
	) {
		auto functor_wrapper = [](ForEachBatchUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->base.user_data;
			(*functor)(for_each_data);
		};

		ForEachBatchCommitFunctor(world, functor_wrapper, &functor, query_descriptor);
	}

	template<typename Functor>
	ECS_INLINE void ForEachEntitySelectionCommitFunctor(IteratorInterface<const Entity>* entities, World* world, const ArchetypeQueryDescriptor& query_descriptor, Functor&& functor) {
		auto functor_wrapper = [](ForEachEntitySelectionUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->base.user_data;
			(*functor)(for_each_data);
		};

		ForEachEntitySelectionCommitFunctor(entities, world, functor_wrapper, &functor, query_descriptor);
	}
	
	// The Functor type must be a struct with 3 functions: 
	// void ForEachGroupInitialize(ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) 
	// void ForEachGroupFinalize(ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) 
	// void ForEachEntity(ForEachEntitySelectionUntypedFunctorData* for_each_data);
	// The user data pointer should be ignored for those functions.
	template<typename Functor>
	ECS_INLINE void ForEachEntitySelectionSharedGroupingCommitFunctor(
		IteratorInterface<const Entity>* entities,
		bool allow_missing_component_group,
		World* world,
		const ArchetypeQueryDescriptor& query_descriptor,
		Functor&& functor
	) {
		auto group_initialize_functor_wrapper = [](ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->user_data;
			functor->ForEachGroupInitialize(for_each_data);
		};

		auto group_finalize_functor_wrapper = [](ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->user_data;
			functor->ForEachGroupFinalize(for_each_data);
		};

		auto entity_functor_wrapper = [](ForEachEntitySelectionUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->base.user_data;
			functor->ForEachEntity(for_each_data);
		};

		ForEachEntitySelectionSharedGroupingCommitFunctor(entities, allow_missing_component_group, world, group_initialize_functor_wrapper, group_finalize_functor_wrapper, entity_functor_wrapper, &functor, query_descriptor);
	}

	static bool ForEachRunAlways(unsigned int thread_id, World* world, void* user_data) {
		return true;
	}

	typedef bool (*ForEachCondition)(unsigned int thread_id, World* world, void* user_data);

	// Extra options that can be passed to the ForEach type safe wrappers
	struct ForEachOptions {
		ForEachCondition condition = ForEachRunAlways;
		unsigned int deferred_action_capacity = 0;
	};

	// Extra options that can be passed to the ForEachSelection type safe wrapper
	struct ForEachSelectionOptions {
		ForEachCondition condition = ForEachRunAlways;
		unsigned int deferred_action_capacity = 0;
		unsigned short batch_size = 0;
	};

	namespace Internal {
		// If data size is 0, it will simply reference the pointer. If data size is greater than 0, it will copy it.
		// If you plan on doing deferred calls, then set the deferred_action_capacity to a value large enough to encompass all
		// the calls you want to make. The value should be per batch (each batch will receive its own command stream).
		// The runtime will allocate memory for that, you don't need to free it
		ECSENGINE_API void ForEachEntity(
			unsigned int thread_id,
			World* world,
			ForEachEntityUntypedFunctor functor,
			const char* functor_name,
			void* data,
			size_t data_size,
			ComponentSignature optional_signature,
			ComponentSignature optional_shared_signature,
			unsigned int deferred_action_capacity = 0
		);

		// -------------------------------------------------------------------------------------------------------------------------------

		// If data size is 0, it will simply reference the pointer. If data size is greater than 0, it will copy it.
		// If you plan on doing deferred calls, then set the deferred_action_capacity to a value large enough to encompass all
		// the calls you want to make. The value should be per batch (each batch will receive its own command stream).
		// The runtime will allocate memory for that, you don't need to free it
		ECSENGINE_API void ForEachBatch(
			unsigned int thread_id,
			World* world,
			ForEachBatchUntypedFunctor functor,
			const char* functor_name,
			void* data,
			size_t data_size,
			ComponentSignature optional_signature,
			ComponentSignature optional_shared_signature,
			unsigned int deferred_action_capacity = 0
		);

		// -------------------------------------------------------------------------------------------------------------------------------

		// Iterates over the given entities with the specified components. The advantage is that the query dependencies are properly registered
		// And that it will parallelize the functor by default. The last parameter, the batch_size, can be used to tell the runtime how many
		// Entities each individual parallel task should have. In most cases, you do not need to specify this value, but in case you want
		// Very few entries per task (if each task unit takes a long time) or many (in order to reduce the number of tasks to be spawned)
		// At the moment, exclude queries are not supported. The boolean are_entities_stable is a fast path that allows the function to
		// Not copy the given entities in case they are already stable (and do not change after this call)
		ECSENGINE_API void ForEachEntitySelection(
			IteratorInterface<const Entity>* entities,
			bool are_entities_stable,
			unsigned int thread_id,
			World* world,
			ForEachEntitySelectionUntypedFunctor functor,
			const char* functor_name,
			void* data,
			size_t data_size,
			const ArchetypeQueryDescriptor& query_descriptor,
			unsigned int deferred_action_capacity = 0,
			unsigned short batch_size = 0
		);

		// Iterates over the given entities with the specified components, while groupping them based on the first shared component that appears in the query descriptor. 
		// The advantage is that the query dependencies are properly registered and that it will parallelize the functor by default. 
		// The last parameter, the batch_size, can be used to tell the runtime how many entities each individual parallel task should have. 
		// In most cases, you do not need to specify this value, but in case you want very few entries per task (if each task unit takes a long time) 
		// Or many (in order to reduce the number of tasks to be spawned). At the moment, exclude queries are not supported. The last mandatory flag is used
		// To know whether or not it should accept entities with the component missing, in that case grouping them together. The index field of the
		// ForEachEntitySelectionUntypedFunctorData is not valid, it should not be used.
		ECSENGINE_API void ForEachEntitySelectionSharedGrouping(
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
			bool allow_missing_component,
			unsigned int deferred_action_capacity = 0,
			unsigned short batch_size = 0
		);

		// -------------------------------------------------------------------------------------------------------------------------------

		struct RegisterForEachInfo {
			ModuleTaskFunctionData* module_data;
			TaskComponentQuery* query;
		};

		template<typename... Components>
		void AddQueryComponents(RegisterForEachInfo* info) {
			constexpr size_t component_count = sizeof...(Components);
			
			if constexpr (component_count > 0) {
				constexpr bool is_shared[component_count] = { Components::IsShared()... };
				constexpr bool is_excluded[component_count] = { Components::IsExclude()... };
				constexpr ECS_ACCESS_TYPE access_type[component_count] = { Components::Access()... };
				constexpr bool is_optional[component_count] = { Components::IsOptional()... };

				Component component[component_count] = { Components::Type::ID()... };

				for (size_t index = 0; index < component_count; index++) {
					if (is_shared[index]) {
						if (is_excluded[index]) {
							info->query->AddSharedComponentExclude(component[index], info->module_data->allocator);
						}
						else {
							if (is_optional[index]) {
								info->query->AddOptionalSharedComponent(component[index], access_type[index], info->module_data->allocator);
							}
							else {
								info->query->AddSharedComponent(component[index], access_type[index], info->module_data->allocator);
							}
						}
					}
					else {
						if (is_excluded[index]) {
							info->query->AddComponentExclude(component[index], info->module_data->allocator);
						}
						else {
							if (is_optional[index]) {
								info->query->AddOptionalComponent(component[index], access_type[index], info->module_data->allocator);
							}
							else {
								info->query->AddComponent(component[index], access_type[index], info->module_data->allocator);
							}
						}
					}
				}
			}
		}
		
		template<typename Functor, typename Miscellaneous>
		struct ForEachTypeSafeWrapperData {
			Functor functor;
			void* functor_data;
			// Can be used to inject more data
			Miscellaneous miscellaneous;
		};

		struct EmptyMisc {};

		// DataType must be ForEachEntityUntypedFunctorData, ForEachBatchUntypedFunctorData or ForEachEntitySelectionUntypedFunctorData
		template<typename DataType, typename Functor, typename... T>
		void ForEachEntityBatchTypeSafeWrapper(DataType* data) {
			// Here it doesn't matter what the misc type is, since we are not using it
			ForEachTypeSafeWrapperData<Functor, EmptyMisc>* wrapper_data = (ForEachTypeSafeWrapperData<Functor, EmptyMisc>*)data->base.user_data;
			
			constexpr size_t component_count = sizeof...(T);
			
			constexpr bool is_shared[component_count] = {
				T::IsShared()...
			};

			void** components[2] = {
				data->unique_components,
				data->shared_components
			};
			void* current_components[component_count];

			size_t total_index = 0;
			size_t component_index[2] = { 0 };

			for (size_t index = 0; index < component_count; index++) {
				void** component_ptrs = components[is_shared[index]];
				size_t ptr_index = component_index[is_shared[index]]++;
				current_components[total_index++] = component_ptrs[ptr_index];
			}

			// We need to decrement index to reflect the correct final index
			total_index--;
			// Need to change the user data and then restore it back
			data->base.user_data = wrapper_data->functor_data;
			wrapper_data->functor(&data->base, (typename T::Type*)(current_components[total_index--])...);
			data->base.user_data = wrapper_data;
		}

		template<typename... Components>
		void GetComponentSignatureFromTemplatePack(
			ComponentSignature& unique_components, 
			ComponentSignature& shared_components,
			ComponentSignature& unique_exclude,
			ComponentSignature& shared_exclude,
			ComponentSignature& optional_unique_components,
			ComponentSignature& optional_shared_components
		) {
			constexpr size_t count = sizeof...(Components);
			
			unique_components.count = 0;
			shared_components.count = 0;
			unique_exclude.count = 0;
			shared_exclude.count = 0;
			optional_unique_components.count = 0;
			optional_shared_components.count = 0;
			if constexpr (count > 0) {
				constexpr bool is_shared[count] = {
					Components::IsShared()...
				};
				constexpr short component_ids[count] = {
					Components::Type::ID()...
				};
				constexpr bool is_optional[count] = {
					Components::IsOptional()...
				};
				constexpr bool is_exclude[count] = {
					Components::IsExclude()...
				};

				for (size_t index = 0; index < count; index++) {
					if (is_exclude[index]) {
						if (is_shared[index]) {
							shared_exclude[shared_exclude.count++] = { component_ids[index] };
						}
						else {
							unique_exclude[unique_exclude.count++] = { component_ids[index] };
						}
					}
					else {
						if (is_shared[index]) {
							if (is_optional[index]) {
								optional_shared_components[optional_shared_components.count++] = { component_ids[index] };
							}
							else {
								shared_components[shared_components.count++] = { component_ids[index] };
							}
						}
						else {
							if (is_optional[index]) {
								optional_unique_components[optional_unique_components.count++] = { component_ids[index] };
							}
							else {
								unique_components[unique_components.count++] = { component_ids[index] };
							}
						}
					}
				}
			}
		}

		enum FOR_EACH_OPTIONS : unsigned char {
			FOR_EACH_NONE = 0,
			FOR_EACH_IS_BATCH = 1 << 0,
			FOR_EACH_IS_COMMIT = 1 << 1
		};

		ECSENGINE_API void IncrementWorldQueryIndex(World* world);

		// Retrieves the components from the parameter pack and writes them into a ArchetypeQueryDescriptor
		// For which you must specify the name and the template parameter name
#define GET_COMPONENT_SIGNATURE_FROM_TEMPLATE_PACK(query_descriptor_name, template_pack_name) \
		/* Retrieve the optional components since they are not stored in the query cache */ \
		Component __unique_components[ECS_ARCHETYPE_MAX_COMPONENTS]; \
		Component __shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS]; \
		Component __unique_exclude_components[ECS_ARCHETYPE_MAX_COMPONENTS]; \
		Component __shared_exclude_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS]; \
		Component __optional_components[ECS_ARCHETYPE_MAX_COMPONENTS]; \
		Component __optional_shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS]; \
\
		ArchetypeQueryDescriptor query_descriptor_name; \
		query_descriptor_name.unique = { __unique_components, 0 }; \
		query_descriptor_name.shared = { __shared_components, 0 }; \
		query_descriptor_name.unique_exclude = { __unique_exclude_components, 0 }; \
		query_descriptor_name.shared_exclude = { __shared_exclude_components, 0 }; \
		query_descriptor_name.unique_optional = { __optional_components, 0 }; \
		query_descriptor_name.shared_optional = { __optional_shared_components, 0 }; \
\
		GetComponentSignatureFromTemplatePack<template_pack_name...>( \
			query_descriptor_name.unique, \
			query_descriptor_name.shared, \
			query_descriptor_name.unique_exclude, \
			query_descriptor_name.shared_exclude, \
			query_descriptor_name.unique_optional, \
			query_descriptor_name.shared_optional \
		); \
\
		ECS_CRASH_CONDITION(query_descriptor_name.unique_exclude.count == 0 && query_descriptor_name.shared_exclude.count == 0, "ECS ForEach:" \
			" You must specify the exclude components in the Function template parameter pack");

		template<bool get_query, FOR_EACH_OPTIONS type_options, typename... Components>
		struct ForEachEntityOrBatch {
			ForEachEntityOrBatch(unsigned int _thread_id, World* _world, ForEachOptions _options = {}, const char* _function_name = ECS_FUNCTION)
				: thread_id(_thread_id), world(_world), function_name(_function_name), options(_options) {
				if constexpr (get_query) {
					RegisterForEachInfo* info = (RegisterForEachInfo*)world;
					AddQueryComponents<Components...>(info);
				}
				else {
					//if constexpr (options & FOR_EACH_IS_COMMIT) {
						IncrementWorldQueryIndex(world);
					//}
				}
			}

			// The fact that lambdas are not allowed is intentional, because of the reason that lambdas
			// Provide ugly stack traces when an error occurs in them, and in order to have reasonable
			// stack traces, named function pointers with accompanying data is the preferred way.
			template<typename... ExcludeComponents, typename Functor>
			void Function(
				Functor functor,
				void* function_pointer_data = nullptr, 
				size_t function_pointer_data_size = 0
			) {
				if constexpr (!get_query) {
					if (!options.condition(thread_id, world, function_pointer_data)) {
						return;
					}

					GET_COMPONENT_SIGNATURE_FROM_TEMPLATE_PACK(query_descriptor, Components);

					ForEachTypeSafeWrapperData<Functor, EmptyMisc> wrapper_data{ functor, function_pointer_data };
					if constexpr (~type_options & FOR_EACH_IS_COMMIT) {
						if (function_pointer_data_size > 0) {
							// We need to allocate this data, we can use the thread task manager allocator for this
							wrapper_data.functor_data = world->task_manager->AllocateTempBuffer(thread_id, function_pointer_data_size);
							memcpy(wrapper_data.functor_data, function_pointer_data, function_pointer_data_size);
						}
					}
					if constexpr (type_options & FOR_EACH_IS_COMMIT) {
						if constexpr (type_options & FOR_EACH_IS_BATCH) {
							ForEachBatchCommitFunctor(
								world,
								ForEachEntityBatchTypeSafeWrapper<ForEachBatchUntypedFunctorData, Functor, Components...>,
								&wrapper_data,
								query_descriptor
							);
						}
						else {
							ForEachEntityCommitFunctor(
								world,
								ForEachEntityBatchTypeSafeWrapper<ForEachEntityUntypedFunctorData, Functor, Components...>,
								&wrapper_data,
								query_descriptor
							);
						}
					}
					else {
						if constexpr (type_options & FOR_EACH_IS_BATCH) {
							ForEachBatch(
								thread_id,
								world,
								ForEachEntityBatchTypeSafeWrapper<ForEachBatchUntypedFunctorData, Functor, Components...>,
								function_name,
								&wrapper_data,
								sizeof(wrapper_data),
								query_descriptor.unique_optional,
								query_descriptor.shared_optional,
								options.deferred_action_capacity
							);
						}
						else {
							ForEachEntity(
								thread_id,
								world,
								ForEachEntityBatchTypeSafeWrapper<ForEachEntityUntypedFunctorData, Functor, Components...>,
								function_name,
								&wrapper_data,
								sizeof(wrapper_data),
								query_descriptor.unique_optional,
								query_descriptor.shared_optional,
								options.deferred_action_capacity
							);
						}
					}
				}
				else {
					RegisterForEachInfo* info = (RegisterForEachInfo*)world;
					AddQueryComponents<ExcludeComponents...>(info);
				}
			}

			unsigned int thread_id;
			World* world;
			const char* function_name;
			ForEachOptions options;
		};

		template<bool get_query, bool is_commit, typename... Components>
		struct ForEachEntitySelectionTypeSafe {
			ECS_INLINE ForEachEntitySelectionTypeSafe(
				IteratorInterface<const Entity>* _entities,
				bool _are_entities_stable,
				unsigned int _thread_id,
				World* _world,
				ForEachSelectionOptions _options = {},
				const char* _function_name = ECS_FUNCTION
			) : thread_id(_thread_id), world(_world), function_name(_function_name), options(_options), entities(_entities), are_entities_stable(_are_entities_stable) {
				if constexpr (get_query) {
					RegisterForEachInfo* info = (RegisterForEachInfo*)world;
					AddQueryComponents<Components...>(info);
				}
			}

			// The fact that lambdas are not allowed is intentional, because of the reason that lambdas
			// Provide ugly stack traces when an error occurs in them, and in order to have reasonable
			// stack traces, named function pointers with accompanying data is the preferred way.
			template<typename Functor>
			void Function(
				Functor functor,
				void* function_pointer_data = nullptr,
				size_t function_pointer_data_size = 0
			) {
				if constexpr (!get_query) {
					if (!options.condition(thread_id, world, function_pointer_data)) {
						return;
					}

					GET_COMPONENT_SIGNATURE_FROM_TEMPLATE_PACK(query_descriptor, Components);


					ForEachTypeSafeWrapperData<Functor, EmptyMisc> wrapper_data{ functor, function_pointer_data };

					if constexpr (!is_commit) {
						if (function_pointer_data_size > 0) {
							// We need to allocate this data, we can use the thread task manager allocator for this
							wrapper_data.functor_data = world->task_manager->AllocateTempBuffer(thread_id, function_pointer_data_size);
							memcpy(wrapper_data.functor_data, function_pointer_data, function_pointer_data_size);
						}

						ForEachEntitySelection(
							entities,
							thread_id,
							world,
							ForEachEntityBatchTypeSafeWrapper<ForEachEntitySelectionUntypedFunctorData, Functor, Components...>,
							function_name,
							&wrapper_data,
							sizeof(wrapper_data),
							query_descriptor,
							options.deferred_action_capacity,
							options.batch_size
						);
					}
					else {
						ForEachEntitySelectionCommitFunctor(
							entities, 
							world, 
							ForEachEntityBatchTypeSafeWrapper<ForEachEntitySelectionUntypedFunctorData, Functor, Components...>,
							&wrapper_data, 
							query_descriptor
						);
					}
				}
			}

			unsigned int thread_id;
			bool are_entities_stable;
			World* world;
			const char* function_name;
			IteratorInterface<const Entity>* entities;
			ForEachSelectionOptions options;
		};

		// Shared component must be a Query shared component type
		// Exclude components are not allowed.
		template<bool get_query, bool is_commit, typename SharedComponent, typename... Components>
		struct ForEachEntitySelectionSharedGroupingTypeSafe {
			static_assert(SharedComponent::IsShared(), "ForEachEntitySelectionSharedGrouping: Must use a shared component as grouping!");

			ECS_INLINE ForEachEntitySelectionSharedGroupingTypeSafe(
				IteratorInterface<const Entity>* _entities,
				unsigned int _thread_id,
				World* _world,
				ForEachSelectionOptions _options = {},
				const char* _function_name = ECS_FUNCTION
			) : thread_id(_thread_id), world(_world), function_name(_function_name), options(_options), entities(_entities) {
				if constexpr (get_query) {
					RegisterForEachInfo* info = (RegisterForEachInfo*)world;
					AddQueryComponents<SharedComponent, SharedComponent, Components...>(info);
				}
			}

			// The fact that lambdas are not allowed is intentional, because of the reason that lambdas
			// Provide ugly stack traces when an error occurs in them, and in order to have reasonable
			// stack traces, named function pointers with accompanying data is the preferred way.
			// There are 2 group functors, one that is called before the entity functor and one that is called after the
			// Entity functor.
			template<typename EntityFunctor>
			void Function(
				ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_initialize_functor,
				ForEachEntitySelectionSharedGroupingUntypedFunctor grouping_finalize_functor,
				EntityFunctor entity_functor,
				void* function_pointer_data = nullptr,
				size_t function_pointer_data_size = 0
			) {
				if constexpr (!get_query) {
					if (!options.condition(thread_id, world, function_pointer_data)) {
						return;
					}

					GET_COMPONENT_SIGNATURE_FROM_TEMPLATE_PACK(query_descriptor, Components);

					// Must insert the shared component at the very beginning
					bool allow_missing_component_group = SharedComponent::IsOptional();
					if (allow_missing_component_group) {
						ECS_CRASH_CONDITION(query_descriptor.shared_optional.count < ECS_ARCHETYPE_MAX_SHARED_COMPONENTS, "ForEachEntitySelectionSharedGrouping: Too many shared optional components!");
						memmove(query_descriptor.shared_optional.indices + 1, query_descriptor.shared_optional.indices, sizeof(*query_descriptor.shared_optional.indices) * query_descriptor.shared_optional.count);
						query_descriptor.shared_optional.indices[0] = SharedComponent::Type::ID();
						query_descriptor.shared_optional.count++;
					}
					else {
						ECS_CRASH_CONDITION(query_descriptor.shared.count < ECS_ARCHETYPE_MAX_SHARED_COMPONENTS, "ForEachEntitySelectionSharedGrouping: Too many shared components!");
						memmove(query_descriptor.shared.indices + 1, query_descriptor.shared.indices, sizeof(*query_descriptor.shared.indices) * query_descriptor.shared.count);
						query_descriptor.shared.indices[0] = SharedComponent::Type::ID();
						query_descriptor.shared.count++;
					}

					struct MiscellaneousData {
						ForEachEntitySelectionSharedGroupingUntypedFunctor initialize_functor;
						ForEachEntitySelectionSharedGroupingUntypedFunctor finalize_functor;
					};

					ForEachTypeSafeWrapperData<EntityFunctor, MiscellaneousData> wrapper_data{ entity_functor, function_pointer_data, { grouping_initialize_functor, grouping_finalize_functor } };
					auto grouping_initialize_wrapper = [](ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) {
						ForEachTypeSafeWrapperData<EntityFunctor, MiscellaneousData>* data = (ForEachTypeSafeWrapperData<EntityFunctor, MiscellaneousData>*)for_each_data->user_data;
						// Need to change the user data and then restore it back
						for_each_data->user_data = data->functor_data;
						bool return_value = data->miscellaneous.initialize_functor(for_each_data);
						for_each_data->user_data = data;
						return return_value;
					};

					auto grouping_finalize_wrapper = [](ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) {
						ForEachTypeSafeWrapperData<EntityFunctor, MiscellaneousData>* data = (ForEachTypeSafeWrapperData<EntityFunctor, MiscellaneousData>*)for_each_data->user_data;
						// Need to change the user data and then restore it back
						for_each_data->user_data = data->functor_data;
						bool return_value = data->miscellaneous.finalize_functor(for_each_data);
						for_each_data->user_data = data;
						return return_value;
					};

					if constexpr (!is_commit) {
						if (function_pointer_data_size > 0) {
							// We need to allocate this data, we can use the thread task manager allocator for this
							wrapper_data.functor_data = world->task_manager->AllocateTempBuffer(thread_id, function_pointer_data_size);
							memcpy(wrapper_data.functor_data, function_pointer_data, function_pointer_data_size);
						}

						ForEachEntitySelectionSharedGrouping(
							entities,
							thread_id,
							world,
							grouping_initialize_wrapper,
							grouping_finalize_wrapper,
							ForEachEntityBatchTypeSafeWrapper<ForEachEntitySelectionUntypedFunctorData, EntityFunctor, SharedComponent, Components...>,
							function_name,
							&wrapper_data,
							sizeof(wrapper_data),
							query_descriptor,
							allow_missing_component_group,
							options.deferred_action_capacity,
							options.batch_size
						);
					}
					else {
						ForEachEntitySelectionSharedGroupingCommitFunctor(
							entities,
							allow_missing_component_group,
							world,
							grouping_initialize_wrapper,
							grouping_finalize_wrapper,
							ForEachEntityBatchTypeSafeWrapper<ForEachEntitySelectionUntypedFunctorData, EntityFunctor, SharedComponent, Components...>,
							&wrapper_data,
							query_descriptor
						);
					}
				}
			}

			unsigned int thread_id;
			World* world;
			const char* function_name;
			IteratorInterface<const Entity>* entities;
			ForEachSelectionOptions options;
		};

	}

	template<bool get_query, typename... Components>
	using ForEachEntity = Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_NONE, Components...>;

	template<bool get_query, typename... Components>
	using ForEachEntityCommit = Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_IS_COMMIT, Components...>;

	template<bool get_query, typename... Components>
	using ForEachBatch = Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_IS_BATCH, Components...>;

	template<bool get_query, typename... Components>
	using ForEachBatchCommit = Internal::ForEachEntityOrBatch<get_query, (Internal::FOR_EACH_OPTIONS)(Internal::FOR_EACH_IS_BATCH | Internal::FOR_EACH_IS_COMMIT), Components...>;

	template<bool get_query, typename... Components>
	using ForEachEntitySelection = Internal::ForEachEntitySelectionTypeSafe<get_query, false, Components...>;

	template<bool get_query, typename... Components>
	using ForEachEntitySelectionCommit = Internal::ForEachEntitySelectionTypeSafe<get_query, true, Components...>;

	template<bool get_query, typename SharedComponent, typename... Components>
	using ForEachEntitySelectionSharedGrouping = Internal::ForEachEntitySelectionSharedGroupingTypeSafe<get_query, false, SharedComponent, Components...>;

	template<bool get_query, typename SharedComponent, typename... Components>
	using ForEachEntitySelectionSharedGroupingCommit = Internal::ForEachEntitySelectionSharedGroupingTypeSafe<get_query, true, SharedComponent, Components...>;

	// Can be used for EntitySelection as well
#define ECS_REGISTER_FOR_EACH_TASK(schedule_element, thread_task_function, module_function_data)	\
	ECSEngine::Internal::RegisterForEachInfo __register_info##thread_task_function; \
	__register_info##thread_task_function.module_data = module_function_data; \
	__register_info##thread_task_function.query = &schedule_element.component_query; \
	thread_task_function<true>(0, (ECSEngine::World*)&__register_info##thread_task_function, nullptr); \
	schedule_element.task_function = thread_task_function<false>; \
	schedule_element.task_name = STRING(thread_task_function); \
	module_function_data->tasks->AddAssert(&schedule_element)

	// This is a helper macro that eases the use of the schedule element
	// The last parameter, dependencies, is a Stream<TaskDependency>
#define ECS_REGISTER_SIMPLE_FOR_EACH_TASK(module_function_data, thread_task_function, task_group_value, dependencies)	\
	ECSEngine::TaskSchedulerElement __task_element##thread_task_function; \
	__task_element##thread_task_function.task_group = task_group_value; \
	__task_element##thread_task_function.task_dependencies = module_function_data->AllocateAndSetDependencies(dependencies); \
	ECS_REGISTER_FOR_EACH_TASK(__task_element##thread_task_function, thread_task_function, module_function_data)

#define ECS_REGISTER_FOR_EACH_COMPONENTS(...) Internal::AddQueryComponents<__VA_ARGS__>((Internal::RegisterForEachInfo*)world);

	// -------------------------------------------------------------------------------------------------------------------------------

//	typedef void (*ForEachBaseArchetypeFunctor)(ArchetypeBase* base, void** unique_components, void** shared_components, void* data);
//
//#define ECS_FOR_EACH_BASE_ARCHETYPE(name) void name(unsigned int chunk_count, Entity** entities, unsigned int* chunk_sizes, void*** unique_components, void** shared_components, void* data)
//
//	// The iteration order for the unique components is the chunk_index then the unique component index:
//	// example unique_components[chunk_index][unique_component_index]
//	ECSENGINE_API void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQuery query, void* data, ForEachBaseArchetypeFunctor functor);
//
//	// The iteration order for the unique components is the chunk_index then the unique component index:
//	// example unique_components[chunk_index][unique_component_index]
//	ECSENGINE_API void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQueryExclude query, void* data, ForEachBaseArchetypeFunctor functor);
//
//	// -------------------------------------------------------------------------------------------------------------------------------
//
//	typedef void (*ForEachSharedInstanceFunctor)(unsigned short archetype_index, unsigned short base_archetype_index, void* data);
//
//#define ECS_FOR_EACH_SHARED_INSTANCE(name) void name(unsigned short archetype_index, unsigned short base_archetype_index, void* data)
//
//	ECSENGINE_API void ForEachSharedInstance(EntityManager* entity_manager, SharedComponentSignature shared_signature, void* data, ForEachSharedInstanceFunctor functor);
//
//	// -------------------------------------------------------------------------------------------------------------------------------
}