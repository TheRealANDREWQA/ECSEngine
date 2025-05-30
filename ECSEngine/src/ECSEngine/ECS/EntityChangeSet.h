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

	// This structure describe the changes that were done for a particular component of an entity, changes
	// That include alterations of the reflection type itself of the component
	struct EntityChange {
		Component component;
		ECS_CHANGE_SET_TYPE type;
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

	enum ECS_DETERMINE_ENTITY_CHANGES_FILTER : unsigned char {
		ECS_DETERMINE_ENTITY_CHANGES_NONE = 0,
		ECS_DETERMINE_ENTITY_CHANGES_UNIQUE_ADDITIONS = 1 << 0,
		ECS_DETERMINE_ENTITY_CHANGES_UNIQUE_REMOVALS = 1 << 1,
		ECS_DETERMINE_ENTITY_CHANGES_UNIQUE_UPDATES = 1 << 2,
		ECS_DETERMINE_ENTITY_CHANGES_SHARED_ADDITIONS = 1 << 3,
		ECS_DETERMINE_ENTITY_CHANGES_SHARED_REMOVALS = 1 << 4,
		ECS_DETERMINE_ENTITY_CHANGES_SHARED_UPDATES = 1 << 5,
		ECS_DETERMINE_ENTITY_CHANGES_ALL = ECS_DETERMINE_ENTITY_CHANGES_UNIQUE_ADDITIONS | ECS_DETERMINE_ENTITY_CHANGES_UNIQUE_REMOVALS |
			ECS_DETERMINE_ENTITY_CHANGES_UNIQUE_UPDATES | ECS_DETERMINE_ENTITY_CHANGES_SHARED_ADDITIONS | ECS_DETERMINE_ENTITY_CHANGES_SHARED_REMOVALS |
			ECS_DETERMINE_ENTITY_CHANGES_SHARED_UPDATES
	};

	// The allocator is needed to make the allocations for the update set
	// The change set is generated such that the source will be updated to
	// The destination if applying the change set
	// The last argument can be used to allow only certain features to be detected
	ECSENGINE_API void DetermineEntityChanges(
		const Reflection::ReflectionManager* reflection_manager,
		const EntityManager* source_entity_manager,
		const EntityManager* destination_entity_manager,
		Entity source_entity,
		Entity destination_entity,
		CapacityStream<EntityChange>* changes,
		AllocatorPolymorphic allocator,
		ECS_DETERMINE_ENTITY_CHANGES_FILTER filter = ECS_DETERMINE_ENTITY_CHANGES_ALL
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