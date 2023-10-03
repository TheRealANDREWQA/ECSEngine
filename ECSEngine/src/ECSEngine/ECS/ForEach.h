#pragma once
#include "InternalStructures.h"
#include "../Tools/Modules/ModuleDefinition.h"

namespace ECSEngine {

	struct World;
	struct ArchetypeQueryDescriptor;

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

	struct ForEachEntityFunctorData {
		unsigned int thread_id;
		Entity entity;
		World* world;
		void** unique_components;
		void** shared_components;
		void* data;
		EntityManagerCommandStream* command_stream;
	};

	typedef void (*ForEachEntityFunctor)(ForEachEntityFunctorData* data);

	struct ForEachBatchFunctorData {
		unsigned int thread_id;
		World* world;
		const Entity* entities;
		unsigned short count;
		void** unique_components;
		void** shared_components;
		void* data;
		EntityManagerCommandStream* command_stream;
	};

	typedef void (*ForEachBatchFunctor)(ForEachBatchFunctorData* data);
	
	struct ForEachEntityData {
		unsigned int thread_id;
		Entity entity;
		World* world;
		EntityManagerCommandStream* command_stream;
	};

	struct ForEachBatchData {
		unsigned int thread_id;
		unsigned short count;
		World* world;
		const Entity* entities;
		EntityManagerCommandStream* command_stream;
	};

	// This is the same as the ForEachEntity with the difference that it can be outside the ECS runtime. This version
	// doesn't use the archetype query acceleration. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachEntityCommitFunctor(
		World* world,
		ForEachEntityFunctor functor,
		void* data,
		const ArchetypeQueryDescriptor& query_descriptor
	);

	// This is the same as the ForEachEntity with the difference that it can be outside the ECS runtime. This version
	// doesn't use the archetype query acceleration. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachBatchCommitFunctor(
		World* world,
		ForEachBatchFunctor functor,
		void* data,
		unsigned short batch_size,
		const ArchetypeQueryDescriptor& query_descriptor
	);

	template<typename Functor>
	ECS_INLINE void ForEachEntityCommitFunctor(
		World* world,
		const ArchetypeQueryDescriptor& query_descriptor,
		Functor& functor
	) {
		auto functor_wrapper = [](ForEachEntityFunctorData* for_each_data) {
			Functor* functor = (Functor*)for_each_data->data;
			(*functor)(for_each_data);
		};

		ForEachEntityCommitFunctor(world, functor_wrapper, &functor, query_descriptor);
	}

	template<typename Functor>
	ECS_INLINE void ForEachBatchCommitFunctor(
		World* world,
		unsigned short batch_size,
		const ArchetypeQueryDescriptor& query_descriptor,
		Functor& functor
	) {
		auto functor_wrapper = [](ForEachBatchFunctorData* for_each_data) {
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
			ForEachEntityFunctor functor,
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
			ForEachBatchFunctor functor,
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
							if (is_optional) {
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

		static ForEachEntityData FromFunctorToEntityData(ForEachEntityFunctorData* data) {
			ForEachEntityData for_each_data;

			for_each_data.command_stream = data->command_stream;
			for_each_data.entity = data->entity;
			for_each_data.thread_id = data->thread_id;
			for_each_data.world = data->world;

			return for_each_data;
		}

		static ForEachBatchData FromFunctorToBatchData(ForEachBatchFunctorData* data) {
			ForEachBatchData for_each_data;

			for_each_data.command_stream = data->command_stream;
			for_each_data.entities = data->entities;
			for_each_data.thread_id = data->thread_id;
			for_each_data.world = data->world;
			for_each_data.count = data->count;

			return for_each_data;
		}

		template<bool is_batch, typename Functor, typename... T>
		void ForEachEntityBatchTypeSafeWrapper(ForEachEntityFunctorData* data) {
			std::remove_reference_t<Functor>* functor = (std::remove_reference_t<Functor>*)data->data;
			
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

			// The parameters need to be indexed backwards
			total_index = component_count - 1;
			
			if constexpr (is_batch) {
				ForEachBatchData for_each_data = FromFunctorToBatchData(data);
				(*functor)(&for_each_data, (typename T::Type*)(current_components[total_index--])...);
			}
			else {
				ForEachEntityData for_each_data = FromFunctorToEntityData(data);
				(*functor)(&for_each_data, (typename T::Type*)(current_components[total_index--])...);
			}
		}

		template<typename... Components>
		void GetComponentSignatureFromTemplatePack(
			ComponentSignature& unique_components, 
			ComponentSignature& shared_components, 
			ComponentSignature& optional_unique_components,
			ComponentSignature& optional_shared_components
		) {
			constexpr size_t count = sizeof...(Components);
			
			unique_components.count = 0;
			shared_components.count = 0;
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

				for (size_t index = 0; index < count; index++) {
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

		template<bool is_batch, typename... Components>
		struct ForEachEntityBatchCommit {
			template<typename... ExcludeComponents, typename Functor>
			static void Function(World* world, unsigned short batch_size, Functor& functor) {
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
				// Used for the exclude - there shouldn't be any optional components in it
				ComponentSignature null_optional_signature = { nullptr, 0 };

				Internal::GetComponentSignatureFromTemplatePack<Components...>(
					query_descriptor.unique, 
					query_descriptor.shared, 
					query_descriptor.unique_optional, 
					query_descriptor.shared_optional
				);
				Internal::GetComponentSignatureFromTemplatePack<ExcludeComponents...>(
					query_descriptor.unique_exclude, 
					query_descriptor.shared_exclude, 
					null_optional_signature, 
					null_optional_signature
				);

				if constexpr (is_batch) {
					ForEachBatchCommitFunctor(
						world,
						Internal::ForEachEntityBatchTypeSafeWrapper<true, Functor, Components...>,
						&functor,
						batch_size,
						query_descriptor
					);
				}
				else {
					ForEachEntityCommitFunctor(
						world,
						Internal::ForEachEntityBatchTypeSafeWrapper<false, Functor, Components...>,
						&functor,
						query_descriptor
					);
				}
			}
		};

		template<bool is_batch, bool get_query, typename... Components>
		struct ForEachEntityOrBatch {
			template<typename... ExcludeComponents, typename Functor>
			static void Function(unsigned int thread_id, World* world, Functor& functor, const char* function_name) {
				if constexpr (get_query) {
					Internal::RegisterForEachInfo* info = (Internal::RegisterForEachInfo*)world;
					Internal::AddQueryComponents<Components...>(info);
					Internal::AddQueryComponents<ExcludeComponents...>(info);
				}
				else {
					// Retrieve the optional components since they are not stored in the query cache
					Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					Component shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
					Component optional_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					Component optional_shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
					ComponentSignature unique_signature = { unique_components, 0 };
					ComponentSignature shared_signature = { shared_components, 0 };
					ComponentSignature optional_signature = { optional_components, 0 };
					ComponentSignature optional_shared_signature = { optional_shared_components, 0 };
					Internal::GetComponentSignatureFromTemplatePack<Components...>(
						unique_signature,
						shared_signature,
						optional_signature,
						optional_shared_signature
					);

					if constexpr (is_batch) {
						Internal::ForEachBatch(
							thread_id,
							world,
							Internal::ForEachEntityBatchTypeSafeWrapper<is_batch, Functor, Components...>,
							function_name,
							&functor,
							sizeof(functor),
							optional_signature,
							optional_shared_signature
						);
					}
					else {
						Internal::ForEachEntity(
							thread_id,
							world,
							Internal::ForEachEntityBatchTypeSafeWrapper<is_batch, Functor, Components...>,
							function_name,
							&functor,
							sizeof(functor),
							optional_signature,
							optional_shared_signature
						);
					}
				}
			}
		};

	}

	// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
	// Runs on multiple threads
	template<bool get_query, typename... Components>
	struct ForEachEntity {
		// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
		template<typename... ExcludeComponents, typename Functor>
		static void Function(unsigned int thread_id, World* world, Functor& functor, const char* function_name = __builtin_FUNCTION()) {
			Internal::ForEachEntityOrBatch<false, get_query, Components...>::Function<ExcludeComponents...>(thread_id, world, functor, function_name);
		}
	};

	// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
	template<typename... Components>
	struct ForEachEntityCommit {
		// The functor must take as first parameter a [const] ForEachEntityData* and the type correct component pointers
		template<typename... ExcludeComponents, typename Functor>
		static void Function(World* world, Functor& functor) {
			Internal::ForEachEntityBatchCommit<false, Components...>::Function<ExcludeComponents...>(world, 0, functor);
		}
	};

	// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
	// Runs on multiple threads
	template<bool get_query, typename... Components>
	struct ForEachEntityBatch {
		// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
		template<typename... ExcludeComponents, typename Functor>
		static void Function(unsigned int thread_id, World* world, Functor& functor, const char* function_name = __builtin_FUNCTION()) {
			Internal::ForEachEntityOrBatch<true, get_query, Components...>::Function<ExcludeComponents...>(thread_id, world, functor, function_name);
		}
	};

	// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
	template<typename... Components>
	struct ForEachBatchCommit {
		// The functor must take as first parameter a [const] ForEachBatchData* and the type correct component pointers
		template<typename... ExcludeComponents, typename Functor>
		static void Function(World* world, unsigned short batch_size, Functor& functor) {
			Internal::ForEachEntityBatchCommit<true, Components...>::Function<ExcludeComponents...>(world, batch_size, functor);
		}
	};

#define ECS_REGISTER_FOR_EACH_TASK(schedule_element, thread_task_function, module_function_data)	\
	ECSEngine::Internal::RegisterForEachInfo __register_info##thread_task_function; \
	__register_info##thread_task_function.module_data = module_function_data; \
	__register_info##thread_task_function.query = &schedule_element.component_query; \
	thread_task_function<true>(0, (ECSEngine::World*)&__register_info##thread_task_function, nullptr); \
	schedule_element.task_function = thread_task_function<false>; \
	schedule_element.task_name = STRING(thread_task_function); \
	module_function_data->tasks->AddAssert(schedule_element)

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