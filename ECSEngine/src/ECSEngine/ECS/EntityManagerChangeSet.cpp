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
// For containers where the expected number of entries is smaller, use a smaller exponent, in this case 64 elements
#define DECK_POWER_OF_TWO_EXPONENT_SMALL 6

#define DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY (ECS_MB * 50)

using namespace ECSEngine::Reflection;

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	void EntityManagerChangeSet::Initialize(AllocatorPolymorphic allocator) {
		entity_unique_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_destroys.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_additions_entity.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_additions_entity_info.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		shared_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		global_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);

		destroyed_archetypes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT_SMALL);
		new_archetypes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT_SMALL);
		moved_archetypes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT_SMALL);
		base_archetype_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT_SMALL);
	}

	void EntityManagerChangeSet::Deallocate() {
		// The unique and shared changes have their own buffers
		entity_unique_component_changes.ForEach([&](const EntityChanges& changes) {
			changes.unique_changes.Deallocate(Allocator());
			changes.removed_shared_components.Deallocate(Allocator());
			changes.shared_instance_changes.Deallocate(Allocator());
			return false;
		});
		shared_component_changes.ForEach([&](const SharedComponentChanges& changes) {
			changes.changes.Deallocate(Allocator());
			return false;
		});

		entity_unique_component_changes.Deallocate();
		entity_info_changes.Deallocate();
		entity_info_destroys.Deallocate();
		entity_info_additions_entity.Deallocate();
		entity_info_additions_entity_info.Deallocate();
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
					change_set.entity_info_additions_entity.Add(entity);
					change_set.entity_info_additions_entity_info.Add(new_entity_info);
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
				change_set.entity_info_additions_entity.Add(entity);
				change_set.entity_info_additions_entity_info.Add(entity_info);
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

		// TODO: This must be changed - the way it is computed it is vastly different!

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
		bool write_entity_manager_header_section
	) {
		// Firstly, write the change set itself, the structure that describes what changes need to be performed,
		// Then write the component data that was changed

		if (write_entity_manager_header_section) {
			if (!SerializeEntityManagerHeaderSection(new_entity_manager, write_instrument, serialize_options)) {
				return false;
			}
		}

		// TODO: This must be changed - the deserialization was vastly changed!

		// Use the reflection manager for that, it should handle this successfully
		if (Serialize(reflection_manager, reflection_manager->GetType(STRING(EntityManagerChangeSet)), &change_set, write_instrument) != ECS_SERIALIZE_OK) {
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

		// Now write the unique components, per entity
		size_t unique_component_write_offset = write_instrument->GetOffset();
		if (change_set->entity_unique_component_changes.ForEach<true>([&](const EntityManagerChangeSet::EntityChanges& changes) -> bool {
			for (size_t index = 0; index < changes.unique_changes.size; index++) {
				if (changes.unique_changes[index].change_type == ECS_CHANGE_SET_ADD || changes.unique_changes[index].change_type == ECS_CHANGE_SET_UPDATE) {
					// Add a prefix size for how long the serialization of these components is
					unsigned int current_write_count = 0;
					if (!write_instrument->AppendUninitialized(sizeof(current_write_count))) {
						return true;
					}

					const SerializeEntityManagerComponentInfo& component_info = *serialize_options->component_table->GetValuePtr(changes.unique_changes[index].change_type);
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
		
		AllocatorPolymorphic function_temporary_allocator;

		// Deserialize the change set itself
		EntityManagerChangeSet change_set;
		DeserializeOptions change_set_deserialize_options;
		change_set_deserialize_options.error_message = deserialize_options->detailed_error_string;
		change_set_deserialize_options.default_initialize_missing_fields = true;
		// Use the same temporary allocator, in case the options do not specify the change set as an output
		if (options->change_set == nullptr) {
			if (temporary_allocator.m_initial_capacity == 0) {
				temporary_allocator = ResizableLinearAllocator(DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY, DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY, ECS_MALLOC_ALLOCATOR);
			}
			change_set_deserialize_options.field_allocator = &temporary_allocator;
			function_temporary_allocator = &temporary_allocator;
		}
		else {
			change_set_deserialize_options.field_allocator = options->temporary_allocator;
			function_temporary_allocator = options->temporary_allocator;
		}
		if (Deserialize(
			reflection_manager, 
			reflection_manager->GetType(STRING(EntityManagerChangeSet)), 
			&change_set, 
			read_instrument, 
			&change_set_deserialize_options
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
				const internal::CachedGlobalComponentInfo* cached_info = header_section->cached_global_infos.GetValuePtr(change.component);
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
					ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "Write size for global component {#} could not be read", header_section->GetGlobalComponentName(change.component));
					return true;
				}

				size_t global_component_storage[ECS_KB * 4];
				ECS_ASSERT(write_size <= sizeof(global_component_storage));

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
					const internal::CachedSharedComponentInfo* cached_info = header_section->cached_shared_infos.GetValuePtr(change.component);
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

					size_t shared_instance_storage[ECS_KB * 4];
					ECS_ASSERT(write_size <= sizeof(shared_instance_storage));

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
						ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "A shared component {#} instance has corrupted data", component_info->name);
						return true;
					}

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

		// Perform the archetype handling - which is the most complicated part. Start with the main archetypes
		// Define an apply archetype change interface to perform the main archetype handling.
		// One important observation!! Instead of actually destroying the archetype directly using the
		// entity manager destroy call, put that archetype into a temporary array and don't deallocate its buffers.
		// The reason for this is the following: there can be entities which belong to this archetype but they have
		// Been moved to a different archetype. Destroying the archetype would destroy these entities as well, and
		// We will lose their data. So, just remove swap it back inside the entity manager's array.
		ResizableStream<Archetype> archetypes_pending_deletion(entity_manager->TemporaryAllocatorSingleThreaded(), 8);

		struct ApplyArchetypeChange : ApplyArrayDeltaChangeInterface<unsigned short> {
			ApplyArchetypeChange(EntityManager* _entity_manager, const EntityManagerChangeSet* _change_set, ResizableStream<Archetype>* _archetypes_pending_deletion) : entity_manager(_entity_manager),
				change_set(_change_set), archetypes_pending_deletion(_archetypes_pending_deletion), new_archetypes_iterator(change_set->new_archetypes.ConstIterator()), 
				moved_archetypes_iterator(change_set->moved_archetypes.ConstIterator()),
				removed_archetypes_iterator(change_set->destroyed_archetypes.ConstIterator()) {}

			virtual void RemoveEntry(unsigned short index) override {
				archetypes_pending_deletion->Add(entity_manager->GetArchetype(index));
				
				entity_manager->m_archetypes.RemoveSwapBack(index);
				unsigned int last_archetype_index = entity_manager->m_archetypes.size;
				*entity_manager->GetArchetypeUniqueComponentsPtr(index) = *entity_manager->GetArchetypeUniqueComponentsPtr(last_archetype_index);
				*entity_manager->GetArchetypeSharedComponentsPtr(index) = *entity_manager->GetArchetypeSharedComponentsPtr(last_archetype_index);
				//entity_manager->DestroyArchetypeCommit(index);
			}

			virtual unsigned short CreateNewEntry() override {
				const EntityManagerChangeSet::NewArchetype* new_archetype = new_archetypes_iterator.Get();
				entity_manager->CreateArchetypeCommit(new_archetype->unique_signature, new_archetype->shared_signature);
				return new_archetype->index;
			}
			
			virtual void MoveEntry(unsigned short previous_index, unsigned short new_index) override {
				entity_manager->SwapArchetypeCommit(previous_index, new_index);
			}

			virtual IteratorInterface<const MovedElementIndex<unsigned short>>* GetMovedEntries() override {
				return &moved_archetypes_iterator;
			}

			virtual IteratorInterface<const unsigned short>* GetRemovedEntries() override {
				return &removed_archetypes_iterator;
			}

			virtual unsigned short GetNewEntryCount() override {
				return change_set->new_archetypes.size;
			}

			virtual unsigned short GetContainerSize() override {
				return entity_manager->m_archetypes.size;
			}

			EntityManager* entity_manager;
			const EntityManagerChangeSet* change_set;
			ResizableStream<Archetype>* archetypes_pending_deletion;
			// Needed for the function CreateNewEntry.
			DeckPowerOfTwo<EntityManagerChangeSet::NewArchetype>::ConstIteratorType new_archetypes_iterator;
			DeckPowerOfTwo<MovedElementIndex<unsigned short>>::ConstIteratorType moved_archetypes_iterator;
			DeckPowerOfTwo<unsigned short>::ConstIteratorType removed_archetypes_iterator;
		};

		ApplyArchetypeChange apply_archetype_change(entity_manager, &change_set, &archetypes_pending_deletion);
		struct PendingArchetypeBaseDeletion {
			// This index can be used to directly index into the appropriate archetype, since
			// The delta change for the main archetypes has been applied.
			unsigned int main_archetype_index;
			Archetype::InternalBase base;
		};
		// Similarly to main archetypes, we must record the pending deletions but not actually perform them, only fake their deletion
		// Such that we can recover the entity data of entities moved out this archetype.
		ResizableStream<PendingArchetypeBaseDeletion> archetypes_base_pending_deletion(entity_manager->TemporaryAllocatorSingleThreaded(), 8);

		// Use the marker to reset the linear allocator gracefully
		AllocatorMarker apply_archetype_change_allocator_marker = GetAllocatorMarker(function_temporary_allocator);
		ApplyArrayDeltaChange(&apply_archetype_change, function_temporary_allocator);
		RestoreAllocatorMarker(function_temporary_allocator, apply_archetype_change_allocator_marker);

		// Define an apply archetype base change interface to handle the base archetypes delta.
		struct ApplyArchetypeBaseChange : ApplyArrayDeltaChangeInterface<unsigned short> {
			ApplyArchetypeBaseChange(
				EntityManager* _entity_manager,
				ResizableStream<PendingArchetypeBaseDeletion>* _archetypes_base_pending_deletion,
				const EntityManagerChangeSet::BaseArchetypeChanges& base_archetype_changes
			) : entity_manager(_entity_manager), main_archetype_index(base_archetype_changes.archetype),
				archetypes_base_pending_deletion(_archetypes_base_pending_deletion),
				new_entries_count(base_archetype_changes.new_base.size),
				new_entries_iterator(base_archetype_changes.new_base.ConstIterator()),
				moved_entries_iterator(base_archetype_changes.moved_base.ConstIterator()),
				removed_entries_iterator(base_archetype_changes.destroyed_base.ConstIterator()) {}

			virtual void RemoveEntry(unsigned short index) override {
				Archetype* archetype = entity_manager->GetArchetype(main_archetype_index);
				archetypes_base_pending_deletion->Add({ main_archetype_index, archetype->m_base_archetypes[index] });
				archetype->m_base_archetypes.RemoveSwapBack(index);

				//entity_manager->DestroyArchetypeBaseCommit(main_archetype_index, index);
			}

			virtual unsigned short CreateNewEntry() override {
				const EntityManagerChangeSet::NewArchetypeBase* new_archetype = new_entries_iterator.Get();
				ComponentSignature shared_signature = entity_manager->GetArchetype(main_archetype_index)->GetUniqueSignature();
				entity_manager->CreateArchetypeBaseCommit(main_archetype_index, SharedComponentSignature(shared_signature.indices, new_archetype->instances.buffer, shared_signature.count));
				return new_archetype->index;
			}

			virtual void MoveEntry(unsigned short previous_index, unsigned short new_index) override {
				entity_manager->SwapArchetypeBaseCommit(main_archetype_index, previous_index, new_index);
			}

			virtual IteratorInterface<const MovedElementIndex<unsigned short>>* GetMovedEntries() override {
				return &moved_entries_iterator;
			}

			virtual IteratorInterface<const unsigned short>* GetRemovedEntries() override {
				return &removed_entries_iterator;
			}

			virtual unsigned short GetNewEntryCount() override {
				return new_entries_count;
			}

			virtual unsigned short GetContainerSize() override {
				return entity_manager->GetArchetype(main_archetype_index)->GetBaseCount();
			}

			EntityManager* entity_manager;
			ResizableStream<PendingArchetypeBaseDeletion>* archetypes_base_pending_deletion;
			// This is the main index of the archetype inside entity manager's archetype array
			unsigned int main_archetype_index;
			// This value is cached such that the initial values is always returned, since new_entries_iterator
			// Will advance the remaining_count as the entries are created.
			unsigned int new_entries_count;
			StreamIterator<const EntityManagerChangeSet::NewArchetypeBase> new_entries_iterator;
			StreamIterator<const MovedElementIndex<unsigned short>> moved_entries_iterator;
			StreamIterator<const unsigned short> removed_entries_iterator;
		};

		// We can reuse the apply archetype change allocator here as well
		change_set.base_archetype_changes.ForEach([&](const EntityManagerChangeSet::BaseArchetypeChanges& base_archetype_changes) {
			ApplyArchetypeBaseChange archetype_base_change(entity_manager, &archetypes_base_pending_deletion, base_archetype_changes);

			AllocatorMarker apply_archetype_change_allocator_marker = GetAllocatorMarker(function_temporary_allocator);
			ApplyArrayDeltaChange(&archetype_base_change, function_temporary_allocator);
			RestoreAllocatorMarker(function_temporary_allocator, apply_archetype_change_allocator_marker);
			return false;
		});

		// Now a very complex process must be performed in order to determine the unique components which have changed for each entity
		// And which archetype is now the proper archetype for each entity, while also taking into account pending archetype deletions. 
		// The source of this data is the entity info change. We can take the previous archetype's components and make the delta compared to the new one.

		// Step 1. Reinsert the pending deleted main archetypes. Add them to the end of the array, since this will not disturb any indices.
		for (unsigned int index = 0; index < archetypes_pending_deletion.size; index++) {
			const Archetype* archetype = &archetypes_pending_deletion[index];
			unsigned int reinsertion_index = entity_manager->m_archetypes.Add(archetype);
			entity_manager->GetArchetypeUniqueComponentsPtr(reinsertion_index)->ConvertComponents(archetypes_pending_deletion[index].GetUniqueSignature());
			entity_manager->GetArchetypeSharedComponentsPtr(reinsertion_index)->ConvertComponents(archetypes_pending_deletion[index].GetSharedSignature());
			
			// We must update the entity infos of all entities that belong to this archetype.
			for (unsigned int base_index = 0; base_index < archetype->GetBaseCount(); base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				for (unsigned int entity_index = 0; entity_index < base->EntityCount(); entity_index++) {
					EntityInfo* entity_info = entity_manager->m_entity_pool->GetInfoPtrNoChecks(base->m_entities[entity_index]);
					entity_info->main_archetype = reinsertion_index;
				}
			}
		}

		// Step 2. Reinsert the pending deleted base archetypes. Add them to the end of the array of the archetype, since this will not disturb any indices
		for (unsigned int index = 0; index < archetypes_base_pending_deletion.size; index++) {
			// The order in which these were inserted guarantees that the entries from the same main archetype are consecutive
			unsigned int initial_same_archetype_index = index;
			while (index < archetypes_base_pending_deletion.size - 1 && archetypes_base_pending_deletion[index].main_archetype_index == archetypes_base_pending_deletion[index + 1].main_archetype_index) {
				index++;
			}

			Archetype* archetype = entity_manager->GetArchetype(archetypes_base_pending_deletion[initial_same_archetype_index].main_archetype_index);
			for (unsigned int subindex = initial_same_archetype_index; subindex <= index; subindex++) {
				unsigned int reinsertion_index = archetype->m_base_archetypes.Add(archetypes_base_pending_deletion[subindex].base);
			
				// Similarily to main archetypes, we must update the entity infos of all entities that belong to this base archetype.
				const ArchetypeBase* base = archetype->GetBase(reinsertion_index);
				for (unsigned int entity_index = 0; entity_index < base->EntityCount(); entity_index++) {
					EntityInfo* entity_info = entity_manager->m_entity_pool->GetInfoPtrNoChecks(base->m_entities[entity_index]);
					entity_info->base_archetype = reinsertion_index;
				}
			}
		}

		// Step 3. After the reinsertion took place, we can perform the per entity operations.
		// Start off with the entity deletions.
		change_set.entity_info_destroys.ForEachChunk([&](Stream<Entity> entities) {
			entity_manager->DeleteEntitiesCommit(entities);
		});

		// Step 4. Create the new entities that need to be created - the appropriate archetypes are all set.
		// These new entities will be created into the archetype without components. In case it didn't exist before,
		// We will need to destroy it at the end of the function. We need to keep track of the entities which could
		// Not be placed at their stream index, because of the creation order. Store them and move them later, after
		// All additions have been performed.
		size_t entity_create_index = 0;
		AllocatorMarker add_entities_allocator_marker = GetAllocatorMarker(function_temporary_allocator);
		
		struct CreatedEntityPendingStreamRepair {
			Entity entity;
			unsigned int desired_index;
		};
		ResizableStream<CreatedEntityPendingStreamRepair> created_entities_pending_stream_repair(function_temporary_allocator, 16);

		// This function swaps the entity from the source to the target index, and the target to the source.
		// You can choose if the source components should be copied to the target, thet target to source is always performed.
		auto swap_base_archetype_entity_to_location = [](ArchetypeBase* base_archetype, unsigned int target_index, unsigned int source_index, bool copy_source_components) {
			swap(base_archetype->m_entities[target_index], base_archetype->m_entities[source_index]);
			if (copy_source_components) {
				size_t temporary_memory[ECS_COMPONENT_MAX_BYTE_SIZE / sizeof(size_t)];
				for (unsigned char component_index = 0; component_index < base_archetype->m_components.count; component_index++) {
					unsigned int component_size = base_archetype->m_infos[base_archetype->m_components[component_index]].size;
					swap(
						base_archetype->GetComponentByIndex(source_index, component_index), 
						base_archetype->GetComponentByIndex(target_index, component_index), 
						temporary_memory, 
						component_size
					);
				}
			}
			else {
				for (unsigned char component_index = 0; component_index < base_archetype->m_components.count; component_index++) {
					memcpy(
						base_archetype->GetComponentByIndex(source_index, component_index), 
						base_archetype->GetComponentByIndex(target_index, component_index), 
						base_archetype->m_infos[base_archetype->m_components[component_index]].size
					);
				}
			}
		};

		auto new_entity_infos_iterator = change_set.entity_info_additions_entity_info.ConstIterator();
		change_set.entity_info_additions_entity.ForEachChunk([&](Stream<Entity> entities) {
			// Create these entities by bypassing the entity manager - since it will force us
			// To set the entities into an archetype that will be later on moved from. The archetype
			// Indices passed to this function don't matter, they are used just to initialize the entity
			// Infos, but we will update them afterwards.
			entity_manager->m_entity_pool->AllocateSpecific(entities, { 0,0 }, 0);
			
			// Now that the entities are allocated, write their appropriate entity infos
			// And write them into their respective archetypes.
			for (size_t index = 0; index < entities.size; index++) {
				EntityInfo current_info = change_set.entity_info_additions_entity_info[entity_create_index];
				*entity_manager->m_entity_pool->GetInfoPtrNoChecks(index) = current_info;
				entity_create_index++;

				ArchetypeBase* base_archetype = entity_manager->GetBase(current_info);
				unsigned int added_index = base_archetype->AddEntity(entities[index]);
				if (added_index != current_info.stream_index) {
					if (current_info.stream_index < base_archetype->EntityCount()) {
						// The entity can be placed at its preferred location. Swap it there
						swap_base_archetype_entity_to_location(base_archetype, current_info.stream_index, added_index, false);
					}
					else {
						// Add this to the pending stream repair. We must use store the desired stream index
						// Since this entity in of itself can be swapped, for this reason, update the entity info
						// Of this entry to reflect the real current index.
						entity_manager->m_entity_pool->GetInfoPtrNoChecks(index)->stream_index = added_index;
						created_entities_pending_stream_repair.Add({ entities[index], (unsigned int)current_info.stream_index });
					}
				}
			}
		});

		// Perform the pending stream repair now
		for (unsigned int index = 0; index < created_entities_pending_stream_repair.size; index++) {
			EntityInfo* entity_info = entity_manager->m_entity_pool->GetInfoPtrNoChecks(created_entities_pending_stream_repair[index].entity);
			ArchetypeBase* base_archetype = entity_manager->GetBase(*entity_info);
			swap_base_archetype_entity_to_location(base_archetype, created_entities_pending_stream_repair[index].desired_index, entity_info->stream_index, false);
		}

		RestoreAllocatorMarker(function_temporary_allocator, add_entities_allocator_marker);

		// Step 5. After this has been done, the unique components of the newly created entities must be reconstructed.
		// These have been written in the same order as the new entities in the change set.
		entity_create_index = 0;
		if (change_set.entity_info_additions_entity.ForEach<true>([&](Entity entity) {
			// It is probably faster to access the entity info from the parallel stream than going through
			// The entity pool since it has better locality.
			EntityInfo entity_info = change_set.entity_info_additions_entity_info[entity_create_index];
			entity_create_index;

			ArchetypeBase* archetype_base = entity_manager->GetBase(entity_info);
			// The components are serialized in the same order as the base.
			for (unsigned char component_index = 0; component_index < archetype_base->m_components.count; component_index++) {
				Component component = archetype_base->m_components[component_index];

				const internal::CachedComponentInfo* cached_info = header_section->cached_unique_infos.GetValuePtr(component);
				const DeserializeEntityManagerComponentInfo* deserialized_component_info = cached_info->info;

				unsigned int write_size = 0;
				if (!read_instrument->Read(&write_size)) {
					ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "Write size for unique component {#} could not be read", header_section->GetComponentName(component));
					return true;
				}

				// Push a new subinstrument, such that the component can reason about itself and we prevent
				// The component from reading overbounds
				ReadInstrument::SubinstrumentData subinstrument_data;
				auto subinstrument_deallocator = read_instrument->PushSubinstrument(&subinstrument_data, write_size);

				DeserializeEntityManagerComponentData function_data;
				function_data.components = archetype_base->GetComponentByIndex(entity_info.stream_index, component_index);
				function_data.read_instrument = read_instrument;
				function_data.extra_data = deserialized_component_info->extra_data;
				function_data.version = cached_info->version;
				function_data.count = 1;
				function_data.component_allocator = entity_manager->GetComponentAllocator(cached_info->found_at);

				// Now call the extract function
				bool is_data_valid = deserialized_component_info->function(&function_data);
				if (!is_data_valid) {
					ECS_FORMAT_ERROR_MESSAGE(deserialize_options->detailed_error_string, "A unique component {#} has corrupted data", deserialized_component_info->name);
					return true;
				}
			}

			return false;
		})) {
			return false;
		}

		// Step 6.


		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}