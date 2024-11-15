#pragma once
#include "../Core.h"
#include "../Utilities/Serialization/DeltaStateSerializationForward.h"
#include "../Containers/Stream.h"
#include "InternalStructures.h"
#include "EntityChangeSet.h"

namespace ECSEngine {

	struct EntityManager;
	struct World;

	struct EntityManagerChangeSet {
		// This is related to a component change/addition/removal
		struct ExistingEntity {
			Entity entity;
			EntityChange change;
		};

		// This is created when the entity info of an entity is changed
		struct EntityInfoChange {
			Entity entity;
			EntityInfo info;
		};

		// When an entity is destroyed, this will record the entity that was destroyed
		struct EntityInfoDestroy {
			Entity entity;
		};

		// When a new entity is created, this will record the entity that was created
		struct EntityInfoAddition {
			Entity entity;
			EntityInfo entity_info;
		};

		struct GlobalComponentChange {
			ECS_CHANGE_SET_TYPE type;
			Component component;
		};

		// These are entities that existed previously, but component data was changed about them
		ResizableStream<ExistingEntity> existing_entity_changes;
		// From the EntityInfo structures we can deduce the entity pool and the entity order inside the archetypes
		ResizableStream<EntityInfoChange> entity_info_changes;
		ResizableStream<EntityInfoDestroy> entity_info_destroys;
		ResizableStream<EntityInfoAddition> entity_info_additions;
		ResizableStream<GlobalComponentChange> global_component_changes;
	};

	// -----------------------------------------------------------------------------------------------------------------------------



	// -----------------------------------------------------------------------------------------------------------------------------

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - outside the runtime context
	// The entity manager must be stable for the entire duration of the writer
	ECSENGINE_API void SetEntityManagerDeltaWriterInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const EntityManager* entity_manager, CapacityStream<void>& stack_memory);

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - for a simulation world
	ECSENGINE_API void SetEntityManagerDeltaWriterWorldInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const World* world, CapacityStream<void>& stack_memory);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The entity manager must be stable for the entire duration of the writer
	ECSENGINE_API void SetEntityManagerDeltaReaderInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, EntityManager* entity_manager, CapacityStream<void>& stack_memory);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	ECSENGINE_API void SetEntityManagerDeltaReaderWorldInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, World* world, CapacityStream<void>& stack_memory);

	// -----------------------------------------------------------------------------------------------------------------------------

}