// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Deck.h"
#include "InternalStructures.h"
#include "EntityChangeSet.h"
#include "EntityHierarchy.h"
#include "../Utilities/StreamUtilities.h"

namespace ECSEngine {

	struct EntityManager;
	struct SerializeEntityManagerOptions;
	struct DeserializeEntityManagerOptions;
	struct DeserializeEntityManagerHeaderSectionData;

	namespace Reflection {
		struct ReflectionManager;
	}

	struct ECSENGINE_API ECS_REFLECT EntityManagerChangeSet {
		// This structure describes a unique component update/addition/removal
		struct EntityComponentChange {
			ECS_CHANGE_SET_TYPE change_type;
			Component component;
		};

		// This can contain a new component addition or the update of an existing component
		struct EntitySharedComponentInstanceChange {
			Component component;
			SharedInstance instance;
		};

		// Store the changes per entity, which achieves a blend of fast entity change set runtime determination, and disk write efficiency
		struct EntityChanges {
			Entity entity;
			Stream<EntityComponentChange> unique_changes;
			Stream<Component> removed_shared_components;
			// Can contain a new component addition or the update of an existing component
			Stream<EntitySharedComponentInstanceChange> shared_instance_changes;
		};

		// Describes a new archetype that was created
		struct NewArchetype {
			Stream<Component> unique_signature;
			Stream<Component> shared_signature;
			// A short suffices as indexing. This is the index of the archetype where it should be located at.
			unsigned short index;
		};

		struct NewArchetypeBase {
			Stream<SharedInstance> instances;
			// A short suffices as indexing. This is the index inside the archetype's array where it should be located at.
			unsigned short index;
		};

		struct BaseArchetypeChanges {
			// A short suffices as indexing
			unsigned short archetype;
			// Like the normal archetypes, the order in which these must be applied is critical.
			// Firstly, those that need to be destroyed are removed, then the new ones are created
			// And at last those that are moved are handled.
			ResizableStream<unsigned short> destroyed_base;
			ResizableStream<NewArchetypeBase> new_base;
			ResizableStream<MovedElementIndex<unsigned short>> moved_base;
		};

		// This is created when the entity info of an entity is changed
		struct EntityInfoChange {
			Entity entity;
			EntityInfo info;
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
		DeckPowerOfTwo<Entity> entity_info_destroys;

		// These are 2 SoA buffers. The reason for not using AoS is because on deserialization
		// Having the entities as a buffer directly helps with the creation API
		//struct {
			DeckPowerOfTwo<Entity> entity_info_additions_entity;
			DeckPowerOfTwo<EntityInfo> entity_info_additions_entity_info;
		//};

		// This structure contains the modifications that shared instances went through - we need to record these as well,
		// Otherwise the entities might reference invalid data
		DeckPowerOfTwo<SharedComponentChanges> shared_component_changes;
		DeckPowerOfTwo<GlobalComponentChange> global_component_changes;

		// We need to record the archetype changes as well, such that the consistency is maintained
		// The order in which these are applied is important. Firstly, the necessary archetypes are destroyed,
		// Then the new archetypes are created and at last the moved archetypes are handled. It is critical that
		// These operations are done exactly in this order, otherwise the consistency is not achieved
		DeckPowerOfTwo<unsigned short> destroyed_archetypes;
		DeckPowerOfTwo<NewArchetype> new_archetypes;
		DeckPowerOfTwo<MovedElementIndex<unsigned short>> moved_archetypes;

		// The base archetype changes need to be recorded as well
		DeckPowerOfTwo<BaseArchetypeChanges> base_archetype_changes;

		// Record the hierarchy change set as well
		EntityHierarchyChangeSet hierarchy_change_set;
	};

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
	// New entity manager. The serialize options should be pre-determined such that they are not constantly re-computed for multiple
	// Delta writes. With the write_entity_manager_header_section flag you can disable the entity manager header section, 
	// In case that was written separately.
	// Returns true if it succeeded, else false
	ECSENGINE_API bool SerializeEntityManagerChangeSet(
		const EntityManagerChangeSet* change_set,
		const EntityManager* new_entity_manager,
		const SerializeEntityManagerOptions* serialize_options,
		const Reflection::ReflectionManager* reflection_manager,
		WriteInstrument* write_instrument,
		bool write_entity_manager_header_section
	);

	struct DeserializeEntityManagerChangeSetOptions {
		// If the header section was serialized separately, you will need to provide it here,
		// Otherwise it will assume that it needs to read it.
		const DeserializeEntityManagerHeaderSectionData* deserialize_manager_header_section = nullptr;
		// If you want to receive the change set as an output parameter, you can fill this pointer in
		// And provide a temporary allocator such that the buffers can be allocated from it
		EntityManagerChangeSet* change_set = nullptr;
		AllocatorPolymorphic temporary_allocator = {};
	};

	// Reads a deserialized entity manager change set and applies it to the entity manager.
	// The deserialize_options remove_missing_components applies here, if a component is missing,
	// And this flag is false, it will fail the deserialization. Check the other options that are
	// Available, including receiving the change set as an out parameter, in case you want to perform
	// Some other operations.
	// Returns true if it succeeded, else false.
	ECSENGINE_API bool DeserializeEntityManagerChangeSet(
		EntityManager* entity_manager,
		const DeserializeEntityManagerOptions* deserialize_options,
		const Reflection::ReflectionManager* reflection_manager,
		ReadInstrument* read_instrument,
		DeserializeEntityManagerChangeSetOptions* options = nullptr
	);

	// -----------------------------------------------------------------------------------------------------------------------------

}