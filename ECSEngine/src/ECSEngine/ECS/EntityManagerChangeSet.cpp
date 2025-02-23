#include "ecspch.h"
#include "EntityManagerChangeSet.h"
#include "EntityManager.h"

// 256
#define DECK_POWER_OF_TWO_EXPONENT 8

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	void EntityManagerChangeSet::Initialize(AllocatorPolymorphic allocator) {
		entity_unique_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_destroys.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_additions.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		shared_instances_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		global_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
	}

	void EntityManagerChangeSet::Deallocate() {
		entity_unique_component_changes.Deallocate();
		entity_info_changes.Deallocate();
		entity_info_destroys.Deallocate();
		entity_info_additions.Deallocate();
		shared_instances_changes.Deallocate();
		global_component_changes.Deallocate();
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// The change set must have the buffers set up before calling this function. This function only calculates the changes that have taken
	// Place in the entity pool entity info structures, it does not check unique/shared/global components
	static void DetermineEntityManagerEntityInfoChangeSet(const EntityManager* previous_entity_manager, const EntityManager* new_entity_manager, EntityManagerChangeSet& change_set) {
		const EntityPool* previous_pool = previous_entity_manager->m_entity_pool;
		const EntityPool* new_pool = new_entity_manager->m_entity_pool;

		// We can utilize the existing interface of the entity pool in order to query the data
		previous_pool->ForEach([&](Entity entity, EntityInfo entity_info) {
			// Check to see if the entity info still exists in the new entity manager, if it doesn't, a removal took place
			unsigned int new_pool_generation_index = new_pool->IsEntityAt(entity.index);
			if (new_pool_generation_index == -1) {
				// Removal
				change_set.entity_info_destroys.Add({ entity });
			}
			else {
				// If the generation index is different, then it means that the previous entity was destroyed and a new one was created
				// In its place instead
				EntityInfo new_entity_info = new_pool->GetInfoNoChecks(Entity(entity.index, new_pool_generation_index));

				if (new_pool_generation_index != entity_info.generation_count) {
					change_set.entity_info_destroys.Add({ entity });
					change_set.entity_info_additions.Add({ entity, new_entity_info });
				}
				else {
					// The generation is the same, then check the fields of the entity info for changes
					if (entity_info != new_entity_info) {
						change_set.entity_info_changes.Add({ entity, new_entity_info });
					}
				}
			}
		});

		// After iterating over the previous pool, we now need to iterate over the new pool to determine new additions
		new_pool->ForEach([&](Entity entity, EntityInfo entity_info) {
			unsigned int previous_pool_generation_index = new_pool->IsEntityAt(entity.index);
			if (previous_pool_generation_index == -1) {
				// This is a new addition, register it
				change_set.entity_info_additions.Add({ entity, entity_info });
			}
		});
	}

	static void DetermineEntityManagerUniqueComponentChangeSet(const EntityManager* previous_entity_manager, const EntityManager* new_entity_manager, EntityManagerChangeSet& change_set) {
		ComponentSignature archetype_unique_signature;
		
		// Iterate over the previous manager, using the for each function, and for all entities that exist in the other manager as well,
		// Perform the cross reference to see what was added/removed/updated
		previous_entity_manager->ForEachEntity(
			[&](const Archetype* archetype) {
				archetype_unique_signature = archetype->GetUniqueSignature();
			}, 
			[](const Archetype* archetype, unsigned int base_index) {}, 
			[&](const Archetype* archetype, const ArchetypeBase* archetype_base, Entity entity, void** unique_components) {
				const EntityInfo* new_entity_info = new_entity_manager->TryGetEntityInfo(entity);
				if (new_entity_info != nullptr) {
					//new_entity_manager->TryGetComponent()
				}
			});
	}

	static void DetermineEntityManagerSharedComponentChangeSet(const EntityManager* previous_entity_manager, const EntityManager* new_entity_manager, EntityManagerChangeSet& change_set) {
		
	}

	static void DetermineEntityManagerGlobalComponentChangeSet(const EntityManager* previous_entity_manager, const EntityManager* new_entity_manager, EntityManagerChangeSet& change_set) {

	}

	EntityManagerChangeSet DetermineEntityManagerChangeSet(const EntityManager* previous_entity_manager, const EntityManager* new_entity_manager, AllocatorPolymorphic change_set_allocator) {
		EntityManagerChangeSet change_set;
		// Initialize the change set as the first action
		change_set.Initialize(change_set_allocator);

		DetermineEntityManagerEntityInfoChangeSet(previous_entity_manager, new_entity_manager, change_set);
		DetermineEntityManagerUniqueComponentChangeSet(previous_entity_manager, new_entity_manager, change_set);
		DetermineEntityManagerSharedComponentChangeSet(previous_entity_manager, new_entity_manager, change_set);
		DetermineEntityManagerGlobalComponentChangeSet(previous_entity_manager, new_entity_manager, change_set);
	
		return change_set;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SetEntityManagerDeltaWriterInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const EntityManager* entity_manager, CapacityStream<void>& stack_memory) {

	}

	void SetEntityManagerDeltaWriterWorldInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const World* world, CapacityStream<void>& stack_memory) {

	}

	void SetEntityManagerDeltaReaderInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, EntityManager* entity_manager, CapacityStream<void>& stack_memory) {

	}

	void SetEntityManagerDeltaReaderWorldInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, World* world, CapacityStream<void>& stack_memory) {

	}

	// -----------------------------------------------------------------------------------------------------------------------------

}