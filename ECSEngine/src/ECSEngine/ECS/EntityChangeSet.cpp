#include "ecspch.h"
#include "EntityChangeSet.h"
#include "../Utilities/Reflection/Reflection.h"
#include "EntityManager.h"

namespace ECSEngine {

	bool DetermineUpdateChangesForComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* component_type,
		const void* previous_data,
		const void* new_data,
		EntityChange* change,
		AllocatorPolymorphic allocator
	) {
		ResizableStream<Reflection::ReflectionTypeChange> changes{ allocator, 0 };
		AdditionStream<Reflection::ReflectionTypeChange> addition_stream = &changes;
		Reflection::DetermineReflectionTypeInstanceUpdates(
			reflection_manager,
			component_type,
			previous_data,
			new_data,
			addition_stream
		);
		if (addition_stream.Size() > 0) {
			change->type = ECS_ENTITY_CHANGE_UPDATE;
			change->updated_fields = changes.ToStream();
			return true;
		}
		else {
			return false;
		}
	}

	void DetermineEntityChanges(
		const Reflection::ReflectionManager* reflection_manager,
		const EntityManager* source_entity_manager,
		const EntityManager* destination_entity_manager,
		Entity source_entity,
		Entity destination_entity,
		CapacityStream<EntityChange>* changes,
		AllocatorPolymorphic allocator
	) {
		// Firstly, determine the components that were added/removed and then
		// Those that have had their fields changed
		ComponentSignature previous_unique_signature;
		ComponentSignature previous_shared_signature;
		if (source_entity_manager->ExistsEntity(source_entity)) {
			previous_unique_signature = source_entity_manager->GetEntitySignatureStable(source_entity);
			previous_shared_signature = source_entity_manager->GetEntitySharedSignatureStable(source_entity).ComponentSignature();
		}

		ComponentSignature new_unique_signature = destination_entity_manager->GetEntitySignatureStable(destination_entity);
		ComponentSignature new_shared_signature = destination_entity_manager->GetEntitySharedSignatureStable(destination_entity).ComponentSignature();

		// Loop through the new components and determine the additions
		auto register_additions = [changes](ComponentSignature previous_components, ComponentSignature new_components, bool is_shared) {
			for (unsigned char index = 0; index < new_components.count; index++) {
				if (previous_components.Find(new_components[index]) == UCHAR_MAX) {
					unsigned int change_index = changes->Reserve();
					changes->buffer[change_index].type = ECS_ENTITY_CHANGE_ADD;
					changes->buffer[change_index].is_shared = is_shared;
					changes->buffer[change_index].component = new_components[index];
				}
			}
		};
		register_additions(previous_unique_signature, new_unique_signature, false);
		register_additions(previous_shared_signature, new_shared_signature, true);

		// Loop through the previous components and determine the removals
		auto register_removals = [changes](ComponentSignature previous_components, ComponentSignature new_components, bool is_shared) {
			for (unsigned char index = 0; index < previous_components.count; index++) {
				if (new_components.Find(previous_components[index]) == UCHAR_MAX) {
					unsigned int change_index = changes->Reserve();
					changes->buffer[change_index].type = ECS_ENTITY_CHANGE_REMOVE;
					changes->buffer[change_index].is_shared = is_shared;
					changes->buffer[change_index].component = previous_components[index];
				}
			}
		};
		register_removals(previous_unique_signature, new_unique_signature, false);
		register_removals(previous_shared_signature, new_shared_signature, true);

		// Now determine the updates. We need separate algorithms for unique and shared
		// Since the shared part is a little bit more involved
		auto register_updates = [&](ComponentSignature previous_components, ComponentSignature new_components, bool is_shared) {
			for (unsigned int index = 0; index < previous_components.count; index++) {
				unsigned char new_index = new_components.Find(previous_components[index]);
				if (new_index != UCHAR_MAX) {
					const void* previous_data = nullptr;
					const void* new_data = nullptr;
					const Reflection::ReflectionType* component_type = nullptr;
					EntityChange entity_change;
					entity_change.is_shared = is_shared;
					entity_change.component = new_components[new_index];

					if (is_shared) {
						previous_data = source_entity_manager->GetSharedComponent(source_entity, previous_components[index]);
						new_data = destination_entity_manager->GetSharedComponent(destination_entity, new_components[new_index]);
						component_type = reflection_manager->GetType(destination_entity_manager->GetSharedComponentName(new_components[new_index]));
					}
					else {
						previous_data = source_entity_manager->GetComponent(source_entity, previous_components[index]);
						new_data = destination_entity_manager->GetComponent(destination_entity, new_components[new_index]);
						component_type = reflection_manager->GetType(destination_entity_manager->GetComponentName(new_components[new_index]));
					}

					if (DetermineUpdateChangesForComponent(reflection_manager, component_type, previous_data, new_data, &entity_change, allocator)) {
						changes->AddAssert(entity_change);
					}
				}
			}
		};
		register_updates(previous_unique_signature, new_unique_signature, false);
		register_updates(previous_shared_signature, new_shared_signature, true);
	}

	void ApplyEntityChanges(
		const Reflection::ReflectionManager* reflection_manager, 
		EntityManager* entity_manager, 
		Stream<Entity> entities, 
		const void** unique_components, 
		ComponentSignature unique_component_signature, 
		const void** shared_components, 
		ComponentSignature shared_component_signature, 
		Stream<EntityChange> changes
	)
	{
		ECS_STACK_CAPACITY_STREAM(Component, components_to_be_added, ECS_ARCHETYPE_MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(const void*, components_to_be_added_data, ECS_ARCHETYPE_MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(Component, shared_components_to_be_added, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(SharedInstance, shared_instances_to_be_added, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(Component, components_to_be_removed, ECS_ARCHETYPE_MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(Component, shared_components_to_be_removed, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
		// We batch together the unique component updates for faster speed
		// We need to store the entity instead of the component since the entity
		// Might change archetypes and we will then have a dangling reference to a component
		ECS_STACK_CAPACITY_STREAM(Stream<Entity>, entities_to_be_updated, ECS_ARCHETYPE_MAX_COMPONENTS);
		entities_to_be_updated.size = unique_component_signature.count;
		for (size_t index = 0; index < unique_component_signature.count; index++) {
			entities_to_be_updated[index].Initialize(entity_manager->MainAllocator(), entities.size);
			entities_to_be_updated[index].size = 0;
		}

		ECS_STACK_CAPACITY_STREAM(Stream<Reflection::ReflectionTypeChange>, unique_component_changes, ECS_ARCHETYPE_MAX_COMPONENTS);
		memset(unique_component_changes.buffer, 0, unique_component_changes.MemoryOf(unique_component_changes.capacity));
		ECS_STACK_CAPACITY_STREAM(SharedInstance, shared_instances, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
		// Create or find all the shared components instances that correspond to the given data
		for (size_t index = 0; index < changes.size; index++) {
			Component component = changes[index].component;
			if (changes[index].is_shared) {
				if (changes[index].type == ECS_ENTITY_CHANGE_ADD || changes[index].type == ECS_ENTITY_CHANGE_UPDATE) {
					unsigned char signature_index = shared_component_signature.Find(component);
					ECS_ASSERT(signature_index != UCHAR_MAX);
					shared_instances[signature_index] = entity_manager->GetOrCreateSharedComponentInstanceCommit(component, shared_components[signature_index]);
				}
			}
			else {
				if (changes[index].type == ECS_ENTITY_CHANGE_UPDATE) {
					unsigned char signature_index = unique_component_signature.Find(component);
					ECS_ASSERT(signature_index != UCHAR_MAX);
					unique_component_changes[signature_index] = changes[index].updated_fields;
				}
			}
		}

		// Unfortunately, we cannot batch the updates since some entities might have overrides
		// So, so some entities might already have the components to be added, in that case we must
		// Treat this as an update, or some entities might be missing components that are missing,
		// in that case, it must be treated like an add
		for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
			// What we can do instead, is to batch the per entity changes
			// So all adds at once, all removals at once. Updates need to be batched separately
			// Such that we perform few reflection traversals of the types
			Entity current_entity = entities[entity_index];
			for (size_t index = 0; index < changes.size; index++) {
				Component current_component = changes[index].component;
				// The update and add can be commased into the same check since we need to make sure
				// That the integrity is there
				if (changes[index].type == ECS_ENTITY_CHANGE_ADD || changes[index].type == ECS_ENTITY_CHANGE_UPDATE) {
					if (changes[index].is_shared) {
						unsigned char signature_index = shared_component_signature.Find(current_component);
						ECS_ASSERT(signature_index != UCHAR_MAX);
						if (!entity_manager->HasSharedComponent(current_entity, current_component)) {
							shared_components_to_be_added.Add(current_component);
							shared_instances_to_be_added.Add(shared_instances[signature_index]);
						}
						else {
							// Treat this as an update
							entity_manager->ChangeEntitySharedInstanceCommit(current_entity, current_component, shared_instances[signature_index]);
						}
					}
					else {
						unsigned char signature_index = unique_component_signature.Find(current_component);
						ECS_ASSERT(signature_index != UCHAR_MAX);
						if (!entity_manager->HasComponent(current_entity, current_component)) {
							components_to_be_added.Add(current_component);
							components_to_be_added_data.Add(unique_components[signature_index]);
						}
						else {
							// Treat this as an update
							entities_to_be_updated[signature_index].Add(current_entity);
						}
					}
				}
				else if (changes[index].type == ECS_ENTITY_CHANGE_REMOVE) {
					// Perform the removal only if the component actually exists
					if (changes[index].is_shared) {
						if (entity_manager->HasSharedComponent(current_entity, current_component)) {
							shared_components_to_be_removed.Add(current_component);
						}
					}
					else {
						if (entity_manager->HasComponent(current_entity, current_component)) {
							components_to_be_removed.Add(current_component);
						}
					}
				}
			}

			// Perform the batch additions and removals
			if (components_to_be_added.size > 0) {
				entity_manager->AddComponentCommit(
					current_entity,
					{ components_to_be_added.buffer, (unsigned char)components_to_be_added.size },
					components_to_be_added_data.buffer
				);
				components_to_be_added.size = 0;
			}
			if (shared_components_to_be_added.size > 0) {
				entity_manager->AddSharedComponentCommit(
					current_entity,
					{ shared_components_to_be_added.buffer, shared_instances_to_be_added.buffer, (unsigned char)shared_components_to_be_added.size }
				);
				shared_components_to_be_added.size = 0;
			}
			if (components_to_be_removed.size > 0) {
				entity_manager->RemoveComponentCommit(
					current_entity,
					{ components_to_be_removed.buffer, (unsigned char)components_to_be_removed.size }
				);
				components_to_be_removed.size = 0;
			}
			if (shared_components_to_be_removed.size > 0) {
				entity_manager->RemoveSharedComponentCommit(
					current_entity,
					{ shared_components_to_be_removed.buffer, (unsigned char)shared_components_to_be_removed.size }
				);
				shared_components_to_be_removed.size = 0;
			}
		}

		// At the end, remove all the unreferenced instances
		// And perform the batched updates + deallocation of the buffer
		for (unsigned char index = 0; index < shared_component_signature.count; index++) {
			entity_manager->UnregisterUnreferencedSharedInstancesCommit(shared_component_signature[index]);
		}
		for (unsigned char index = 0; index < unique_component_signature.count; index++) {
			if (entities_to_be_updated[index].size > 0) {
				if (unique_component_changes[index].size == 0) {
					// This is the case where the component is added
					// But there are entities with that already existing component
					// And they get put into the update stream
					// For these components, we simply have to overwrite them
					for (size_t subindex = 0; subindex < entities_to_be_updated[index].size; subindex++) {
						entity_manager->SetEntityComponentCommit(entities_to_be_updated[index][subindex], unique_component_signature[index], unique_components[index]);
					}
				}
				else {
					Stream<void*> components_to_be_updated;
					components_to_be_updated.Initialize(entity_manager->MainAllocator(), entities_to_be_updated[index].size);
					for (size_t subindex = 0; subindex < entities_to_be_updated[index].size; subindex++) {
						components_to_be_updated[subindex] = entity_manager->GetComponent(entities_to_be_updated[index][subindex], unique_component_signature[index]);
					}
					const Reflection::ReflectionType* reflection_type = reflection_manager->GetType(entity_manager->GetComponentName(unique_component_signature[index]));
					Reflection::UpdateReflectionTypeInstancesFromChanges(
						reflection_manager,
						reflection_type,
						components_to_be_updated,
						unique_components[index],
						unique_component_changes[index],
						entity_manager->GetComponentAllocatorPolymorphic(unique_component_signature[index])
					);
					components_to_be_updated.Deallocate(entity_manager->MainAllocator());
				}
			}
			if (entities_to_be_updated[index].buffer != nullptr) {
				Deallocate(entity_manager->MainAllocator(), entities_to_be_updated[index].buffer);
			}
		}
	}

}