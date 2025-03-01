#pragma once
#include "../Core.h"
#include "../Utilities/Serialization/DeltaStateSerializationForward.h"
#include "../Containers/Deck.h"
#include "InternalStructures.h"
#include "EntityChangeSet.h"

namespace ECSEngine {

	struct EntityManager;
	struct World;

	namespace Reflection {
		struct ReflectionManager;
	}

	struct ECSENGINE_API EntityManagerChangeSet {
		// This structure describes a component update/addition/removal
		struct EntityComponentChange {
			ECS_CHANGE_SET_TYPE change_type;
			Component component;
		};

		// Store the changes per entity, which achieves a blend of fast entity change set runtime determination, and disk write efficiency
		struct EntityChanges {
			Entity entity;
			Stream<EntityComponentChange> changes;
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

		// This structure contains information about an instance of a shared component that was added/removed/changed
		struct SharedComponentInstanceChange {
			SharedInstance instance;
			ECS_CHANGE_SET_TYPE type;
		};

		struct SharedComponentChanges {
			Component component;
			Stream<SharedComponentInstanceChange> changes;
		};

		struct GlobalComponentChange {
			ECS_CHANGE_SET_TYPE type;
			Component component;
		};

		ECS_INLINE AllocatorPolymorphic Allocator() const {
			// Any deck can go in here, they all use the same allocator
			return entity_unique_component_changes.Allocator();
		}

		void Initialize(AllocatorPolymorphic allocator);

		void Deallocate();

		// Use decks instead of ResizableStream since the variability of changes can be quite large, and if a temporary linear allocator is used
		// Those reallocations can blow the allocator because of the fact that it can't deallocate. And since these fields won't ever be used to directly
		// Index, only to iterate over, the penalty is small

		// These are entities that existed previously, but component data was changed about them
		DeckPowerOfTwo<EntityChanges> entity_unique_component_changes;
		// From the EntityInfo structures we can deduce the entity pool and the entity order inside the archetypes
		DeckPowerOfTwo<EntityInfoChange> entity_info_changes;
		DeckPowerOfTwo<EntityInfoDestroy> entity_info_destroys;
		DeckPowerOfTwo<EntityInfoAddition> entity_info_additions;
		// This structure contains the modifications that shared instances went through - we need to record these as well,
		// Otherwise the entities might reference invalid data
		DeckPowerOfTwo<SharedComponentChanges> shared_component_changes;
		DeckPowerOfTwo<GlobalComponentChange> global_component_changes;
	};

	typedef DeltaStateGenericHeader EntityManagerDeltaSerializationHeader;

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the current input serialization version. It will be at max a byte.
	ECSENGINE_API unsigned char SerializeEntityManagerDeltaVersion();

	// -----------------------------------------------------------------------------------------------------------------------------

	// The allocator will be used to allocate the buffers needed. It computes the necessary changes that should be applied to the previous entity manager
	// To obtain the new entity manager. The reflection manager should contain the component types
	ECSENGINE_API EntityManagerChangeSet DetermineEntityManagerChangeSet(
		const EntityManager* previous_entity_manager, 
		const EntityManager* new_entity_manager, 
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic change_set_allocator
	);

	// Writes the delta between a previous entity manager and a new entity manager, after the change set has been computed.
	// The previous entity manager is no longer needed, since the changes that require actual data to be written will use the
	// New entity manager.
	// Returns true if it succeeded, else false
	ECSENGINE_API bool SerializeEntityManagerChangeSet(
		const EntityManagerChangeSet* change_set,
		const EntityManager* new_entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		WriteInstrument* write_instrument
	);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - outside the runtime context
	// The entity manager and the reflection manager must be stable for the entire duration of the writer
	ECSENGINE_API void SetEntityManagerDeltaWriterInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info, 
		const EntityManager* entity_manager, 
		const Reflection::ReflectionManager* reflection_manager, 
		CapacityStream<void>& stack_memory
	);

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - for a simulation world
	// The reflection manager must be stable for the entire duration of the writer
	ECSENGINE_API void SetEntityManagerDeltaWriterWorldInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info, 
		const World* world, 
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory
	);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The entity manager and the reflection manager must be stable for the entire duration of the reader
	ECSENGINE_API void SetEntityManagerDeltaReaderInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info, 
		EntityManager* entity_manager, 
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory
	);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The reflection manager must be stable for the entire duration of the reader
	ECSENGINE_API void SetEntityManagerDeltaReaderWorldInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info, 
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory
	);

	// -----------------------------------------------------------------------------------------------------------------------------

}