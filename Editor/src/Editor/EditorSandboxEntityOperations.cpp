#include "editorpch.h"
#include "EditorSandboxEntityOperations.h"
#include "EditorSandbox.h"
#include "EditorState.h"
#include "../Modules/Module.h"

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
			// Assume a max of ECS_KB for a unique component
			void* stack_allocation = ECS_STACK_ALLOC(ECS_KB);
			editor_state->editor_components.ResetComponent(component_name, stack_allocation);
			entity_manager->AddComponentCommit(entity, component, stack_allocation);
			SetSandboxSceneDirty(editor_state, sandbox_index);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add component {#} to entity {#}.", component_name, entity.value);
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntitySharedComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name, SharedInstance instance)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	Component component = editor_state->editor_components.GetComponentID(component_name);
	if (component.value != -1) {
		if (entity_manager->ExistsEntity(entity)) {
			if (instance.value == -1) {
				instance = GetSharedComponentDefaultInstance(editor_state, sandbox_index, component);
			}

			entity_manager->AddSharedComponentCommit(entity, component, instance);
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
	if (editor_state->editor_components.IsUniqueComponent(component_name)) {
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
	
	for (size_t index = 0; index < unique.count; index++) {
		void* component_data = entity_manager->GetComponent(entity, unique[index]);
		editor_state->editor_components.ResetComponent(unique[index], component_data, false);
	}
	
	// For every shared component, check to see if the instance exists
	for (size_t index = 0; index < shared.count; index++) {
		bool exists = entity_manager->ExistsSharedInstance(shared.indices[index], shared.instances[index]);
		if (!exists) {
			// Look for a default component and associate it
			size_t _component_storage[512];
			editor_state->editor_components.ResetComponent(shared.indices[index], _component_storage, true);
			entity_manager->RegisterSharedInstanceCommit(shared.indices[index], _component_storage);
		}
	}

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
		entity_manager->CopyEntityCommit(entity, 1, true, &destination_entity);
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

struct ConvertToOrFromLinkData {
	ModuleLinkComponentTarget module_link;
	const Reflection::ReflectionManager* reflection_manager;
	const Reflection::ReflectionType* target_type;
	const Reflection::ReflectionType* link_type;
	const AssetDatabase* asset_database;
	void* target_data;
	AllocatorPolymorphic allocator;
};

ConvertToOrFromLinkData GetConvertToOrFromLinkData(
	EditorState* editor_state, 
	unsigned int sandbox_index,
	Entity entity,
	Stream<char> link_component, 
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data;

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(link_component);
	ECS_ASSERT(target.size > 0);

	unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, link_component);
	ECS_ASSERT(module_index != -1);
	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, module_index, link_component);

	const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_component);
	const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target);

	bool is_shared = Reflection::IsReflectionTypeSharedComponent(target_type);
	void* target_data = nullptr;

	Component target_id = { (short)target_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
	EntityManager* active_entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	if (is_shared) {
		target_data = active_entity_manager->GetComponent(entity, target_id);
	}
	else {
		target_data = active_entity_manager->GetSharedData(target_id, active_entity_manager->GetSharedComponentInstance(target_id, entity));
	}

	convert_data.allocator = allocator;
	convert_data.asset_database = editor_state->asset_database;
	convert_data.link_type = link_type;
	convert_data.module_link = link_target;
	convert_data.reflection_manager = editor_state->editor_components.internal_manager;
	convert_data.target_data = target_data;
	convert_data.target_type = target_type;

	return convert_data;
}

bool ConvertTargetToLinkComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	Stream<char> link_component,
	void* link_data, 
	AllocatorPolymorphic allocator
)
{
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, sandbox_index, entity, link_component, allocator);
	return ModuleFromTargetToLinkComponent(
		convert_data.module_link,
		convert_data.reflection_manager,
		convert_data.target_type,
		convert_data.link_type,
		convert_data.asset_database,
		convert_data.target_data,
		link_data,
		convert_data.allocator
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertLinkComponentToTarget(
	EditorState* editor_state, 
	unsigned int sandbox_index,
	Entity entity, 
	Stream<char> link_component, 
	const void* link_data, 
	AllocatorPolymorphic allocator
)
{
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, sandbox_index, entity, link_component, allocator);
	return ModuleLinkComponentToTarget(
		convert_data.module_link,
		convert_data.reflection_manager,
		convert_data.link_type,
		convert_data.target_type,
		convert_data.asset_database,
		link_data,
		convert_data.target_data,
		convert_data.allocator
	);
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

SharedInstance GetSharedComponentDefaultInstance(EditorState* editor_state, unsigned int sandbox_index, Component component)
{
	size_t component_storage[ECS_KB * 8];
	editor_state->editor_components.ResetComponent(component, component_storage, true);
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	SharedInstance instance = entity_manager->GetSharedComponentInstance(component, component_storage);
	if (instance.value == -1) {
		instance = entity_manager->RegisterSharedInstanceCommit(component, component_storage);
	}
	return instance;
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

void RemoveSandboxEntityComponentEx(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name) {
	if (editor_state->editor_components.IsUniqueComponent(component_name)) {
		RemoveSandboxEntityComponent(editor_state, sandbox_index, entity, component_name);
	}
	else {
		RemoveSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name);
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

void ResetSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(component_name);
	bool is_shared = editor_state->editor_components.IsSharedComponent(target.size > 0 ? target : component_name);
	Stream<char> initial_component = component_name;
	if (target.size > 0) {
		component_name = target;
	}

	if (!is_shared) {
		// It has built in checks for not crashing
		editor_state->editor_components.ResetComponentFromManager(entity_manager, component_name, entity);
	}
	else {
		// Remove it and then add it again with the default component
		RemoveSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name);
		AddSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------
