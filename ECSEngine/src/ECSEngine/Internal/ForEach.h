#pragma once
#include "InternalStructures.h"

namespace ECSEngine {

	struct World;

	// -------------------------------------------------------------------------------------------------------------------------------

	typedef void (*ForEachEntityFunctor)(
		unsigned int thread_id,
		World* world,
		Entity entity, 
		void** unique_components,
		void** shared_components, 
		void* data, 
		EntityManagerCommandStream* command_stream
	);

#define ECS_FOR_EACH_ENTITY_FUNCTOR(name) void name(unsigned int thread_id, World* world, Entity entity, void** unique_components, void** shared_components, void* _data, EntityManagerCommand* command_stream)

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

	typedef void (*ForEachBatchFunctor)(
		unsigned int thread_id,
		World* world,
		Entity* entities,
		unsigned short count,
		void** unique_components,
		void** shared_components,
		void* data, 
		EntityManagerCommandStream* command_stream
	);

#define ECS_FOR_EACH_BATCH_FUNCTOR(name) void name(unsigned int thread_id, World* world, Entity* entities, unsigned short count, void** unique_components, void** shared_components, void* _data, EntityManagerCommandStream* command_stream)

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