#pragma once
#include "World.h"
#include "Multithreading/TaskSchedulerTypes.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------------------

	template<typename T>
	struct Read {
		using type = T;

		constexpr static inline ECS_ACCESS_TYPE Access() {
			return ECS_READ;
		}

		constexpr static inline bool IsExclude() {
			return false;
		}
	};

	template<typename T>
	struct Write {
		using type = T;

		constexpr static inline ECS_ACCESS_TYPE Access() {
			return ECS_WRITE;
		}

		constexpr static inline bool IsExclude() {
			return false;
		}
	};

	template<typename T>
	struct ReadWrite {
		using type = T;

		constexpr static inline ECS_ACCESS_TYPE Access() {
			return ECS_READ_WRITE;
		}

		constexpr static inline bool IsExclude() {
			return false;
		}
	};

	template<typename T>
	struct Exclude {
		using type = T;

		constexpr static inline bool IsExclude() {
			return true;
		}
	};

	template<typename Head, typename... AccessTypes>
	void QueryExtractComponent(TaskComponentQuery& query, LinearAllocator* allocator) {
		unsigned short index = Head::type::Index();
		constexpr bool exclude = Head::IsExclude();
		constexpr bool is_shared = Head::type::IsShared();

		if constexpr (exclude) {
			if constexpr (is_shared) {
				query.AddSharedComponentExclude({ index }, allocator);
			}
			else {
				query.AddComponentExclude({ index }, allocator);
			}
		}
		else {
			ECS_ACCESS_TYPE access = Head::Access();
			if constexpr (is_shared) {
				query.AddSharedComponent({ index }, access, allocator);
			}
			else {
				query.AddComponent({ index }, access, allocator);
			}
		}

		if constexpr (sizeof...(AccessTypes) > 0) {
			QueryExtractComponent<AccessTypes...>(query, allocator);
		}
	}

	template<typename... AccessTypes>
	struct QueryBuilder {
		// The stack allocator is needed in case there are more components that can be stored in place
		static TaskComponentQuery GetQuery(
			LinearAllocator* stack_allocator, 
			unsigned short batch_size = 0,
			ECS_THREAD_TASK_READ_VISIBILITY_TYPE read_visibility = ECS_THREAD_TASK_READ_LAZY,
			ECS_THREAD_TASK_WRITE_VISIBILITY_TYPE write_visibility = ECS_THREAD_TASK_WRITE_LAZY
		) {
			TaskComponentQuery query;

			QueryExtractComponent<AccessTypes...>(query, stack_allocator);
			query.batch_size = batch_size;
			query.read_type = read_visibility;
			query.write_type = write_visibility;

			return query;
		}

		template<typename Functor>
		static void ForEach(unsigned int thread_id, World* world, Functor&& functor) {
			TaskScheduler* scheduler = world->task_scheduler;
			Stream<unsigned short> archetypes = scheduler->GetQueryArchetypes(thread_id, world);
			EntityManager* entity_manager = world->entity_manager;

			for (size_t index = 0; index < archetypes.size; index++) {
				const Archetype* archetype = entity_manager->GetArchetype(archetypes[index]);

			}
		}
	};

	typedef void (*ForEachEntityFunctor)(Entity entity, void** unique_components, void** shared_components, void* data);

#define ECS_FOR_EACH_ENTITY_FUNCTOR(name) void name(Entity entity, void** unique_components, void** shared_components, void* data)

	ECSENGINE_API void ECS_VECTORCALL ForEachEntity(EntityManager* entity_manager, ArchetypeQuery query, void* data, ForEachEntityFunctor functor);

	ECSENGINE_API void ECS_VECTORCALL ForEachEntity(EntityManager* entity_manager, ArchetypeQueryExclude query, void* data, ForEachEntityFunctor functor);

	// -------------------------------------------------------------------------------------------------------------------------------

	typedef void (*ForEachBaseArchetypeFunctor)(ArchetypeBase* base, void** unique_components, void** shared_components, void* data);

#define ECS_FOR_EACH_BASE_ARCHETYPE(name) void name(unsigned int chunk_count, Entity** entities, unsigned int* chunk_sizes, void*** unique_components, void** shared_components, void* data)

	// The iteration order for the unique components is the chunk_index then the unique component index:
	// example unique_components[chunk_index][unique_component_index]
	ECSENGINE_API void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQuery query, void* data, ForEachBaseArchetypeFunctor functor);

	// The iteration order for the unique components is the chunk_index then the unique component index:
	// example unique_components[chunk_index][unique_component_index]
	ECSENGINE_API void ECS_VECTORCALL ForEachBaseArchetype(EntityManager* entity_manager, ArchetypeQueryExclude query, void* data, ForEachBaseArchetypeFunctor functor);

	// -------------------------------------------------------------------------------------------------------------------------------

	typedef void (*ForEachSharedInstanceFunctor)(unsigned short archetype_index, unsigned short base_archetype_index, void* data);

#define ECS_FOR_EACH_SHARED_INSTANCE(name) void name(unsigned short archetype_index, unsigned short base_archetype_index, void* data)

	ECSENGINE_API void ForEachSharedInstance(EntityManager* entity_manager, SharedComponentSignature shared_signature, void* data, ForEachSharedInstanceFunctor functor);

	// -------------------------------------------------------------------------------------------------------------------------------
}