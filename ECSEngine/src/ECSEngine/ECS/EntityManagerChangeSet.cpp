#include "ecspch.h"
#include "EntityManagerChangeSet.h"
#include "../Utilities/Reflection/Reflection.h"
#include "EntityManager.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "EntityManagerSerialize.h"
#include "../Utilities/ReaderWriterInterface.h"

// 256
#define DECK_POWER_OF_TWO_EXPONENT 8

#define DESERIALIZE_CHANGE_SET_TEMPORARY_ALLOCATOR_CAPACITY (ECS_MB * 50)

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
		}
		else {
			change_set_deserialize_options.field_allocator = options->temporary_allocator;
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
				auto subinstrument = read_instrument->PushSubinstrument(&subinstrument_data, write_size);

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
					auto subinstrument = read_instrument->PushSubinstrument(&subinstrument_data, write_size);

					DeserializeEntityManagerSharedComponentData function_data;
					function_data.component = shared_instance_storage;
					function_data.read_instrument = read_instrument;
					function_data.extra_data = component_info->extra_data;
					function_data.version = cached_info->version;
					// Global components have their allocator as the main entity manager allocator, since they
					// Might need a lot of memory
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

		// Now perform the per entity related operations.
		// Start with destroying the entities which are for sure gone
		for (size_t index = 0; index < change_set.entity_info_destroys.buffers.size; index++) {
			entity_manager->DeleteEntitiesCommit(change_set.entity_info_destroys.buffers[index]);
		}

		//entity_manager->CreateEntitiesCommit()

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}