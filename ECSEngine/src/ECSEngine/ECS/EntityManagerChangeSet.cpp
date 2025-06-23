#include "ecspch.h"
#include "EntityManagerChangeSet.h"
#include "../Utilities/Reflection/Reflection.h"
#include "EntityManager.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "EntityManagerSerialize.h"
#include "../Utilities/ReaderWriterInterface.h"
#include "../Utilities/DeltaChange.h"

// 256
#define DECK_POWER_OF_TWO_EXPONENT 8
// 32
#define DECK_POWER_OF_TWO_EXPONENT_SMALL 5

#define DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY (ECS_MB * 50)

using namespace ECSEngine::Reflection;

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	void EntityManagerChangeSet::Initialize(AllocatorPolymorphic allocator) {
		entity_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		destroyed_entities.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		new_entities.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT_SMALL);
		shared_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		global_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
	}

	void EntityManagerChangeSet::Deallocate() {
		// The unique and shared changes have their own buffers
		entity_component_changes.ForEach([&](const EntityChanges& changes) {
			changes.unique_changes.Deallocate(Allocator());
			changes.removed_shared_components.Deallocate(Allocator());
			changes.shared_instance_changes.Deallocate(Allocator());
		});
		shared_component_changes.ForEach([&](const SharedComponentChanges& changes) {
			changes.changes.Deallocate(Allocator());
		});
		new_entities.ForEach([&](const NewEntitiesContainer& new_entities) {
			new_entities.entities.Deallocate(Allocator());
			new_entities.unique_components.Deallocate(Allocator());
			new_entities.shared_components.Deallocate(Allocator());
			new_entities.shared_instances.Deallocate(Allocator());
		});

		entity_component_changes.Deallocate();
		entity_info_changes.Deallocate();
		destroyed_entities.Deallocate();
		new_entities.Deallocate();
		shared_component_changes.Deallocate();
		global_component_changes.Deallocate();
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// The change set must have the buffers set up before calling this function. This function only calculates the changes that have taken
	// Place in the entity pool entity info structures, it does not check unique/shared/global components. It will also report the new
	// Entities that have been created.
	static void DetermineEntityManagerEntityInfoChangeSet(const EntityManager* previous_entity_manager, const EntityManager* new_entity_manager, EntityManagerChangeSet& change_set) {
		const EntityPool* previous_pool = previous_entity_manager->m_entity_pool;
		const EntityPool* new_pool = new_entity_manager->m_entity_pool;

		// When the generation count for an entity has changed, we need to know if that entity truly exists or not.
		// The generation count can be changed when the entity was destroyed and no other was created in its place (the common
		// occurence), but it can happen that a new one was created in its place. To verify this, create a hash table with the
		// Entities that are stored inside the new entity manager's archetypes, since those contain all valid entities.
		HashTableEmpty<Entity, HashFunctionPowerOfTwo> new_entity_manager_valid_entities;
		new_entity_manager_valid_entities.Initialize(change_set.Allocator(), HashTablePowerOfTwoCapacityForElements(new_entity_manager->GetEntityCount()));
		new_entity_manager->ForEachEntitySimple([&](Entity entity) {
			new_entity_manager_valid_entities.Insert(entity);
		});

		struct Uint2Hasher {
			ECS_INLINE static unsigned int Hash(uint2 value) {
				return Cantor(value.x, value.y);
			}
		};

		using NewEntitiesMapping = HashTable<CapacityStream<Entity>, uint2, HashFunctionPowerOfTwo, Uint2Hasher>;

		// Keep a hash table that maps the { main_archetype_index, base_archetype_index } to a mapping of new entities container.
		// That mapping will be then condensed into the change set in order to use less memory on the serialization, but we need this
		// In order to quickly lookup the matching new entities container for a pair of archetype indices. We need one hash table
		// That maps from previous archetype indices, and another one that maps from the new entity manager indices, the reason is because
		// We have 2 insertion places, one with indices from the previous and the other one with indices from the new manager.
		NewEntitiesMapping previous_created_entities_containers;
		previous_created_entities_containers.Initialize(change_set.Allocator(), 32u);

		NewEntitiesMapping new_created_entities_containers;
		new_created_entities_containers.Initialize(change_set.Allocator(), 32u);

		auto insert_new_entity_to_table = [&](NewEntitiesMapping& mapping, const EntityManager* entity_manager, Entity entity) -> void {
			EntityInfo entity_info = entity_manager->GetEntityInfo(entity);
			uint2 archetype_indices = { (unsigned int)entity_info.main_archetype, (unsigned int)entity_info.base_archetype };
			unsigned int mapping_index = mapping.Find(archetype_indices);
			if (mapping_index == -1) {
				mapping.InsertDynamic(change_set.Allocator(), {}, archetype_indices, mapping_index);
			}

			CapacityStream<Entity>* mapping_entities = mapping.GetValuePtrFromIndex(mapping_index);
			mapping_entities->AddResizePercentage(entity, change_set.Allocator());
		};

		// We can utilize the existing interface of the entity pool in order to query the data
		previous_pool->ForEach([&](Entity entity, EntityInfo entity_info) {
			// Check to see if the entity info still exists in the new entity manager, if it doesn't, a removal took place
			unsigned int new_pool_generation_index = new_pool->IsEntityAt(entity.index);
			if (new_pool_generation_index == -1) {
				// Removal
				change_set.destroyed_entities.Add({ entity });
			}
			else {
				// If the generation index is different, then it means that the previous entity was destroyed and possibly a new one was created
				// In its place instead
				EntityInfo new_entity_info = new_pool->GetInfoNoChecks(Entity(entity.index, new_pool_generation_index));

				if (new_pool_generation_index != entity_info.generation_count) {
					// The previous entity is always destroyed.
					change_set.destroyed_entities.Add({ entity });
					
					// If we find a new entity at the given generation count, then it means a new one was created
					Entity new_entity = entity;
					new_entity.generation_count = new_pool_generation_index;
					if (new_entity_manager_valid_entities.Find(new_entity) != -1) {
						// We must create a new entity into the previous created entities containers
						insert_new_entity_to_table(previous_created_entities_containers, previous_entity_manager, entity);
					}
				}
				else {
					// The generation is the same, then check the fields of the entity info for changes, except
					// The archetype indices, we are not interested in those.
					if (entity_info.layer != new_entity_info.layer || entity_info.tags != new_entity_info.tags ||
						entity_info.generation_count != new_entity_info.generation_count) {
						EntityManagerChangeSet::EntityInfoChange info_change;
						info_change.SetGenerationCount(entity_info.generation_count);
						info_change.SetLayer(entity_info.layer);
						info_change.SetTag(entity_info.tags);
						info_change.entity = entity;
						change_set.entity_info_changes.Add(info_change);
					}
				}
			}
		});

		// After iterating over the previous pool, we now need to iterate over the new pool to determine new additions
		new_pool->ForEach([&](Entity entity, EntityInfo entity_info) {
			unsigned int previous_pool_generation_index = new_pool->IsEntityAt(entity.index);
			if (previous_pool_generation_index == -1) {
				// This is a new addition, register it
				insert_new_entity_to_table(new_created_entities_containers, new_entity_manager, entity);
			}
		});

		// At last, merge previous_created_entities_containers and new_created_entities_containers
		// Iterate over the previous created entities containers table and try to match that archetype
		// With the archetype from the other hash table. If it exists, their entries must be combined into a single one.
		// Then iterate over the new hash table and for all archetypes that have not been matched previously we must
		// Add them as well.
		previous_created_entities_containers.ForEachConst([&](CapacityStream<Entity> entities, uint2 archetype_indices) {
			// Try to match it with an entry from the new entity manager
			const Archetype* previous_archetype = previous_entity_manager->GetArchetype(archetype_indices.x);
			EntityManagerChangeSet::NewEntitiesContainer new_entities_container;
			new_entities_container.unique_components = previous_archetype->GetUniqueSignature().ToStream();
			new_entities_container.shared_components = previous_archetype->GetSharedSignature().ToStream();
			new_entities_container.shared_instances = { previous_archetype->GetBaseInstances(archetype_indices.y), new_entities_container.shared_components.size };
			
			unsigned int new_archetype_index = new_entity_manager->FindArchetype({ previous_entity_manager->GetArchetypeUniqueComponents(archetype_indices.x), previous_entity_manager->GetArchetypeSharedComponents(archetype_indices.x) });
			unsigned int new_base_index = -1;
			if (new_archetype_index != -1) {
				new_base_index = new_entity_manager->GetArchetype(new_archetype_index)->FindBaseIndex(previous_archetype->GetSharedSignature(archetype_indices.y));
			}

			if (new_archetype_index != -1 && new_base_index != -1) {
				// There is a matching, the entries must be merged
				unsigned int matched_entry_table_index = new_created_entities_containers.Find({ new_archetype_index, new_base_index });
				if (matched_entry_table_index == -1) {
					// There are no additions, only the current entities can be maintained
					new_entities_container.entities = entities;
				}
				else {
					Stream<Entity> other_entities = new_created_entities_containers.GetValueFromIndex(matched_entry_table_index);
					new_entities_container.entities = CoalesceStreamsSameTypeWithDeallocate<Entity>(change_set.Allocator(), entities, other_entities);
					
					// Also, remove the entry from new_created_entities_containers such that when we iterate in the next pass
					// We know that its entities have been matched and only the entries that are not matched will remain.
					new_created_entities_containers.EraseFromIndex(matched_entry_table_index);
				}
			}
			else {
				// There is no matching, only these entries must be kept
				new_entities_container.entities = entities;
			}
			
			change_set.new_entities.Add(&new_entities_container);
		});
		
		// The entries that remained here are not matched by the previous hash table, since those that have been matched were erased,
		// So these entries do not require merging.
		new_created_entities_containers.ForEachConst([&](CapacityStream<Entity> entities, uint2 archetype_indices) {
			const Archetype* archetype = new_entity_manager->GetArchetype(archetype_indices.x);
			EntityManagerChangeSet::NewEntitiesContainer new_entities_container;
			new_entities_container.unique_components = archetype->GetUniqueSignature().ToStream();
			new_entities_container.shared_components = archetype->GetSharedSignature().ToStream();
			new_entities_container.shared_instances = { archetype->GetBaseInstances(archetype_indices.y), new_entities_container.shared_components.size };
			new_entities_container.entities = entities;
			
			change_set.new_entities.Add(&new_entities_container);
		});

		// Deallocate the temporary allocations
		new_entity_manager_valid_entities.Deallocate(change_set.Allocator());
		previous_created_entities_containers.Deallocate(change_set.Allocator());
		new_created_entities_containers.Deallocate(change_set.Allocator());
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
						change_set.entity_component_changes.Add({ entity, component_changes.Copy(change_set.Allocator()) });
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
		change_set.hierarchy_change_set = DetermineEntityHierarchyChangeSet(&previous_entity_manager->m_hierarchy, &new_entity_manager->m_hierarchy, change_set_allocator);
	
		return change_set;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool SerializeEntityManagerChangeSet(
		const EntityManagerChangeSet* change_set,
		const EntityManager* new_entity_manager,
		const SerializeEntityManagerOptions* serialize_options,
		const ReflectionManager* reflection_manager,
		WriteInstrument* write_instrument,
		const SerializeEntityManagerChangeSetOptions* options
	) {
		SerializeEntityManagerChangeSetOptions default_options;
		if (options == nullptr) {
			options = &default_options;
		}

		// Firstly, write the change set itself, the structure that describes what changes need to be performed,
		// Then write the component data that was changed
		if (options->write_entity_manager_header_section) {
			if (!SerializeEntityManagerHeaderSection(new_entity_manager, write_instrument, serialize_options)) {
				return false;
			}
		}

		// Use the reflection manager for that, it should handle this successfully
		if (Serialize(reflection_manager, reflection_manager->GetType(STRING(EntityManagerChangeSet)), change_set, write_instrument, options->serialize_options) != ECS_SERIALIZE_OK) {
			return false;
		}

		// Now write the actual components that need to be written. Start with the global ones
		size_t global_component_write_offset = write_instrument->GetOffset();
		if (change_set->global_component_changes.ForEach<true>([&](const EntityManagerChangeSet::GlobalComponentChange& change) -> bool {
			if (change.type == ECS_CHANGE_SET_ADD || change.type == ECS_CHANGE_SET_UPDATE) {
				const void* global_data = new_entity_manager->GetGlobalComponent(change.component);

				const SerializeEntityManagerGlobalComponentInfo& component_info = *serialize_options->global_component_table->GetValuePtr(change.component);
				unsigned int global_component_size = 0;
				if (!write_instrument->AppendUninitialized(sizeof(global_component_size))) {
					return true;
				}

				SerializeEntityManagerGlobalComponentData function_data;
				function_data.write_instrument = write_instrument;
				function_data.components = global_data;
				function_data.extra_data = component_info.extra_data;
				function_data.count = 1;

				if (!component_info.function(&function_data)) {
					return true;
				}

				size_t current_write_offset = write_instrument->GetOffset();
				// Subtract the size of the component size that is included in the difference
				size_t write_difference = current_write_offset - global_component_write_offset - sizeof(global_component_size);
				if (!EnsureUnsignedIntegerIsInRange<unsigned int>(write_difference)) {
					return true;
				}
				// Write the count into the prefix size
				global_component_size = (unsigned int)write_difference;
				if (!write_instrument->WriteUninitializedData(global_component_write_offset, &global_component_size, sizeof(global_component_size))) {
					return true;
				}

				global_component_write_offset = current_write_offset;
			}

			return false;
			})) {
			return false;
		}

		// Continue with the shared components
		size_t shared_component_write_offset = write_instrument->GetOffset();
		if (change_set->shared_component_changes.ForEach<true>([&](const EntityManagerChangeSet::SharedComponentChanges& changes) -> bool {
			for (size_t index = 0; index < changes.changes.size; index++) {
				const SerializeEntityManagerSharedComponentInfo& component_info = *serialize_options->shared_component_table->GetValuePtr(changes.component);
				if (changes.changes[index].type == ECS_CHANGE_SET_ADD || changes.changes[index].type == ECS_CHANGE_SET_UPDATE) {

					// Prefix the actual instance data size with the write size of each instance.
					unsigned int write_size = 0;
					if (!write_instrument->AppendUninitialized(sizeof(write_size))) {
						return true;
					}

					SerializeEntityManagerSharedComponentData function_data;
					function_data.write_instrument = write_instrument;
					function_data.component_data = new_entity_manager->GetSharedData(changes.component, changes.changes[index].instance);
					function_data.extra_data = component_info.extra_data;
					function_data.instance = changes.changes[index].instance;

					if (!component_info.function(&function_data)) {
						return true;
					}

					size_t current_instrument_offset = write_instrument->GetOffset();
					size_t current_write_size = current_instrument_offset - shared_component_write_offset - sizeof(write_size);
					if (!EnsureUnsignedIntegerIsInRange<unsigned int>(current_write_size)) {
						return true;
					}

					if (!write_instrument->WriteUninitializedData(shared_component_write_offset, &write_size, sizeof(write_size))) {
						return true;
					}

					shared_component_write_offset = current_instrument_offset;
				}
			}

			return false;
		})) {
			return false;
		}

		// Now write the unique components, per entity. We don't have to write anything special for the shared data,
		// It can be handled with the info from the change set structure itself.
		size_t unique_component_write_offset = write_instrument->GetOffset();
		if (change_set->entity_component_changes.ForEach<true>([&](const EntityManagerChangeSet::EntityChanges& changes) -> bool {
			for (size_t index = 0; index < changes.unique_changes.size; index++) {
				if (changes.unique_changes[index].change_type == ECS_CHANGE_SET_ADD || changes.unique_changes[index].change_type == ECS_CHANGE_SET_UPDATE) {
					// Add a prefix size for how long the serialization of these components is
					unsigned int current_write_count = 0;
					if (!write_instrument->AppendUninitialized(sizeof(current_write_count))) {
						return true;
					}

					const SerializeEntityManagerComponentInfo& component_info = *serialize_options->component_table->GetValuePtr(changes.unique_changes[index].component);
					SerializeEntityManagerComponentData function_data;
					function_data.write_instrument = write_instrument;
					function_data.components = new_entity_manager->GetComponent(changes.entity, changes.unique_changes[index].component);
					function_data.count = 1;
					function_data.extra_data = component_info.extra_data;

					if (!component_info.function(&function_data)) {
						return true;
					}

					size_t current_offset = write_instrument->GetOffset();
					// Don't forget to subtract the write count from the write difference
					size_t write_difference = current_offset - unique_component_write_offset - sizeof(current_write_count);
					if (!EnsureUnsignedIntegerIsInRange<unsigned int>(write_difference)) {
						return true;
					}

					current_write_count = (unsigned int)write_difference;
					if (!write_instrument->WriteUninitializedData(unique_component_write_offset, &current_write_count, sizeof(current_write_count))) {
						return true;
					}
					unique_component_write_offset = current_offset;
				}
			}

			return false;
		})) {
			return false;
		}

		// After the new additions or updates have been serialized, we can safely return. The other data that was needed
		// Was serialized with the change set itself.
		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeserializeEntityManagerChangeSet(
		EntityManager* entity_manager,
		const DeserializeEntityManagerOptions* deserialize_options,
		const ReflectionManager* reflection_manager,
		ReadInstrument* read_instrument,
		DeserializeEntityManagerChangeSetOptions* options
	) {
		DeserializeEntityManagerChangeSetOptions default_options;
		if (options == nullptr) {
			options = &default_options;
		}

		// Handle the header section firstly
		ResizableLinearAllocator temporary_allocator;
		auto deallocate_header_section_data_allocator = StackScope([&]() {
			if (temporary_allocator.m_initial_capacity > 0) {
				temporary_allocator.Free();
			}
		});

		DeserializeEntityManagerHeaderSectionData header_section_data_storage;
		const DeserializeEntityManagerHeaderSectionData* header_section = options->deserialize_manager_header_section;
		if (header_section == nullptr) {
			temporary_allocator = ResizableLinearAllocator(DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY, DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY, ECS_MALLOC_ALLOCATOR);
			Optional<DeserializeEntityManagerHeaderSectionData> deserialized_header_section = DeserializeEntityManagerHeaderSection(
				read_instrument, 
				&temporary_allocator,
				deserialize_options
			);
			if (!deserialized_header_section.has_value) {
				return false;
			}

			header_section_data_storage = deserialized_header_section.value;
			header_section = &header_section_data_storage;
		}
		
		// Deserialize the change set itself. Prepare the deserialize options, in case they are not specified

		EntityManagerChangeSet change_set;
		DeserializeOptions default_change_set_deserialize_options;
		DeserializeOptions* change_set_deserialize_options = options->deserialize_change_set_options;
		if (options->deserialize_change_set_options == nullptr) {
			default_change_set_deserialize_options.error_message = deserialize_options->detailed_error_string;
			default_change_set_deserialize_options.default_initialize_missing_fields = true;
			// Use the same temporary allocator, in case the options do not specify the change set as an output
			if (options->change_set == nullptr) {
				if (temporary_allocator.m_initial_capacity == 0) {
					temporary_allocator = ResizableLinearAllocator(DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY, DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY, ECS_MALLOC_ALLOCATOR);
				}
				default_change_set_deserialize_options.field_allocator = &temporary_allocator;
			}
			else {
				default_change_set_deserialize_options.field_allocator = options->temporary_allocator;
			}
			change_set_deserialize_options = &default_change_set_deserialize_options;
		}

		if (Deserialize(
			reflection_manager, 
			reflection_manager->GetType(STRING(EntityManagerChangeSet)), 
			&change_set, 
			read_instrument, 
			change_set_deserialize_options
		) != ECS_DESERIALIZE_OK) {
			return false;
		}

		// Iterate over the global components and perform all types of actions
		if (change_set.global_component_changes.ForEach<true>([&](const EntityManagerChangeSet::GlobalComponentChange& change) -> bool {
			switch (change.type) {
			case ECS_CHANGE_SET_REMOVE:
			{
				entity_manager->UnregisterGlobalComponentCommit(change.component);
			}
			break;
			case ECS_CHANGE_SET_ADD:
			case ECS_CHANGE_SET_UPDATE:
			{
				const internal::SerializedCachedGlobalComponentInfo* cached_info = header_section->cached_global_infos.GetValuePtr(change.component);
				const DeserializeEntityManagerGlobalComponentInfo* component_info = cached_info->info;

				if (change.type == ECS_CHANGE_SET_ADD) {
					if (entity_manager->ExistsGlobalComponent(cached_info->found_at)) {
						ECS_FORMAT_ERROR_MESSAGE(
							deserialize_options->detailed_error_string,
							"Applying entity manager delta failed. Global component {#} already exists (it shouldn't according to the delta)",
							component_info->name
						);
						return true;
					}
				}
				else {
					if (!entity_manager->ExistsGlobalComponent(cached_info->found_at)) {
						ECS_FORMAT_ERROR_MESSAGE(
							deserialize_options->detailed_error_string,
							"Applying entity manage delta failed. Global component {#} doesn't exists (it should according to the delta)",
							component_info->name
						);
						return true;
					}

					// Remove the global component and register it once more with the deserialized data.
					// In this way, the deallocation of existing buffers is handled properly
					entity_manager->UnregisterGlobalComponentCommit(cached_info->found_at);
				}

				// Deserialize the component
				unsigned int write_size = 0;
				if (!read_instrument->Read(&write_size)) {
					ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(name_allocator, 512, ECS_MB);
					ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "Write size for global component {#} could not be read", header_section->GetGlobalComponentNameLookup(change.component, &name_allocator));
					return true;
				}

				alignas(void*) char global_component_storage[ECS_COMPONENT_MAX_BYTE_SIZE];

				// Push a new subinstrument, such that the component can reason about itself and we prevent
				// The component from reading overbounds
				ReadInstrument::SubinstrumentData subinstrument_data;
				auto subinstrument_deallocator = read_instrument->PushSubinstrument(&subinstrument_data, write_size);

				DeserializeEntityManagerComponentData function_data;
				function_data.components = global_component_storage;
				function_data.read_instrument = read_instrument;
				function_data.extra_data = component_info->extra_data;
				function_data.version = cached_info->version;
				// Global components have their allocator as the main entity manager allocator, since they
				// Might need a lot of memory
				function_data.component_allocator = entity_manager->MainAllocator();
				function_data.count = 1;

				// Now call the extract function
				bool is_data_valid = component_info->function(&function_data);
				if (!is_data_valid) {
					ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "The global component {#} has corrupted data", component_info->name);
					return true;
				}

				size_t current_read_count = read_instrument->GetOffset();
				// Ensure that the functor exhausted the entire data
				ECS_ASSERT(current_read_count == (size_t)write_size, "Change set global component deserialize failed to consume entire data");

				// Register the entry once more, for both the update case and the add case
				entity_manager->RegisterGlobalComponentCommit(
					cached_info->found_at,
					component_info->component_fixup.component_byte_size,
					global_component_storage,
					component_info->name,
					&component_info->component_fixup.component_functions
				);
			}
			break;
			default:
				ECS_ASSERT(false);
			}

			return false;
			})) {
			return false;
		}

		// Iterate over the shared components now
		if (change_set.shared_component_changes.ForEach<true>([&](const EntityManagerChangeSet::SharedComponentChanges& change) -> bool {
			for (size_t index = 0; index < change.changes.size; index++) {
				SharedInstance current_instance = change.changes[index].instance;

				switch (change.changes[index].type) {
				case ECS_CHANGE_SET_REMOVE:
				{
					entity_manager->UnregisterSharedInstanceCommit(change.component, current_instance);
				}
				break;
				case ECS_CHANGE_SET_ADD:
				case ECS_CHANGE_SET_UPDATE:
				{
					const internal::SerializedCachedSharedComponentInfo* cached_info = header_section->cached_shared_infos.GetValuePtr(change.component);
					const DeserializeEntityManagerSharedComponentInfo* component_info = cached_info->info;

					// Ensure that the instance exists/doesn't based on the current type
					if (change.changes[index].type == ECS_CHANGE_SET_ADD) {
						if (entity_manager->ExistsSharedInstance(cached_info->found_at, current_instance)) {
							ECS_FORMAT_ERROR_MESSAGE(
								deserialize_options->detailed_error_string,
								"Applying entity manager delta failed. Shared component {#} already contains an instance (it shouldn't according to the delta)",
								component_info->name
							);
							return true;
						}
					}
					else {
						if (!entity_manager->ExistsSharedInstance(cached_info->found_at, current_instance)) {
							ECS_FORMAT_ERROR_MESSAGE(
								deserialize_options->detailed_error_string,
								"Applying entity manage delta failed. Shared component {#} instance doesn't exists (it should according to the delta)",
								component_info->name
							);
							return true;
						}

						// Remove the shared instance and register it once more with the deserialized data.
						// In this way, the deallocation of existing buffers is handled properly
						entity_manager->UnregisterSharedInstanceCommit(cached_info->found_at, current_instance);
					}

					// Deserialize the shared instance
					unsigned int write_size = 0;
					if (!read_instrument->Read(&write_size)) {
						ECS_FORMAT_ERROR_MESSAGE(
							deserialize_options->detailed_error_string, 
							"Write size for shared component {#} instance could not be read", 
							component_info->name
						);
						return true;
					}

					size_t shared_instance_storage[ECS_COMPONENT_MAX_BYTE_SIZE];

					// Push a new subinstrument, such that the component can reason about itself and we prevent
					// The component from reading overbounds
					ReadInstrument::SubinstrumentData subinstrument_data;
					auto subinstrument_deallocator = read_instrument->PushSubinstrument(&subinstrument_data, write_size);

					DeserializeEntityManagerSharedComponentData function_data;
					function_data.component = shared_instance_storage;
					function_data.read_instrument = read_instrument;
					function_data.extra_data = component_info->extra_data;
					function_data.version = cached_info->version;
					function_data.component_allocator = entity_manager->GetSharedComponentAllocator(cached_info->found_at);
					function_data.data_size = write_size;
					function_data.instance = current_instance;

					// Now call the extract function
					bool is_data_valid = component_info->function(&function_data);
					if (!is_data_valid) {
						ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "Shared component {#} instance has corrupted data", component_info->name);
						return true;
					}

					size_t current_read_count = read_instrument->GetOffset();
					// Ensure that the functor exhausted the entire data
					ECS_ASSERT(current_read_count == (size_t)write_size, "Change set shared component deserialize failed to consume entire data");

					// Register the instance once more, for both the addition case and the normal update case
					entity_manager->RegisterSharedInstanceForValueCommit(
						cached_info->found_at,
						current_instance,
						shared_instance_storage,
						false
					);
				}
				break;
				default:
					ECS_ASSERT(false);
				}
			}

			return false;
		})) {
			return false;
		}
		
		// TODO: At the moment, named shared instances are not handled. Decide if we want to stick to them or not (eliminate them that is)?

		// Now the entity operations must be performed. 
		
		// Start deleting the entities which were destroyed.
		change_set.destroyed_entities.ForEachChunk([&](Stream<Entity> entities) {
			entity_manager->DeleteEntitiesCommit(entities);
		});

		// Returns true if it succeeded, else false. It will read the write size header to ensure that the read
		// Doesn't go overbounds.
		auto deserialize_unique_components = [&](Component component, void* components_storage, unsigned int component_count) -> bool {
			const internal::SerializedCachedComponentInfo* cached_info = header_section->cached_unique_infos.GetValuePtr(component);
			const DeserializeEntityManagerComponentInfo* deserialized_component_info = cached_info->info;

			unsigned int write_size = 0;
			if (!read_instrument->Read(&write_size)) {
				ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(name_allocator, 512, ECS_MB);
				ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "Write size for unique component {#} could not be read", header_section->GetComponentNameLookup(component, &name_allocator));
				return false;
			}

			// Push a new subinstrument, such that the component can reason about itself and we prevent
			// The component from reading overbounds
			ReadInstrument::SubinstrumentData subinstrument_data;
			auto subinstrument_deallocator = read_instrument->PushSubinstrument(&subinstrument_data, write_size);

			DeserializeEntityManagerComponentData function_data;
			function_data.components = components_storage;
			function_data.read_instrument = read_instrument;
			function_data.extra_data = deserialized_component_info->extra_data;
			function_data.version = cached_info->version;
			function_data.count = component_count;
			function_data.component_allocator = entity_manager->GetComponentAllocator(cached_info->found_at);

			// Now call the extract function
			bool is_data_valid = deserialized_component_info->function(&function_data);
			if (!is_data_valid) {
				ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "Unique component {#} has corrupted data", deserialized_component_info->name);
				return false;
			}

			size_t current_read_count = read_instrument->GetOffset();
			// Ensure that the functor exhausted the entire data
			ECS_ASSERT(current_read_count == (size_t)write_size, "Change set unique component deserialize failed to consume entire data");

			return true;
		};

		// Create the new entities - they are already preconditioned in a friendly format that allows an efficient creation
		if (change_set.new_entities.ForEach<true>([&](const EntityManagerChangeSet::NewEntitiesContainer& new_entities) {
			// Exclude these entities from the entity hierarchy - the entity hierarchy change set will take care of that.
			uint2 archetype_indices = entity_manager->CreateSpecificEntitiesCommit(
				new_entities.entities,
				new_entities.unique_components,
				{ new_entities.shared_components.buffer, new_entities.shared_instances.buffer, (unsigned char)new_entities.shared_components.size },
				true
			);

			// We can read their unique components right now
			ArchetypeBase* base_archetype = entity_manager->GetBase(archetype_indices.x, archetype_indices.y);

			// The component order of the serialized data is that of the new_entities.unique_components, so read the components
			// In that order.

			// The entities were added at the back of the container, so we can read directly into the component buffer,
			// No need for temporary buffers
			unsigned int added_entities_stream_index = base_archetype->m_size - new_entities.entities.size;
			for (size_t index = 0; index < new_entities.unique_components.size; index++) {
				unsigned char component_index = base_archetype->FindComponentIndex(new_entities.unique_components[index]);
				if (!deserialize_unique_components(new_entities.unique_components[index], base_archetype->GetComponentByIndex(added_entities_stream_index, component_index), new_entities.entities.size)) {
					return true;
				}
			}

			return false;
		})) {
			return false;
		}

		// After deletions and creations, we must handle the entities that existed in both the previous and new versions. We must take
		// Care of adding/removing/updating components.
		if (change_set.entity_component_changes.ForEach<true>([&](const EntityManagerChangeSet::EntityChanges& changes) {
			// To avoid having to move the entity across many archetypes because of multiple add/delete operations,
			// Use the bulk entity manager function such that we move the entity only once directly to its final archetype.

			Component added_unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
			Component removed_unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
			Component added_or_updated_shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
			SharedInstance added_or_updated_shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
			Component removed_shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

			PerformEntityComponentOperationsData operations_data;
			operations_data.added_unique_components = { added_unique_components, 0 };
			operations_data.removed_unique_components = { removed_unique_components, 0 };
			operations_data.added_or_updated_shared_components = { added_or_updated_shared_components, added_or_updated_shared_instances, 0 };
			operations_data.removed_shared_components = { removed_shared_components, 0 };

			// Perform the unique component assignment
			for (size_t index = 0; index < changes.unique_changes.size; index++) {
				if (changes.unique_changes[index].change_type == ECS_CHANGE_SET_ADD) {
					operations_data.added_unique_components[operations_data.added_or_updated_shared_components.count++] = changes.unique_changes[index].component;
				}
				else if (changes.unique_changes[index].change_type == ECS_CHANGE_SET_REMOVE) {
					operations_data.removed_unique_components[operations_data.removed_unique_components.count++] = changes.unique_changes[index].component;
				}
			}

			// Perform the shared component assignment
			for (size_t index = 0; index < changes.removed_shared_components.size; index++) {
				operations_data.removed_shared_components[operations_data.removed_shared_components.count++] = changes.removed_shared_components[index];
			}
			SharedComponentSignature& added_or_updated_shared_signature = operations_data.added_or_updated_shared_components;
			for (size_t index = 0; index < changes.shared_instance_changes.size; index++) {
				added_or_updated_shared_signature.indices[added_or_updated_shared_signature.count] = changes.shared_instance_changes[index].component;
				added_or_updated_shared_signature.instances[added_or_updated_shared_signature.count] = changes.shared_instance_changes[index].instance;
				added_or_updated_shared_signature.count++;
			}

			// The unique component data for the added components will be set after the perform component call, in order to deserialize
			// Directly into the component from the archetype. The call will handle the case where there is nothing to be changed
			entity_manager->PerformEntityComponentOperationsCommit(changes.entity, operations_data);

			EntityInfo entity_info = entity_manager->GetEntityInfo(changes.entity);
			ArchetypeBase* base_archetype = entity_manager->GetBase(entity_info);
			for (size_t index = 0; index < changes.unique_changes.size; index++) {
				if (changes.unique_changes[index].change_type == ECS_CHANGE_SET_ADD || changes.unique_changes[index].change_type == ECS_CHANGE_SET_UPDATE) {
					// There is a unique component to be deserialized
					if (!deserialize_unique_components(changes.unique_changes[index].component, base_archetype->GetComponentByIndex(entity_info, base_archetype->FindComponentIndex(changes.unique_changes[index].component)), 1)) {
						return true;
					}
				}
			}

			return false;
		})) {
			return false;
		}
		
		// Update the entity infos with the necessary changes
		change_set.entity_info_changes.ForEach([&](EntityManagerChangeSet::EntityInfoChange change) {
			// Even tho it's a little bit ugly, cast away the constness here
			EntityInfo* entity_info = (EntityInfo*)entity_manager->TryGetEntityInfo(change.entity);
			entity_info->generation_count = change.GetGenerationCount();
			entity_info->layer = change.GetLayer();
			entity_info->tags = change.GetTag();
		});

		// At last, apply the entity hierarchy change set
		ApplyEntityHierarchyChangeSet(&entity_manager->m_hierarchy, change_set.hierarchy_change_set);

		// Just to be sure, ensure that the entity infos remain stable
		ECS_ASSERT(entity_manager->ValidateEntityInfos(), "Applying entity manager delta change resulted in invalid entity info state!");

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}