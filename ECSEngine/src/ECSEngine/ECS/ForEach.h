#pragma once
#include "InternalStructures.h"
#include "VectorComponentSignature.h"
#include "../Tools/Modules/ModuleDefinition.h"

namespace ECSEngine {

	struct World;

	// -------------------------------------------------------------------------------------------------------------------------------

	// Tag type for component access
	template<typename T>
	struct QueryRead {
		using Type = T;

		constexpr static bool IsShared() {
			return T::IsShared();
		}

		constexpr static bool IsExclude() {
			return false;
		}

		constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_READ;
		}

		constexpr static bool IsOptional() {
			return false;
		}
	};

	// Tag type for component access
	template<typename T>
	struct QueryWrite {
		using Type = T;

		constexpr static bool IsShared() {
			return T::IsShared();
		}

		constexpr static bool IsExclude() {
			return false;
		}

		constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_WRITE;
		}

		constexpr static bool IsOptional() {
			return false;
		}
	};

	// Tag type for component access
	template<typename T>
	struct QueryReadWrite {
		using Type = T;

		constexpr static bool IsShared() {
			return T::IsShared();
		}

		constexpr static bool IsExclude() {
			return false;
		}

		constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_READ_WRITE;
		}

		constexpr static bool IsOptional() {
			return false;
		}
	};
	
	// Tag type for component exclusion
	template<typename T>
	struct QueryExclude {
		using Type = T;

		constexpr static bool IsShared() {
			return T::IsShared();
		}

		constexpr static bool IsExclude() {
			return true;
		}

		constexpr static ECS_ACCESS_TYPE Access() {
			return ECS_ACCESS_TYPE_COUNT;
		}

		constexpr static bool IsOptional() {
			return false;
		}
	};

	// Tag type for component access - T should be a QueryRead<T>, QueryWrite<T> or QueryReadWrite<T>
	template<typename T>
	struct QueryOptional {
		using Type = typename T::Type;

		constexpr static bool IsShared() {
			return T::IsShared();
		}

		constexpr static bool IsExclude() {
			return false;
		}

		constexpr static ECS_ACCESS_TYPE Access() {
			return T::Access();
		}

		constexpr static bool IsOptional() {
			return true;
		}
	};

	struct ForEachEntityUntypedFunctorData {
		unsigned int thread_id;
		Entity entity;
		World* world;
		void** unique_components;
		void** shared_components;
		void* data;
		EntityManagerCommandStream* command_stream;
	};

	typedef void (*ForEachEntityUntypedFunctor)(ForEachEntityUntypedFunctorData* data);

	struct ForEachBatchUntypedFunctorData {
		unsigned int thread_id;
		World* world;
		const Entity* entities;
		unsigned short count;
		void** unique_components;
		void** shared_components;
		void* data;
		EntityManagerCommandStream* command_stream;
	};

	typedef void (*ForEachBatchUntypedFunctor)(ForEachBatchUntypedFunctorData* data);
	
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
		unsigned short batch_size,
		const ArchetypeQueryDescriptor& query_descriptor
	);

	// TODO: This accepts lambdas at the moment, function pointer support is not decided yet
	template<typename Functor>
	ECS_INLINE void ForEachEntityCommitFunctor(
		World* world,
		const ArchetypeQueryDescriptor& query_descriptor,
		Functor functor
	) {
		auto functor_wrapper = [](ForEachEntityUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->data;
			(*functor)(for_each_data);
		};

		ForEachEntityCommitFunctor(world, functor_wrapper, &functor, query_descriptor);
	}

	// TODO: This accepts lambdas at the moment, function pointer support is not decided yet
	template<typename Functor>
	ECS_INLINE void ForEachBatchCommitFunctor(
		World* world,
		unsigned short batch_size,
		const ArchetypeQueryDescriptor& query_descriptor,
		Functor functor
	) {
		auto functor_wrapper = [](ForEachBatchUntypedFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->data;
			(*functor)(for_each_data);
		};

		ForEachBatchCommitFunctor(world, functor_wrapper, &functor, batch_size, query_descriptor);
	}

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

		static ForEachEntityData FromFunctorToEntityData(ForEachEntityUntypedFunctorData* data, void* user_data) {
			ForEachEntityData for_each_data;

			for_each_data.command_stream = data->command_stream;
			for_each_data.entity = data->entity;
			for_each_data.thread_id = data->thread_id;
			for_each_data.world = data->world;
			for_each_data.user_data = user_data;

			return for_each_data;
		}

		static ForEachBatchData FromFunctorToBatchData(ForEachBatchUntypedFunctorData* data, void* user_data) {
			ForEachBatchData for_each_data;

			for_each_data.command_stream = data->command_stream;
			for_each_data.entities = data->entities;
			for_each_data.thread_id = data->thread_id;
			for_each_data.world = data->world;
			for_each_data.count = data->count;
			for_each_data.user_data = user_data;

			return for_each_data;
		}
		
		template<typename Functor>
		struct ForEachEntityBatchTypeSafeWrapperData {
			Functor functor;
			void* functor_data;
		};

		template<bool is_batch, typename Functor, typename... T>
		void ForEachEntityBatchTypeSafeWrapper(ForEachEntityUntypedFunctorData* data) {
			ForEachEntityBatchTypeSafeWrapperData<Functor>* wrapper_data = (ForEachEntityBatchTypeSafeWrapperData<Functor>*)data->data;
			
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
			
			if constexpr (is_batch) {
				ForEachBatchData for_each_data = FromFunctorToBatchData(data, wrapper_data->functor_data);
				wrapper_data->functor(&for_each_data, (typename T::Type*)(current_components[total_index--])...);
			}
			else {
				ForEachEntityData for_each_data = FromFunctorToEntityData(data, wrapper_data->functor_data);
				wrapper_data->functor(&for_each_data, (typename T::Type*)(current_components[total_index--])...);
			}
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

		template<bool get_query, FOR_EACH_OPTIONS options, typename... Components>
		struct ForEachEntityOrBatch {
			ForEachEntityOrBatch(unsigned int _thread_id, World* _world, const char* _function_name = ECS_FUNCTION) : thread_id(_thread_id), world(_world), function_name(_function_name) {
				if constexpr (get_query) {
					Internal::RegisterForEachInfo* info = (Internal::RegisterForEachInfo*)world;
					Internal::AddQueryComponents<Components...>(info);
				}
				else {
					//if constexpr (options & FOR_EACH_IS_COMMIT) {
						IncrementWorldQueryIndex(world);
					//}
				}
			}

			// Here functor can be a self contained lambda (or functor struct) or a function pointer
			// for which you can supply additional data in the default parameter function_pointer_data
			// The use of lambdas is convenient when you want to do some single threaded preparation
			// before hand and only reference the values from inside the functor but it has a big drawback
			// The stack trace contains a mangled name that is very hard to pinpoint in source code which
			// code actually crashed or produced a problem. The recommendation is to use named function pointers
			// that you pass to the function with their required data to operate. Even tho it is more tedious,
			// the fact that when a crash happens you don't know exactly where that is from looking at a stack trace
			// is extremely annoying and can eat up a lot of time
			template<typename... ExcludeComponents, typename Functor>
			void Function(Functor functor, void* function_pointer_data = nullptr, size_t function_pointer_data_size = 0) {
				if constexpr (!get_query) {
					// Retrieve the optional components since they are not stored in the query cache
					Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					Component shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
					Component unique_exclude_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					Component shared_exclude_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
					Component optional_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					Component optional_shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

					ArchetypeQueryDescriptor query_descriptor;
					query_descriptor.unique = { unique_components, 0 };
					query_descriptor.shared = { shared_components, 0 };
					query_descriptor.unique_exclude = { unique_exclude_components, 0 };
					query_descriptor.shared_exclude = { shared_exclude_components, 0 };
					query_descriptor.unique_optional = { optional_components, 0 };
					query_descriptor.shared_optional = { optional_shared_components, 0 };

					Internal::GetComponentSignatureFromTemplatePack<Components...>(
						query_descriptor.unique,
						query_descriptor.shared,
						query_descriptor.unique_exclude,
						query_descriptor.shared_exclude,
						query_descriptor.unique_optional,
						query_descriptor.shared_optional
					);

					ECS_CRASH_CONDITION(query_descriptor.unique_exclude.count == 0 && query_descriptor.shared_exclude.count == 0, "ECS ForEach:"
						" You must specify the exclude components in the Function template parameter pack");

					ForEachEntityBatchTypeSafeWrapperData<Functor> wrapper_data{ functor, function_pointer_data };
					if constexpr (~options & FOR_EACH_IS_COMMIT) {
						if (function_pointer_data_size > 0) {
							// We need to allocate this data, we can use the thread task manager allocator for this
							wrapper_data.functor_data = world->task_manager->AllocateTempBuffer(thread_id, function_pointer_data_size);
							memcpy(wrapper_data.functor_data, function_pointer_data, function_pointer_data_size);
						}
					}
					if constexpr (options & FOR_EACH_IS_COMMIT) {
						if constexpr (options & FOR_EACH_IS_BATCH) {
							ForEachBatchCommitFunctor(
								world,
								Internal::ForEachEntityBatchTypeSafeWrapper<true, Functor, Components...>,
								&wrapper_data,
								batch_size,
								query_descriptor
							);
						}
						else {
							ForEachEntityCommitFunctor(
								world,
								Internal::ForEachEntityBatchTypeSafeWrapper<false, Functor, Components...>,
								&wrapper_data,
								query_descriptor
							);
						}
					}
					else {
						if constexpr (options & FOR_EACH_IS_BATCH) {
							Internal::ForEachBatch(
								thread_id,
								world,
								Internal::ForEachEntityBatchTypeSafeWrapper<true, Functor, Components...>,
								function_name,
								&wrapper_data,
								sizeof(wrapper_data),
								query_descriptor.unique_optional,
								query_descriptor.shared_optional
							);
						}
						else {
							Internal::ForEachEntity(
								thread_id,
								world,
								Internal::ForEachEntityBatchTypeSafeWrapper<false, Functor, Components...>,
								function_name,
								&wrapper_data,
								sizeof(wrapper_data),
								query_descriptor.unique_optional,
								query_descriptor.shared_optional
							);
						}
					}
				}
				else {
					Internal::RegisterForEachInfo* info = (Internal::RegisterForEachInfo*)world;
					Internal::AddQueryComponents<ExcludeComponents...>(info);
				}
			}

			unsigned int thread_id;
			World* world;
			const char* function_name;
		};

	}

	// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
	// Runs on multiple threads. The lambda cannot capture by reference!! (This will cause the thread tasks to reference
	// stack memory from a stack frame that is invalid - if you want a piece of code to be cached across tasks, create a temporary
	// pointer to it from the thread allocator and then reference the pointer by value)
	template<bool get_query, typename... Components>
	using ForEachEntity = Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_NONE, Components...>;

	template<bool get_query, typename... Components>
	using ForEachEntityCommit = Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_IS_COMMIT, Components...>;

	template<bool get_query, typename... Components>
	using ForEachBatch = Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_IS_BATCH, Components...>;

	template<bool get_query, typename... Components>
	using ForEachBatchCommit = Internal::ForEachEntityOrBatch<get_query, (Internal::FOR_EACH_OPTIONS)(Internal::FOR_EACH_IS_BATCH | Internal::FOR_EACH_IS_COMMIT), Components...>;


	//// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
	//// Runs on multiple threads. The lambda cannot capture by reference!! (This will cause the thread tasks to reference
	//// stack memory from a stack frame that is invalid - if you want a piece of code to be cached across tasks, create a temporary
	//// pointer to it from the thread allocator and then reference the pointer by value)
	//template<bool get_query, typename... Components>
	//struct ForEachEntity {
	//	ECS_INLINE ForEachEntity(unsigned int thread_id, World* world, const char* function_name = ECS_FUNCTION) : internal_(thread_id, world, function_name) {}

	//	// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
	//	template< typename Functor>
	//	ECS_INLINE void Function(Functor& functor) {
	//		internal_.Function(functor);
	//	}

	//	Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_NONE, Components...> internal_;
	//};

	//// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
	//template<bool get_query, typename... Components>
	//struct ForEachEntityCommit {
	//	ECS_INLINE ForEachEntityCommit(unsigned int thread_id, World* world, const char* function_name = ECS_FUNCTION) : internal_(thread_id, world, function_name) {}

	//	// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
	//	template<typename Functor>
	//	ECS_INLINE void Function(Functor& functor) {
	//		internal_.Function(functor);
	//	}

	//	Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_IS_COMMIT, Components...> internal_;
	//};

	//// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
	//// Runs on multiple threads. The lambda cannot capture by reference!! (This will cause the thread tasks to reference
	//// stack memory from a stack frame that is invalid - if you want a piece of code to be cached across tasks, create a temporary
	//// pointer to it from the thread allocator and then reference the pointer by value)
	//template<bool get_query, typename... Components>
	//struct ForEachBatch {
	//	ECS_INLINE ForEachBatch(unsigned int thread_id, World* world, const char* function_name = ECS_FUNCTION) : internal_(thread_id, world, function_name) {}

	//	// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
	//	template<typename Functor>
	//	ECS_INLINE void Function(Functor& functor) {
	//		internal_.Function(functor);
	//	}

	//	Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_IS_BATCH, Components...> internal_;
	//};

	//// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
	//template<bool get_query, typename... Components>
	//struct ForEachBatchCommit {
	//	ECS_INLINE ForEachBatchCommit(unsigned int thread_id, World* world, const char* function_name = ECS_FUNCTION) : internal_(thread_id, world, function_name) {}

	//	// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
	//	template<typename... ExcludeComponents, typename Functor>
	//	ECS_INLINE void Function(Functor& functor) {
	//		internal_.Function(functor);
	//	}

	//	Internal::ForEachEntityOrBatch<get_query, Internal::FOR_EACH_IS_BATCH | Internal::FOR_EACH_IS_COMMIT, Components...> internal_;
	//};

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