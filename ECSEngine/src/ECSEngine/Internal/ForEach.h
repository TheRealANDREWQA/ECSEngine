#pragma once
#include "EntityManager.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------------------

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