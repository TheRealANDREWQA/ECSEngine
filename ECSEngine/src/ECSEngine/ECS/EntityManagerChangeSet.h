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

		// This is created when the entity info of an entity is changed (not the archetype related indices,
		// But the other data - generation count, tag or layer)
		struct EntityInfoChange {
			ECS_INLINE size_t GetGenerationCount() const {
				return ExtractBits(generation_count_tag_and_layer, 0, ECS_ENTITY_INFO_GENERATION_COUNT_BITS);
			}

			ECS_INLINE size_t GetTag() const {
				return ExtractBits(generation_count_tag_and_layer, ECS_ENTITY_INFO_GENERATION_COUNT_BITS, ECS_ENTITY_INFO_TAG_BITS);
			}

			ECS_INLINE size_t GetLayer() const {
				return ExtractBits(generation_count_tag_and_layer, ECS_ENTITY_INFO_GENERATION_COUNT_BITS + ECS_ENTITY_INFO_TAG_BITS, ECS_ENTITY_INFO_LAYER_BITS);
			}

			ECS_INLINE void SetGenerationCount(size_t generation_count) {
				generation_count_tag_and_layer = SetBits(generation_count_tag_and_layer, generation_count, 0, ECS_ENTITY_INFO_GENERATION_COUNT_BITS);
			}

			ECS_INLINE void SetTag(size_t tag) {
				generation_count_tag_and_layer = SetBits(generation_count_tag_and_layer, tag, ECS_ENTITY_INFO_GENERATION_COUNT_BITS, ECS_ENTITY_INFO_TAG_BITS);
			}

			ECS_INLINE void SetLayer(size_t layer) {
				generation_count_tag_and_layer = SetBits(generation_count_tag_and_layer, layer, ECS_ENTITY_INFO_GENERATION_COUNT_BITS + ECS_ENTITY_INFO_TAG_BITS, ECS_ENTITY_INFO_LAYER_BITS);
			}

			Entity entity;
			// At the moment, the reflection system cannot parse bit fields.
			// For this reason, use the underlying type directly. All these
			// Fields can fit in an unsigned int for the current bit counts.
			// Provide accessors to get/set these values. The first field is
			// The generation count, then the tag and then the layer.
			unsigned int generation_count_tag_and_layer;
		};

		// This structure encompasses multiple entities that were created using the same signature.
		struct NewEntitiesContainer {
			// Using Stream<Component> and Stream<SharedInstance> because the reflection system understands it
			Stream<Component> unique_components;
			Stream<Component> shared_components;
			Stream<SharedInstance> shared_instances;
			Stream<Entity> entities;
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
		DeckPowerOfTwo<Entity> destroyed_entities;
		DeckPowerOfTwo<NewEntitiesContainer> new_entities;

		// This structure contains the modifications that shared instances went through - we need to record these as well,
		// Otherwise the entities might reference invalid data
		DeckPowerOfTwo<SharedComponentChanges> shared_component_changes;
		DeckPowerOfTwo<GlobalComponentChange> global_component_changes;

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