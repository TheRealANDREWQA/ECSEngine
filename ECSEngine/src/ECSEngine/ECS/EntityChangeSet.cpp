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
		ComponentSignature previous_unique_signature = source_entity_manager->GetEntitySignatureStable(source_entity);
		ComponentSignature previous_shared_signature = source_entity_manager->GetEntitySharedSignatureStable(source_entity).ComponentSignature();

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
		// Firstly add to all the entities the components
		ECS_STACK_CAPACITY_STREAM(Component, components_to_be_added, ECS_ARCHETYPE_MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(const void*, components_to_be_added_data, ECS_ARCHETYPE_MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(Component, shared_components_to_be_added, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(SharedInstance, shared_instances_to_be_added, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);

		for (size_t index = 0; index < changes.size; index++) {
			if (changes[index].type == ECS_ENTITY_CHANGE_ADD) {
				if (changes[index].is_shared) {
					shared_components_to_be_added.Add(changes[index].component);
					unsigned char signature_index = shared_component_signature.Find(changes[index].component);
					ECS_ASSERT(signature_index != UCHAR_MAX);
					SharedInstance instance = entity_manager->GetOrCreateSharedComponentInstanceCommit(changes[index].component, shared_components[signature_index]);
					shared_instances_to_be_added.Add(instance);
				}
				else {
					components_to_be_added.Add(changes[index].component);
					unsigned char signature_index = unique_component_signature.Find(changes[index].component);
					ECS_ASSERT(signature_index != UCHAR_MAX);
					components_to_be_added_data.Add(unique_components[signature_index]);
				}
			}
		}

		/*entity_manager->AddComponentCommit(
			entities, 
			{ components_to_be_added.buffer, components_to_be_added.size }, 
			components_to_be_added_data.buffer, 
			ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT
		);
		entity_manager->AddSharedComponentCommit(
			entities,
			{ shared_components_to_be_added.buffer, shared_instances_to_be_added.buffer, shared_components_to_be_added.size },
		);*/
	}

}