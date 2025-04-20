#include "ecspch.h"
#include "EntityManagerChangeSet.h"
#include "../Utilities/Reflection/Reflection.h"
#include "EntityManager.h"
#include "../Utilities/Serialization/Binary/Serialization.h"

// 256
#define DECK_POWER_OF_TWO_EXPONENT 8

using namespace ECSEngine::Reflection;

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	void EntityManagerChangeSet::Initialize(AllocatorPolymorphic allocator) {
		entity_unique_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_destroys.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_additions.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		shared_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		global_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
	}

	void EntityManagerChangeSet::Deallocate() {
		// The unique and shared changes have their own buffers
		entity_unique_component_changes.ForEach([&](const EntityChanges& changes) {
			changes.changes.Deallocate(Allocator());
			return false;
		});
		shared_component_changes.ForEach([&](const SharedComponentChanges& changes) {
			changes.changes.Deallocate(Allocator());
			return false;
		});

		entity_unique_component_changes.Deallocate();
		entity_info_changes.Deallocate();
		entity_info_destroys.Deallocate();
		entity_info_additions.Deallocate();
		shared_component_changes.Deallocate();
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

	static void DetermineEntityManagerUniqueComponentChangeSet(
		const EntityManager* previous_entity_manager, 
		const EntityManager* new_entity_manager, 
		const ReflectionManager* reflection_manager,
		EntityManagerChangeSet& change_set
	) {
		ComponentSignature archetype_unique_signature;
		const ReflectionType* unique_signature_reflection_types[ECS_ARCHETYPE_MAX_COMPONENTS];
		
		// Iterate over the previous manager, using the for each function, and for all entities that exist in the other manager as well,
		// Perform the cross reference to see what was added/removed/updated
		previous_entity_manager->ForEachEntity(
			[&](const Archetype* archetype) {
				archetype_unique_signature = archetype->GetUniqueSignature();
				// Cache the reflection types per each component
				for (size_t index = 0; index < archetype_unique_signature.count; index++) {
					unique_signature_reflection_types[index] = reflection_manager->GetType(previous_entity_manager->GetComponentName(archetype_unique_signature[index]));
				}
			}, 
			[](const Archetype* archetype, unsigned int base_index) {}, 
			[&](const Archetype* archetype, const ArchetypeBase* archetype_base, Entity entity, void** unique_components) {
				ECS_STACK_CAPACITY_STREAM(EntityManagerChangeSet::EntityComponentChange, component_changes, ECS_ARCHETYPE_MAX_COMPONENTS);

				size_t changes_deck_index = change_set.entity_unique_component_changes.ReserveAndUpdateSize();
				EntityManagerChangeSet::EntityChanges& changes = change_set.entity_unique_component_changes[changes_deck_index];

				const EntityInfo* new_entity_info = new_entity_manager->TryGetEntityInfo(entity);
				if (new_entity_info != nullptr) {
					for (size_t index = 0; index < archetype_unique_signature.count; index++) {
						Component current_component = archetype_unique_signature[index];
						const void* new_component = new_entity_manager->TryGetComponent(*new_entity_info, current_component);
						if (new_component == nullptr) {
							// The new entity is missing this component, register the change
							component_changes.AddAssert({ ECS_CHANGE_SET_REMOVE, current_component });
						}
						else {
							// The component exists, compare the data
							bool is_data_the_same = CompareReflectionTypeInstances(reflection_manager, unique_signature_reflection_types[index], unique_components[index], new_component);
							if (!is_data_the_same) {
								component_changes.AddAssert({ ECS_CHANGE_SET_UPDATE, current_component });
							}
						}
					}

					// After iterating over the previous manager entity components, iterate over the components in the new manager and detect those
					// That do not appear in the previous but are present in the new manager, this indicates that a new component was added
					ComponentSignature new_entity_signature = new_entity_manager->GetEntitySignatureStable(entity);
					for (size_t index = 0; index < new_entity_signature.count; index++) {
						if (archetype_unique_signature.Find(new_entity_signature[index]) == UCHAR_MAX) {
							// The component was added
							component_changes.AddAssert({ ECS_CHANGE_SET_ADD, new_entity_signature[index] });
						}
					}

					if (component_changes.size > 0) {
						// Insert this entry
						change_set.entity_unique_component_changes.Add({ entity, component_changes.Copy(change_set.Allocator()) });
					}
				}
			}
		);
	}

	static void DetermineEntityManagerSharedComponentChangeSet(
		const EntityManager* previous_entity_manager,
		const EntityManager* new_entity_manager,
		const ReflectionManager* reflection_manager,
		EntityManagerChangeSet& change_set
	) {
		// TODO: Assert that the named shared instances did not modify - at the moment we do not handle them
		previous_entity_manager->ForEachSharedComponent([&](Component component) {
			const ReflectionType* reflection_type = reflection_manager->GetType(previous_entity_manager->GetSharedComponentName(component));

			ECS_ASSERT(previous_entity_manager->GetNamedSharedInstanceCount(component) == 0 && new_entity_manager->GetNamedSharedInstanceCount(component) == 0, 
				"Named shared instances are not yet handled for EntityManagerChangeSet");

			ECS_STACK_CAPACITY_STREAM(EntityManagerChangeSet::SharedComponentInstanceChange, changes, ECS_KB * 16);
			// Iterate over the previous changes, where we can detect what was removed and updated
			previous_entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
				if (!new_entity_manager->ExistsSharedInstanceOnly(component, instance)) {
					// This instance was destroyed
					changes.AddAssert({ instance, ECS_CHANGE_SET_REMOVE });
				}
				else {
					// See if the data has changed
					const void* previous_data = previous_entity_manager->GetSharedData(component, instance);
					const void* new_data = new_entity_manager->GetSharedData(component, instance);
					if (!CompareReflectionTypeInstances(reflection_manager, reflection_type, previous_data, new_data)) {
						changes.AddAssert({ instance, ECS_CHANGE_SET_UPDATE });
					}
				}
			});

			// Now iterate over the new instances, to see what was added
			new_entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
				if (!previous_entity_manager->ExistsSharedInstanceOnly(component, instance)) {
					changes.AddAssert({ instance, ECS_CHANGE_SET_ADD });
				}
			});

			if (changes.size > 0) {
				// Add a new entry
				change_set.shared_component_changes.Add({ component, changes.Copy(change_set.Allocator()) });
			}
		});
	}

	static void DetermineEntityManagerGlobalComponentChangeSet(
		const EntityManager* previous_entity_manager, 
		const EntityManager* new_entity_manager, 
		const ReflectionManager* reflection_manager,
		EntityManagerChangeSet& change_set
	) {
		// Iterate over the previous global components to find those that have been removed or updated
		previous_entity_manager->ForAllGlobalComponents([&](const void* data, Component component) {
			const void* new_data = new_entity_manager->TryGetGlobalComponent(component);
			if (new_data != nullptr) {
				// Compare the new data to see if it changed
				if (!CompareReflectionTypeInstances(reflection_manager, reflection_manager->GetType(previous_entity_manager->GetGlobalComponentName(component)), data, new_data)) {
					change_set.global_component_changes.Add({ ECS_CHANGE_SET_UPDATE, component });
				}
			}
			else {
				// The global component was removed
				change_set.global_component_changes.Add({ ECS_CHANGE_SET_REMOVE, component });
			}
		});

		// Iterate over the new global components to find those that were added
		new_entity_manager->ForAllGlobalComponents([&](const void* data, Component component) {
			if (previous_entity_manager->TryGetGlobalComponent(component) == nullptr) {
				change_set.global_component_changes.Add({ ECS_CHANGE_SET_ADD, component });
			}
		});
	}

	EntityManagerChangeSet DetermineEntityManagerChangeSet(
		const EntityManager* previous_entity_manager, 
		const EntityManager* new_entity_manager, 
		const ReflectionManager* reflection_manager, 
		AllocatorPolymorphic change_set_allocator
	) {
		EntityManagerChangeSet change_set;
		// Initialize the change set as the first action
		change_set.Initialize(change_set_allocator);

		DetermineEntityManagerEntityInfoChangeSet(previous_entity_manager, new_entity_manager, change_set);
		DetermineEntityManagerUniqueComponentChangeSet(previous_entity_manager, new_entity_manager, reflection_manager, change_set);
		DetermineEntityManagerSharedComponentChangeSet(previous_entity_manager, new_entity_manager, reflection_manager, change_set);
		DetermineEntityManagerGlobalComponentChangeSet(previous_entity_manager, new_entity_manager, reflection_manager, change_set);
	
		return change_set;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool SerializeEntityManagerChangeSet(
		const EntityManagerChangeSet* change_set,
		const EntityManager* new_entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		WriteInstrument* write_instrument
	) {
		// Firstly, write the change set itself, the structure that describes what changes need to be performed,
		// Then write the component data that was changed
		
		// Use the reflection manager for that, it should handle this successfully
		if (Serialize(reflection_manager, reflection_manager->GetType(STRING(EntityManagerChangeSet)), &change_set, write_instrument) != ECS_SERIALIZE_OK) {
			return false;
		}

		// Now write the actual components that need to be written. Start with the global ones
		change_set->global_component_changes.ForEach([&](const EntityManagerChangeSet::GlobalComponentChange& change) {
			if (change.type == ECS_CHANGE_SET_ADD || change.type == ECS_CHANGE_SET_UPDATE) {
				const void* global_data = new_entity_manager->GetGlobalComponent(change.component);

			}

			return false;
		});

		return false;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}