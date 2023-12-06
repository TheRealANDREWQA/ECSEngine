#pragma once
#include "../Core.h"
#include "InternalStructures.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
		struct ReflectionTypeChange;
	}
	
	struct EntityManager;

	enum ECS_ENTITY_CHANGE_TYPE : unsigned char {
		ECS_ENTITY_CHANGE_ADD,
		ECS_ENTITY_CHANGE_REMOVE,
		ECS_ENTITY_CHANGE_UPDATE
	};

	struct EntityChange {
		Component component;
		ECS_ENTITY_CHANGE_TYPE type;
		bool is_shared;
		// This is relevant only for the update case, where we will record the change set
		Stream<Reflection::ReflectionTypeChange> updated_fields;
	};

	// Returns true if there is a change, else false
	// The allocator is needed to make the allocations for the update set
	ECSENGINE_API bool DetermineUpdateChangesForComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* component_type,
		const void* previous_data,
		const void* new_data,
		EntityChange* change,
		AllocatorPolymorphic allocator
	);

	// The allocator is needed to make the allocations for the update set
	// The change set is generated such that the source will be updated to
	// The destination if applying the change set
	ECSENGINE_API void DetermineEntityChanges(
		const Reflection::ReflectionManager* reflection_manager,
		const EntityManager* source_entity_manager,
		const EntityManager* destination_entity_manager,
		Entity source_entity,
		Entity destination_entity,
		CapacityStream<EntityChange>* changes,
		AllocatorPolymorphic allocator
	);

	// Applies the modifications to all the given entities
	ECSENGINE_API void ApplyEntityChanges(
		const Reflection::ReflectionManager* reflection_manager,
		EntityManager* entity_manager,
		Stream<Entity> entities,
		const void** unique_components,
		ComponentSignature unique_component_signature,
		const void** shared_components,
		ComponentSignature shared_component_signature,
		Stream<EntityChange> changes
	);

}