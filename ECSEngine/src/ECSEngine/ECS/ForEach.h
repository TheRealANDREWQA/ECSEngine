#pragma once
#include "InternalStructures.h"
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
		Entity* entities;
		unsigned short count;
		void** unique_components;
		void** shared_components;
		void* data;
		EntityManagerCommandStream* command_stream;
	};

	typedef void (*ForEachBatchFunctor)(ForEachBatchFunctorData* data);
	
	struct ForEachEntityData {
		unsigned int thread_id;
		World* world;
		Entity entity;
		EntityManagerCommandStream* command_stream;
	};

	struct ForEachBatchData {
		unsigned int thread_id;
		World* world;
		Entity* entities;
		unsigned short count;
		EntityManagerCommandStream* command_stream;
	};

	// This is the same as the ForEachEntity with the difference that it can be outside the ECS runtime. This version
	// doesn't use the archetype query acceleration. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachEntityCommitFunctor(
		World* world,
		ForEachEntityFunctor functor,
		void* data,
		ComponentSignature unique_signature,
		ComponentSignature shared_signature,
		ComponentSignature unique_exclude_signature = {},
		ComponentSignature shared_exclude_signature = {}
	);

	// This is the same as the ForEachEntity with the difference that it can be outside the ECS runtime. This version
	// doesn't use the archetype query acceleration. The world needs to contain an entity manager. (other fields are optional)
	ECSENGINE_API void ForEachBatchCommitFunctor(
		World* world,
		ForEachBatchFunctor functor,
		void* data,
		unsigned short batch_size,
		ComponentSignature unique_signature,
		ComponentSignature shared_signature,
		ComponentSignature unique_exclude_signature = {},
		ComponentSignature shared_exclude_signature = {}
	);

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

				Component component[component_count] = { Components::Type::ID()... };

				for (size_t index = 0; index < component_count; index++) {
					if (is_shared[index]) {
						if (is_excluded[index]) {
							info->query->AddSharedComponentExclude(component[index], info->module_data->allocator);
						}
						else {
							info->query->AddSharedComponent(component[index], access_type[index], info->module_data->allocator);
						}
					}
					else {
						if (is_excluded[index]) {
							info->query->AddComponentExclude(component[index], info->module_data->allocator);
						}
						else {
							info->query->AddComponent(component[index], access_type[index], info->module_data->allocator);
						}
					}
				}
			}
		}

		ECS_INLINE ForEachEntityData FromFunctorToEntityData(ForEachEntityFunctorData* data) {
			ForEachEntityData for_each_data;
			for_each_data.command_stream = data->command_stream;
			for_each_data.entity = data->entity;
			for_each_data.thread_id = data->thread_id;
			for_each_data.world = data->world;
			return for_each_data;
		}

		ECS_INLINE ForEachBatchData FromFunctorToBatchData(ForEachBatchFunctorData* data) {
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
			Functor* functor = (Functor*)data->data;
			
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

			for (size_t index = 0; index < 2; index++) {
				current_components[total_index++] = components[component_index[is_shared[index]]++];
			}

			// The parameters need to be indexed backwards
			total_index = component_count - 1;
			
			if constexpr (is_batch) {
				ForEachBatchData for_each_data = FromFunctorToBatchData(data);
				(*functor)(&for_each_data, (*(typename T::Type*)(current_components[total_index--]))...);
			}
			else {
				ForEachEntityData for_each_data = FromFunctorToEntityData(data);
				(*functor)(&for_each_data, (*(typename T::Type*)(current_components[total_index--]))...);
			}
		}

		template<typename... Components>
		void GetComponentSignatureFromTemplatePack(ComponentSignature& unique_components, ComponentSignature& shared_components) {
			constexpr size_t count = sizeof...(Components);
			
			unique_components.count = 0;
			shared_components.count = 0;
			if constexpr (count > 0) {
				constexpr bool is_shared[count] = {
					Components::IsShared()...
				};
				constexpr short component_ids[count] = {
					Components::Type::ID()...
				};

				for (size_t index = 0; index < count; index++) {
					if (is_shared[index]) {
						shared_components[shared_components.count++] = { component_ids[index] };
					}
					else {
						unique_components[unique_components.count++] = { component_ids[index] };
					}
				}
			}
		}

		template<bool is_batch, typename... Components>
		struct ForEachEntityBatchCommit {
			template<typename... ExcludeComponents, typename Functor>
			static void Function(World* world, unsigned short batch_size, Functor&& functor) {
				Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
				Component shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
				Component unique_exclude_components[ECS_ARCHETYPE_MAX_COMPONENTS];
				Component shared_exclude_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

				ComponentSignature unique_signature = { unique_components, 0 };
				ComponentSignature shared_signature = { shared_components, 0 };
				ComponentSignature unique_exclude_signature = { unique_exclude_components, 0 };
				ComponentSignature shared_exclude_signature = { shared_exclude_components, 0 };
				Internal::GetComponentSignatureFromTemplatePack<Components...>(unique_signature, shared_signature);
				Internal::GetComponentSignatureFromTemplatePack<ExcludeComponents...>(unique_exclude_signature, shared_exclude_signature);

				if constexpr (is_batch) {
					ForEachBatchCommitFunctor(
						world,
						Internal::ForEachEntityBatchTypeSafeWrapper<true, Functor, Components...>,
						&functor,
						batch_size,
						unique_signature,
						shared_signature,
						unique_exclude_components,
						shared_exclude_signature
					);
				}
				else {
					ForEachEntityCommitFunctor(
						world,
						Internal::ForEachEntityBatchTypeSafeWrapper<false, Functor, Components...>,
						&functor,
						unique_signature,
						shared_signature,
						unique_exclude_components,
						shared_exclude_signature
					);
				}
			}
		};

	}

	template<bool get_query, typename... Components>
	struct ForEachEntity {
		template<typename... ExcludeComponents, typename Functor>
		static void Function(unsigned int thread_id, World* world, Functor&& functor, const char* function_name = __builtin_FUNCTION()) {
			if constexpr (get_query) {
				Internal::RegisterForEachInfo* info = (Internal::RegisterForEachInfo*)world;
				Internal::AddQueryComponents<Components...>(info);
				Internal::AddQueryComponents<ExcludeComponents...>(info);
			}
			else {
				Internal::ForEachEntity(thread_id, world, Internal::ForEachEntityBatchTypeSafeWrapper<false, Functor, Components...>, function_name, &functor, sizeof(functor));
			}
		}
	};

	template<typename... Components>
	struct ForEachEntityCommit {
		template<typename... ExcludeComponents, typename Functor>
		static void Function(World* world, Functor&& functor) {
			Internal::ForEachEntityBatchCommit<false, Components...>::Function<ExcludeComponents...>(world, 0, functor);
		}
	};

	template<bool get_query, typename... Components>
	struct ForEachEntityBatch {
		template<typename... ExcludeComponents, typename Functor>
		static void Function(unsigned int thread_id, World* world, Functor&& functor, const char* function_name = __builtin_FUNCTION()) {
			if constexpr (get_query) {
				Internal::RegisterForEachInfo* info = (Internal::RegisterForEachInfo*)world;
				Internal::AddQueryComponents<Components...>(info);
				Internal::AddQueryComponents<ExcludeComponents...>(info);
			}
			else {
				Internal::ForEachBatch(thread_id, world, Internal::ForEachEntityBatchTypeSafeWrapper<true, Functor, Components...>, function_name, &functor, sizeof(functor));
			}
		}
	};

	template<typename... Components>
	struct ForEachBatchCommit {
		template<typename... ExcludeComponents, typename Functor>
		static void Function(World* world, unsigned short batch_size, Functor&& functor) {
			Internal::ForEachEntityBatchCommit<true, Components...>::Function<ExcludeComponents...>(world, batch_size, functor);
		}
	};

#define ECS_REGISTER_FOR_EACH_TASK(schedule_element, thread_task_function, module_function_data)	\
	ECSEngine::Internal::RegisterForEachInfo __register_info##thread_task; \
	__register_info##thread_task.module_data = module_function_data; \
	__register_info##thread_task.query = &schedule_element.component_query; \
	thread_task_function<true>(0, (ECSEngine::World*)&__register_info##thread_task, nullptr); \
	schedule_element.task_function = thread_task_function<false>; \
	schedule_element.task_name = STRING(thread_task_function); \
	module_function_data->tasks->AddSafe(schedule_element)

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