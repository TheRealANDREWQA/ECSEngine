#include "editorpch.h"
#include "EditorSandboxEntityOperations.h"
#include "EditorSandbox.h"
#include "EditorState.h"

#include "ECSEngineComponents.h"
#include "ECSEngineForEach.h"

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
			unsigned short byte_size = entity_manager->ComponentSize(component);
			// Default initialize the component with zeroes
			size_t storage[512];
			memset(storage, 0, byte_size);
			entity_manager->AddComponentCommit(entity, component, storage);
			SetSandboxSceneDirty(editor_state, sandbox_index);
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
			SetSandboxSceneDirty(editor_state, sandbox_index);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add shared component {#} to entity {#}.", component_name, entity.value);
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntityComponentEx(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	if (editor_state->editor_components.IsComponent(component_name)) {
		AddSandboxEntityComponent(editor_state, sandbox_index, entity, component_name);
	}
	else {
		AddSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AttachEntityName(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	if (entity_manager->ExistsEntity(entity)) {
		Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
		AllocatorPolymorphic allocator = entity_manager->GetComponentAllocatorPolymorphic(name_component);

		Name name_data = { function::StringCopy(allocator, name) };
		entity_manager->AddComponentCommit(entity, name_component, &name_data);
		SetSandboxSceneDirty(editor_state, sandbox_index);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ChangeEntityName(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> new_name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	if (entity_manager->ExistsEntity(entity)) {
		Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
		if (entity_manager->HasComponent(entity, name_component)) {
			Name* name_data = (Name*)entity_manager->GetComponent(entity, name_component);
			AllocatorPolymorphic allocator = entity_manager->GetComponentAllocatorPolymorphic(name_component);
			if (name_data->name.size > 0) {
				Deallocate(allocator, name_data->name.buffer);
			}
			name_data->name = function::StringCopy(allocator, new_name);
			SetSandboxSceneDirty(editor_state, sandbox_index);
		}
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
	Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
	ECS_ASSERT(unique.count < ECS_ARCHETYPE_MAX_COMPONENTS);
	memcpy(unique_components, unique.indices, sizeof(Component) * unique.count);
	unique.indices = unique_components;

	Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
	unique[unique.count] = name_component;
	unique.count++;

	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	Entity entity = entity_manager->CreateEntityCommit(unique, shared);

	ECS_STACK_CAPACITY_STREAM(char, entity_name, 512);
	EntityToString(entity, entity_name);
	ChangeEntityName(editor_state, sandbox_index, entity, entity_name);
	SetSandboxSceneDirty(editor_state, sandbox_index);

	return entity;
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CopySandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	if (entity_manager->ExistsEntity(entity)) {
		Entity destination_entity;
		entity_manager->CopyEntityCommit(entity, 1, &destination_entity);
		SetSandboxSceneDirty(editor_state, sandbox_index);
		return destination_entity;
	}
	else {
		return { (unsigned int)-1 };
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

bool CopySandboxEntities(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int count, Entity* copied_entities)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->CopyEntityCommit(entity, count, copied_entities);
		SetSandboxSceneDirty(editor_state, sandbox_index);
		return true;
	}
	else {
		return false;
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void DeleteSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	// Be safe. If for some reason the UI lags behind and the runtime might delete the entity before us
	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->DeleteEntityCommit(entity);
		SetSandboxSceneDirty(editor_state, sandbox_index);
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

void ParentSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity child, Entity parent)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	bool child_exists = entity_manager->ExistsEntity(child);
	if (child_exists) {
		if (parent.value != -1) {
			bool exists_parent = entity_manager->ExistsEntity(parent);
			if (!exists_parent) {
				return;
			}
		}

		EntityPair pair;
		pair.parent = parent;
		pair.child = child;
		entity_manager->ChangeOrSetEntityParentCommit({ &pair, 1 });
		SetSandboxSceneDirty(editor_state, sandbox_index);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ParentSandboxEntities(EditorState* editor_state, unsigned int sandbox_index, Stream<Entity> children, Entity parent)
{
	for (size_t index = 0; index < children.size; index++) {
		ParentSandboxEntity(editor_state, sandbox_index, children[index], parent);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	if (entity_manager->ExistsEntity(entity)) {
		Component component = editor_state->editor_components.GetComponentID(component_name);
		if (entity_manager->HasComponent(entity, component)) {
			entity_manager->RemoveComponentCommit(entity, { &component, 1 });
			SetSandboxSceneDirty(editor_state, sandbox_index);
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
			SetSandboxSceneDirty(editor_state, sandbox_index);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityFromHierarchy(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->RemoveEntityFromHierarchyCommit({ &entity, 1 });
	}
}

// ------------------------------------------------------------------------------------------------------------------------------
