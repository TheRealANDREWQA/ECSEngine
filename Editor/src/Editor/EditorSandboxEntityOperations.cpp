#include "editorpch.h"
#include "EditorSandboxEntityOperations.h"
#include "EditorSandbox.h"
#include "EditorState.h"

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------------------------

const EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_index)
{
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_index);
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (state == EDITOR_SANDBOX_SCENE) {
		return &sandbox->scene_entities;
	}
	return sandbox->sandbox_world.entity_manager;
}

// ------------------------------------------------------------------------------------------------------------------------------

EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_index) {
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_index);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (state == EDITOR_SANDBOX_SCENE) {
		return &sandbox->scene_entities;
	}
	return sandbox->sandbox_world.entity_manager;
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	Component component = editor_state->editor_components.GetComponentID(component_name);
	if (component.value != -1) {
		if (entity_manager->ExistsEntity(entity)) {
			entity_manager->AddComponentCommit(entity, component);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add component {#} to entity {#}.", component_name, entity.value);
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntitySharedComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	Component component = editor_state->editor_components.GetComponentID(component_name);
	if (component.value != -1) {
		if (entity_manager->ExistsEntity(entity)) {
			entity_manager->AddSharedComponentCommit(entity, component, SharedInstance{ 0 });
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add shared component {#} to entity {#}.", component_name, entity.value);
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index)
{
	return CreateSandboxEntity(editor_state, sandbox_index, {}, {});
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, ComponentSignature unique, SharedComponentSignature shared)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	return entity_manager->CreateEntityCommit(unique, shared);
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CopySandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	if (entity_manager->ExistsEntity(entity)) {
		ComponentSignature unique = EntityUniqueComponents(editor_state, sandbox_index, entity);
		SharedComponentSignature shared = EntitySharedInstances(editor_state, sandbox_index, entity);
		Entity destination_entity = CreateSandboxEntity(editor_state, sandbox_index, unique, shared);

		// Now copy the unique data
		for (unsigned char index = 0; index < unique.count; index++) {
			void* source_data = entity_manager->GetComponent(entity, unique.indices[index]);
			void* destination_data = entity_manager->GetComponent(destination_entity, unique.indices[index]);

			unsigned short component_size = entity_manager->ComponentSize(unique.indices[index]);
			memcpy(destination_data, source_data, component_size);
		}

		// Now copy the hierarchies it is in
		ECS_STACK_CAPACITY_STREAM(unsigned int, entity_hierarchies, ECS_ENTITY_HIERARCHY_MAX_COUNT);
		entity_manager->GetEntityHierarchies(entity, entity_hierarchies);
		for (unsigned int index = 0; index < entity_hierarchies.size; index++) {
			Entity current_parent = entity_manager->GetEntityParent(entity_hierarchies[index], entity);

			EntityPair pair;
			pair.child = destination_entity;
			pair.parent = current_parent;
			entity_manager->AddEntityToHierarchyCommit(entity_hierarchies[index], { &pair, 1 });
		}

		return destination_entity;
	}
	else {
		return { (unsigned int)-1 };
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void DeleteSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	// Be safe. If for some reason the UI lags behind and the runtime might delete the entity before us
	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->DeleteEntityCommit(entity);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

ComponentSignature EntityUniqueComponents(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	if (entity_manager->ExistsEntity(entity)) {
		return entity_manager->GetEntitySignatureStable(entity);
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

ComponentSignature EntitySharedComponents(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	if (entity_manager->ExistsEntity(entity)) {
		SharedComponentSignature signature = entity_manager->GetEntitySharedSignatureStable(entity);
		return { signature.indices, signature.count };
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedComponentSignature EntitySharedInstances(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	if (entity_manager->ExistsEntity(entity)) {
		SharedComponentSignature signature = entity_manager->GetEntitySharedSignatureStable(entity);
		return signature;
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity GetSandboxEntity(const EditorState* editor_state, unsigned int sandbox_index, Stream<char> name)
{
	const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	Entity entity = StringToEntity(name);
	// Check to see that the entity exists in that sandbox
	if (!entity_manager->ExistsEntity(entity)) {
		// Invalidate the entity
		entity = -1;
	}
	return entity;
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	if (entity_manager->ExistsEntity(entity)) {
		Component component = editor_state->editor_components.GetComponentID(component_name);
		if (entity_manager->HasComponent(entity, component)) {
			entity_manager->RemoveComponentCommit(entity, { &component, 1 });
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntitySharedComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	if (entity_manager->ExistsEntity(entity)) {
		Component component = editor_state->editor_components.GetComponentID(component_name);
		if (entity_manager->HasSharedComponent(entity, component)) {
			entity_manager->RemoveSharedComponentCommit(entity, { &component, 1 });
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------
