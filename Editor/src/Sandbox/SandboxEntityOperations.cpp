#include "editorpch.h"
#include "SandboxEntityOperations.h"
#include "Sandbox.h"
#include "../Editor/EditorState.h"
#include "../Modules/Module.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Assets/AssetManagement.h"
#include "ECSEngineForEach.h"

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	Component component = editor_state->editor_components.GetComponentID(component_name);
	if (component.value != -1) {
		if (entity_manager->ExistsEntity(entity)) {
			unsigned short byte_size = entity_manager->ComponentSize(component);
			// Default initialize the component with zeroes
			// Assume a max of ECS_KB for a unique component
			void* stack_allocation = ECS_STACK_ALLOC(ECS_KB);
			editor_state->editor_components.ResetComponent(component_name, stack_allocation);

			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);
			entity_manager->AddComponentCommit(entity, component, stack_allocation);

			// Destroy the old archetype if it no longer has entities
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);
			
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add component {#} to entity {#} for {#}.", component_name, entity.value, ViewportString(viewport));
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntitySharedComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	Component component = editor_state->editor_components.GetComponentID(component_name);
	if (component.value != -1) {
		if (entity_manager->ExistsEntity(entity)) {
			if (instance.value == -1) {
				instance = GetSandboxSharedComponentDefaultInstance(editor_state, sandbox_index, component, viewport);
			}

			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);

			entity_manager->AddSharedComponentCommit(entity, component, instance);
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);

			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add shared component {#} to entity {#} for {#}.", component_name, entity.value, ViewportString(viewport));
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	if (editor_state->editor_components.IsUniqueComponent(component_name)) {
		AddSandboxEntityComponent(editor_state, sandbox_index, entity, component_name, viewport);
	}
	else {
		AddSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, { -1 }, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxSelectedEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->selected_entities.Add(entity);
	sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
	SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
}

// ------------------------------------------------------------------------------------------------------------------------------

void AttachSandboxEntityName(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
		AllocatorPolymorphic allocator = entity_manager->GetComponentAllocator(name_component);

		Name name_data = { StringCopy(allocator, name) };
		entity_manager->AddComponentCommit(entity, name_component, &name_data);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxEntityName(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> new_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
		if (entity_manager->HasComponent(entity, name_component)) {
			Name* name_data = (Name*)entity_manager->GetComponent(entity, name_component);
			AllocatorPolymorphic allocator = entity_manager->GetComponentAllocator(name_component);
			if (name_data->name.size > 0) {
				Deallocate(allocator, name_data->name.buffer);
			}
			name_data->name = StringCopy(allocator, new_name);
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, Stream<Entity> entities)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->selected_entities.CopyOther(entities);
	// This is used by the UI to deduce if the change was done by it or by some other service
	sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
	SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
}

// ------------------------------------------------------------------------------------------------------------------------------

void ClearSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index)
{
	ChangeSandboxSelectedEntities(editor_state, sandbox_index, { nullptr, 0 });
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return CreateSandboxEntity(editor_state, sandbox_index, {}, {}, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CreateSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ComponentSignature unique, 
	SharedComponentSignature shared,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
	ECS_ASSERT(unique.count < ECS_ARCHETYPE_MAX_COMPONENTS);
	memcpy(unique_components, unique.indices, sizeof(Component) * unique.count);
	unique.indices = unique_components;

	Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
	unique[unique.count] = name_component;
	unique.count++;

	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index);
	Entity entity = entity_manager->CreateEntityCommit(unique, shared);
	
	for (size_t index = 0; index < unique.count; index++) {
		void* component_data = entity_manager->GetComponent(entity, unique[index]);
		editor_state->editor_components.ResetComponent(unique[index], component_data, ECS_COMPONENT_UNIQUE);
	}
	
	// For every shared component, check to see if the instance exists
	for (size_t index = 0; index < shared.count; index++) {
		bool exists = entity_manager->ExistsSharedInstance(shared.indices[index], shared.instances[index]);
		if (!exists) {
			// Look for a default component and associate it
			size_t _component_storage[512];
			editor_state->editor_components.ResetComponent(shared.indices[index], _component_storage, ECS_COMPONENT_SHARED);
			entity_manager->RegisterSharedInstanceCommit(shared.indices[index], _component_storage);
		}
	}

	ECS_STACK_CAPACITY_STREAM(char, entity_name, 512);
	EntityToString(entity, entity_name);
	ChangeSandboxEntityName(editor_state, sandbox_index, entity, entity_name, viewport);
	SetSandboxSceneDirty(editor_state, sandbox_index, viewport);

	return entity;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool CreateSandboxGlobalComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	const void* data, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	// Check to see if the component already exists - fail if it does
	if (entity_manager->ExistsGlobalComponent(component)) {
		return false;
	}

	ECS_STACK_CAPACITY_STREAM(size_t, component_storage, ECS_KB);
	Stream<char> component_name = editor_state->editor_components.ComponentFromID(component, ECS_COMPONENT_GLOBAL);
	unsigned short component_size = editor_state->editor_components.GetComponentByteSize(component_name);
	const Reflection::ReflectionType* component_type = editor_state->editor_components.GetType(component_name);
	if (data == nullptr) {
		ECS_ASSERT(component_size < component_storage.capacity * sizeof(component_storage[0]));
		editor_state->editor_components.internal_manager->SetInstanceDefaultData(component_type, component_storage.buffer);
		data = component_storage.buffer;
	}

	size_t allocator_size = GetReflectionComponentAllocatorSize(component_type);
	entity_manager->RegisterGlobalComponentCommit(component, component_size, data, allocator_size, component_name);
	SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	return true;
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CopySandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	Entity destination_entity;
	if (CopySandboxEntities(editor_state, sandbox_index, entity, 1, &destination_entity, viewport)) {
		return destination_entity;
	}
	else {
		return { (unsigned int)-1 };
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

bool CopySandboxEntities(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	unsigned int count, 
	Entity* copied_entities,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->CopyEntityCommit(entity, count, true, copied_entities);

		if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
			// Increment the reference count for all assets that this entity references
			ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, entity_assets, 128);
			GetSandboxEntityAssets(editor_state, sandbox_index, entity, &entity_assets, viewport);

			for (unsigned int index = 0; index < entity_assets.size; index++) {
				IncrementAssetReferenceInSandbox(editor_state, entity_assets[index].handle, entity_assets[index].type, sandbox_index, count);
			}
		}

		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		return true;
	}
	else {
		return false;
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

struct ConvertToOrFromLinkData {
	ConvertToAndFromLinkBaseData base_data;
	Component component;
	ECS_COMPONENT_TYPE component_type;
};

static ConvertToOrFromLinkData GetConvertToOrFromLinkData(
	const EditorState* editor_state,
	Stream<char> link_component,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data;

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(link_component);
	ECS_ASSERT(target.size > 0);

	unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, link_component);
	ModuleLinkComponentTarget link_target = { nullptr, nullptr };
	// If the module index is -1, then it is an engine link component
	if (module_index != -1) {
		link_target = GetModuleLinkComponentTarget(editor_state, module_index, link_component);
	}
	else {
		// Check the engine component side
		link_target = GetEngineLinkComponentTarget(editor_state, link_component);
	}

	const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_component);
	const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target);

	ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(target_type);
	Component target_id = GetReflectionTypeComponent(target_type);

	convert_data.base_data.allocator = allocator;
	convert_data.base_data.asset_database = editor_state->asset_database;
	convert_data.base_data.link_type = link_type;
	convert_data.base_data.module_link = link_target;
	convert_data.base_data.reflection_manager = editor_state->editor_components.internal_manager;
	convert_data.base_data.target_type = target_type;
	convert_data.component = target_id;
	convert_data.component_type = component_type;

	return convert_data;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorTargetToLinkComponent(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Stream<char> link_component,
	Entity entity,
	void* link_data,
	const void* previous_link_data,
	const void* previous_target_data,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	const void* target_data = nullptr;
	if (convert_data.component_type == ECS_COMPONENT_SHARED) {
		target_data = entity_manager->GetSharedData(convert_data.component, entity_manager->GetSharedComponentInstance(convert_data.component, entity));
	}
	else {
		target_data = entity_manager->GetComponent(entity, convert_data.component);
	}

	return ConvertFromTargetToLinkComponent(
		&convert_data.base_data,
		target_data,
		link_data,
		previous_target_data,
		previous_link_data
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorTargetToLinkComponent(
	const EditorState* editor_state,
	Stream<char> link_component,
	const void* target_data,
	void* link_data,
	const void* previous_target_data,
	const void* previous_link_data,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	return ConvertFromTargetToLinkComponent(&convert_data.base_data, target_data, link_data, previous_target_data, previous_link_data);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorLinkComponentToTarget(
	EditorState* editor_state,
	EntityManager* entity_manager,
	Stream<char> link_component,
	Entity entity,
	const void* link_data,
	const void* previous_link_data,
	bool apply_modifier_function,
	const void* previous_target_data,
	AllocatorPolymorphic allocator
) {
	// Convert into a temporary component storage such that we don't get to write only partially to the component
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);

	void* target_data = nullptr;
	unsigned short component_byte_size = 0;

	// Deallocate the buffers for that entity component
	if (convert_data.component_type == ECS_COMPONENT_SHARED) {
		SharedInstance instance = entity_manager->GetSharedComponentInstance(convert_data.component, entity);
		target_data = entity_manager->GetSharedData(convert_data.component, instance);
		component_byte_size = entity_manager->SharedComponentSize(convert_data.component);
	}
	else if (convert_data.component_type == ECS_COMPONENT_UNIQUE) {
		target_data = entity_manager->GetComponent(entity, convert_data.component);
		component_byte_size = entity_manager->ComponentSize(convert_data.component);
	}
	else {
		ECS_ASSERT(false, "Requesting global component is illegal for link to target conversion for an entity");
	}

	previous_target_data = previous_target_data != nullptr ? previous_target_data : target_data;

	ECS_STACK_CAPACITY_STREAM(size_t, component_storage, ECS_KB * 4);
	bool conversion_success = ConvertLinkComponentToTarget(
		&convert_data.base_data,
		link_data,
		component_storage.buffer,
		previous_link_data,
		previous_target_data,
		apply_modifier_function
	);
	if (conversion_success) {
		// Deallocate any existing buffers
		if (convert_data.component_type == ECS_COMPONENT_SHARED) {
			SharedInstance instance = entity_manager->GetSharedComponentInstance(convert_data.component, entity);
			entity_manager->DeallocateSharedInstanceBuffersCommit(convert_data.component, instance);
		}
		else {
			entity_manager->DeallocateEntityBuffersIfExistentCommit(entity, convert_data.component);
		}

		// We can copy directly into target data from the component storage
		memcpy(target_data, component_storage.buffer, component_byte_size);
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorLinkComponentToTarget(
	EditorState* editor_state,
	Stream<char> link_component,
	void* target_data,
	const void* link_data,
	const void* previous_target_data,
	const void* previous_link_data,
	bool apply_modifier_function,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	return ConvertLinkComponentToTarget(&convert_data.base_data, link_data, target_data, previous_link_data, previous_target_data, apply_modifier_function);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertSandboxTargetToLinkComponent(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<char> link_component,
	Entity entity,
	void* link_data,
	const void* previous_link_data,
	const void* previous_target_data,
	AllocatorPolymorphic allocator,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return ConvertEditorTargetToLinkComponent(
		editor_state, 
		GetSandboxEntityManager(editor_state, sandbox_index, viewport), 
		link_component, 
		entity, 
		link_data, 
		previous_link_data, 
		previous_target_data, 
		allocator
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertSandboxLinkComponentToTarget(
	EditorState* editor_state, 
	unsigned int sandbox_index,
	Stream<char> link_component, 
	Entity entity, 
	const void* link_data,
	const void* previous_link_data,
	bool apply_modifier_function,
	const void* previous_target_data,
	AllocatorPolymorphic allocator,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return ConvertEditorLinkComponentToTarget(
		editor_state,
		GetSandboxEntityManager(editor_state, sandbox_index, viewport),
		link_component,
		entity,
		link_data,
		previous_link_data,
		apply_modifier_function,
		previous_target_data,
		allocator
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

void DeleteSandboxUnreferencedSharedInstances(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component shared_component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetSandboxEntityManager(editor_state, sandbox_index, viewport)->UnregisterUnreferencedSharedInstancesCommit(shared_component);
}

// ------------------------------------------------------------------------------------------------------------------------------

void DeleteSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	// Be safe. If for some reason the UI lags behind and the runtime might delete the entity before us
	if (entity_manager->ExistsEntity(entity)) {
		// We also need to remove the references to all the assets that this entity is using
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, entity_assets, 128);
		GetSandboxEntityAssets(editor_state, sandbox_index, entity, &entity_assets, viewport);

		// If we are in the runtime entity manager, we shouldn't remove the sandbox assets
		// Since they are handled by the snapshot
		if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
			UnregisterSandboxAsset(editor_state, sandbox_index, entity_assets);
		}

		entity_manager->DeleteEntityCommit(entity);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);

		// If this entity belongs to the selected group, remove it from there as well
		RemoveSandboxSelectedEntity(editor_state, sandbox_index, entity);
		ChangeInspectorEntitySelection(editor_state, sandbox_index);

		RenderSandboxViewports(editor_state, sandbox_index, { 0, 0 }, true);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

ComponentSignature SandboxEntityUniqueComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		return entity_manager->GetEntitySignatureStable(entity);
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

ComponentSignature SandboxEntitySharedComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		SharedComponentSignature signature = entity_manager->GetEntitySharedSignatureStable(entity);
		return { signature.indices, signature.count };
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedComponentSignature SandboxEntitySharedInstances(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		SharedComponentSignature signature = entity_manager->GetEntitySharedSignatureStable(entity);
		return signature;
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedInstance SandboxEntitySharedInstance(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	SharedComponentSignature signature = SandboxEntitySharedInstances(editor_state, sandbox_index, entity, viewport);
	for (unsigned char index = 0; index < signature.count; index++) {
		if (signature.indices[index] == component) {
			return signature.instances[index];
		}
	}
	return { -1 };
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedInstance FindOrCreateSandboxSharedComponentInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	const void* data,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	SharedInstance instance = entity_manager->GetSharedComponentInstance(component, data);
	if (instance.value == -1) {
		instance = entity_manager->RegisterSharedInstanceCommit(component, data);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
	return instance;
}

// ------------------------------------------------------------------------------------------------------------------------------

void FilterSandboxEntitiesValid(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity>* entities, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	for (size_t index = 0; index < entities->size; index++) {
		if (!IsSandboxEntityValid(editor_state, sandbox_index, entities->buffer[index], viewport)) {
			entities->RemoveSwapBack(index);
			index--;
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity GetSandboxEntity(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<char> name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	Entity entity = StringToEntity(name);
	// Check to see that the entity exists in that sandbox
	if (!entity_manager->ExistsEntity(entity)) {
		// Invalidate the entity
		entity = -1;
	}
	return entity;
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedInstance GetSandboxSharedComponentDefaultInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	size_t component_storage[ECS_KB * 8];
	editor_state->editor_components.ResetComponent(component, component_storage, ECS_COMPONENT_SHARED);
	return FindOrCreateSandboxSharedComponentInstance(editor_state, sandbox_index, component, component_storage, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return (void*)GetSandboxEntityComponent((const EditorState*)editor_state, sandbox_index, entity, component, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxEntityComponent(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity) && entity_manager->ExistsComponent(component)) {
		if (entity_manager->HasComponent(entity, component)) {
			return entity_manager->GetComponent(entity, component);
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxSharedInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return (void*)GetSandboxSharedInstance((const EditorState*)editor_state, sandbox_index, component, instance, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxSharedInstance(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	// Already checks to see if the component exists
	if (entity_manager->ExistsSharedInstance(component, instance)) {
		return entity_manager->GetSharedData(component, instance);
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	bool shared,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	// The cast should be fine here
	return (void*)GetSandboxEntityComponentEx((const EditorState*)editor_state, sandbox_index, entity, component, shared, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxEntityComponentEx(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	bool shared,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		if (shared) {
			if (entity_manager->ExistsSharedComponent(component)) {
				if (entity_manager->HasSharedComponent(entity, component)) {
					return entity_manager->GetSharedData(component, entity_manager->GetSharedComponentInstance(component, entity));
				}
			}
		}
		else {
			if (entity_manager->ExistsComponent(component)) {
				if (entity_manager->HasComponent(entity, component)) {
					return entity_manager->GetComponent(entity, component);
				}
			}
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	return entity_manager->TryGetGlobalComponent(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxGlobalComponent(const EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	return entity_manager->TryGetGlobalComponent(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

float3 GetSandboxEntityTranslation(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, entity, viewport);
	if (translation != nullptr) {
		return translation->value;
	}
	return float3::Splat(0.0f);
}

// ------------------------------------------------------------------------------------------------------------------------------

QuaternionScalar GetSandboxEntityRotation(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, entity, viewport);
	if (rotation != nullptr) {
		return rotation->value;
	}
	return QuaternionIdentityScalar();
}

// ------------------------------------------------------------------------------------------------------------------------------

float3 GetSandboxEntityScale(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const Scale* scale = GetSandboxEntityComponent<Scale>(editor_state, sandbox_index, entity, viewport);
	if (scale != nullptr) {
		return scale->value;
	}
	return float3::Splat(1.0f);
}

// ------------------------------------------------------------------------------------------------------------------------------

TransformScalar GetSandboxEntityTransform(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	float3 translation = GetSandboxEntityTranslation(editor_state, sandbox_index, entity, viewport);
	QuaternionScalar rotation = GetSandboxEntityRotation(editor_state, sandbox_index, entity, viewport);
	float3 scale = GetSandboxEntityScale(editor_state, sandbox_index, entity, viewport);
	return { translation, rotation, scale };
}

// ------------------------------------------------------------------------------------------------------------------------------

MemoryArena* GetSandboxComponentAllocator(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return GetSandboxEntityManager(editor_state, sandbox_index, viewport)->GetComponentAllocator(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

MemoryArena* GetSandboxSharedComponentAllocator(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return GetSandboxEntityManager(editor_state, sandbox_index)->GetSharedComponentAllocator(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

Stream<char> GetSandboxEntityName(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	CapacityStream<char> storage,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const void* name_component = GetSandboxEntityComponent(editor_state, sandbox_index, entity, editor_state->editor_components.GetComponentID(STRING(Name)), viewport);
	if (name_component != nullptr) {
		Name* name = (Name*)name_component;
		return name->name;
	}
	else {
		EntityToString(entity, storage);
		return storage;
	}
}

void GetSandboxEntityComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetEditorEntityComponentAssets(editor_state, GetSandboxEntityManager(editor_state, sandbox_index, viewport), entity, component, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntitySharedComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetEditorEntitySharedComponentAssets(editor_state, GetSandboxEntityManager(editor_state, sandbox_index, viewport), entity, component, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxSharedInstanceComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance shared_instance, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const void* instance_data = GetSandboxSharedInstance(editor_state, sandbox_index, component, shared_instance, viewport);
	GetEditorComponentAssets(editor_state, instance_data, component, ECS_COMPONENT_SHARED, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxGlobalComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const void* component_data = GetSandboxGlobalComponent(editor_state, sandbox_index, component, viewport);
	GetEditorComponentAssets(editor_state, component_data, component, ECS_COMPONENT_GLOBAL, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntityAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	ComponentSignature unique_signature = SandboxEntityUniqueComponents(editor_state, sandbox_index, entity, viewport);
	ComponentSignature shared_signature = SandboxEntitySharedComponents(editor_state, sandbox_index, entity, viewport);

	for (unsigned char index = 0; index < unique_signature.count; index++) {
		GetSandboxEntityComponentAssets(editor_state, sandbox_index, entity, unique_signature[index], handles, viewport);
	}

	for (unsigned char index = 0; index < shared_signature.count; index++) {
		GetSandboxEntitySharedComponentAssets(editor_state, sandbox_index, entity, shared_signature[index], handles, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetEditorComponentAssets(
	const EditorState* editor_state,
	const void* component_data,
	Component component,
	ECS_COMPONENT_TYPE component_type,
	CapacityStream<AssetTypedHandle>* handles
) {
	if (component_data != nullptr) {
		const Reflection::ReflectionType* component_reflection_type = editor_state->editor_components.GetType(component, component_type);

		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 128);
		ECS_STACK_CAPACITY_STREAM(unsigned int, int_handles, 128);

		GetAssetFieldsFromLinkComponentTarget(component_reflection_type, asset_fields);
		GetLinkComponentTargetHandles(component_reflection_type, editor_state->asset_database, component_data, asset_fields, int_handles.buffer);

		for (unsigned int index = 0; index < asset_fields.size; index++) {
			handles->AddAssert({ int_handles[index], asset_fields[index].type.type });
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

static void GetEntityComponentAssetsImplementation(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles,
	ECS_COMPONENT_TYPE component_type
) {
	if (component_type == ECS_COMPONENT_GLOBAL) {
		const void* component_data = entity_manager->TryGetGlobalComponent(component);
		GetEditorComponentAssets(editor_state, component_data, component, component_type, handles);
	}
	else {
		if (entity_manager->ExistsEntity(entity)) {
			const void* component_data = nullptr;
			if (component_type == ECS_COMPONENT_SHARED) {
				if (entity_manager->ExistsSharedComponent(component)) {
					if (entity_manager->HasSharedComponent(entity, component)) {
						component_data = entity_manager->GetSharedData(component, entity_manager->GetSharedComponentInstance(component, entity));
					}
				}
			}
			else {
				if (entity_manager->ExistsComponent(component)) {
					if (entity_manager->HasComponent(entity, component)) {
						component_data = entity_manager->GetComponent(entity, component);
					}
				}
			}
			GetEditorComponentAssets(editor_state, component_data, component, component_type, handles);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEditorEntityComponentAssets(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles
)
{
	GetEntityComponentAssetsImplementation(editor_state, entity_manager, entity, component, handles, ECS_COMPONENT_UNIQUE);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEditorEntitySharedComponentAssets(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles
)
{
	GetEntityComponentAssetsImplementation(editor_state, entity_manager, entity, component, handles, ECS_COMPONENT_SHARED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEditorEntityAssets(const EditorState* editor_state, const EntityManager* entity_manager, Entity entity, CapacityStream<AssetTypedHandle>* handles)
{
	ComponentSignature unique_signature = entity_manager->GetEntitySignatureStable(entity);
	SharedComponentSignature shared_signature = entity_manager->GetEntitySharedSignatureStable(entity);

	for (unsigned char index = 0; index < unique_signature.count; index++) {
		GetEditorEntityComponentAssets(editor_state, entity_manager, entity, unique_signature[index], handles);
	}

	for (unsigned char index = 0; index < shared_signature.count; index++) {
		GetEditorEntitySharedComponentAssets(editor_state, entity_manager, entity, shared_signature.indices[index], handles);
	}
}


// ------------------------------------------------------------------------------------------------------------------------------

template<bool has_translation, bool has_rotation>
static void GetSandboxEntitiesMidpointImpl(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	Stream<Entity> entities,
	float3* translation_midpoint,
	QuaternionScalar* rotation_midpoint,
	Stream<TransformGizmo> transform_gizmos,
	bool add_transform_gizmos_to_total_count,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	float3 translation_average = float3::Splat(0.0f);
	QuaternionScalar rotation_average = QuaternionAverageCumulatorInitializeScalar();

	for (size_t index = 0; index < entities.size; index++) {
		if constexpr (has_translation) {
			const Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, entities[index], viewport);
			if (translation != nullptr) {
				translation_average += translation->value;
			}
		}
		if constexpr (has_rotation) {
			const Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, entities[index], viewport);
			if (rotation != nullptr) {
				QuaternionAddToAverageStep(&rotation_average, rotation->value);
			}
		}
	}

	for (size_t index = 0; index < transform_gizmos.size; index++) {
		if constexpr (has_translation) {
			translation_average += transform_gizmos[index].position;
		}
		if constexpr (has_rotation) {
			QuaternionAddToAverageStep(&rotation_average, transform_gizmos[index].rotation);
		}
	}

	size_t total_count = entities.size;
	if (add_transform_gizmos_to_total_count) {
		total_count += transform_gizmos.size;
	}

	if constexpr (has_translation) {
		*translation_midpoint = translation_average * float3::Splat(1.0f / (float)total_count);
	}
	if constexpr (has_rotation) {
		*rotation_midpoint = QuaternionAverageFromCumulator(rotation_average, total_count);
	}
}

float3 GetSandboxEntitiesTranslationMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	float3 midpoint;
	GetSandboxEntitiesMidpointImpl<true, false>(editor_state, sandbox_index, entities, &midpoint, nullptr, {}, false, viewport);
	return midpoint;
}

// ------------------------------------------------------------------------------------------------------------------------------

QuaternionScalar GetSandboxEntitiesRotationMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	QuaternionScalar midpoint;
	GetSandboxEntitiesMidpointImpl<false, true>(editor_state, sandbox_index, entities, nullptr, &midpoint, {}, false, viewport);
	return midpoint;
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntitiesMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	float3* translation_midpoint, 
	QuaternionScalar* rotation_midpoint,
	Stream<TransformGizmo> transform_gizmos,
	bool add_transform_gizmos_to_total_count,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetSandboxEntitiesMidpointImpl<true, true>(
		editor_state, 
		sandbox_index, 
		entities, 
		translation_midpoint, 
		rotation_midpoint, 
		transform_gizmos, 
		add_transform_gizmos_to_total_count, 
		viewport
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxActivePrefabIDs(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	AdditionStream<unsigned int> prefab_ids, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	entity_manager->ForEachEntityComponent(PrefabComponent::ID(), [&](Entity entity, const void* component) {
		const PrefabComponent* prefab = (const PrefabComponent*)component;
		Stream<unsigned int> existing_ids = prefab_ids.ToStream();
		size_t existing_index = SearchBytes(existing_ids, prefab->id);
		if (existing_index == -1) {
			prefab_ids.Add(prefab->id);
		}
	});
}

// ------------------------------------------------------------------------------------------------------------------------------

bool IsSandboxEntitySelected(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	return FindSandboxSelectedEntityIndex(editor_state, sandbox_index, entity) != -1;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool IsSandboxEntityValid(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EntityManager* active_entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	return active_entity_manager->ExistsEntity(entity);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool NeedsApplyModifierLinkComponent(const EditorState* editor_state, Stream<char> link_name)
{
	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, link_name);
	return link_target.build_function != nullptr && link_target.apply_modifier != nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool NeedsApplyModifierButtonLinkComponent(const EditorState* editor_state, Stream<char> link_name)
{
	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, link_name);
	return link_target.build_function != nullptr && link_target.apply_modifier != nullptr && link_target.apply_modifier_needs_button;
}

// ------------------------------------------------------------------------------------------------------------------------------

void ParentSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity child, 
	Entity parent,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
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
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ParentSandboxEntities(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> children, 
	Entity parent,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	for (size_t index = 0; index < children.size; index++) {
		ParentSandboxEntity(editor_state, sandbox_index, children[index], parent, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		Component component = editor_state->editor_components.GetComponentID(component_name);
		if (entity_manager->HasComponent(entity, component)) {
			const void* data = GetSandboxEntityComponent(editor_state, sandbox_index, entity, component, viewport);
			// If we are in runtime, we don't have to unload the assets - they are automatically handled
			if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
				RemoveSandboxComponentAssets(editor_state, sandbox_index, component, data, ECS_COMPONENT_UNIQUE);
			}

			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);
			entity_manager->RemoveComponentCommit(entity, { &component, 1 });
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntitySharedComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		Component component = editor_state->editor_components.GetComponentID(component_name);
		if (entity_manager->HasSharedComponent(entity, component)) {
			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);
			SharedInstance previous_instance = SandboxEntitySharedInstance(editor_state, sandbox_index, entity, component, viewport);
			entity_manager->RemoveSharedComponentCommit(entity, { &component, 1 });
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);

			bool is_unreferenced = entity_manager->IsUnreferencedSharedInstance(component, previous_instance);
			// We need to remove the sandbox references only in scene mode
			if (is_unreferenced && entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
				const void* instance_data = entity_manager->GetSharedData(component, previous_instance);
				RemoveSandboxComponentAssets(editor_state, sandbox_index, component, instance_data, ECS_COMPONENT_SHARED);
				entity_manager->UnregisterSharedInstanceCommit(component, previous_instance);
			}

			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	if (editor_state->editor_components.IsUniqueComponent(component_name)) {
		RemoveSandboxEntityComponent(editor_state, sandbox_index, entity, component_name, viewport);
	}
	else {
		RemoveSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	ECS_ASSERT(entity_manager->ExistsGlobalComponent(component));

	// We only need to remove the sandbox references in scene mode
	if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
		const void* component_data = entity_manager->GetGlobalComponent(component);
		RemoveSandboxComponentAssets(editor_state, sandbox_index, component, component_data, ECS_COMPONENT_GLOBAL);
	}

	entity_manager->UnregisterGlobalComponentCommit(component);
	SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityFromHierarchy(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->RemoveEntityFromHierarchyCommit({ &entity, 1 });
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxComponentAssets(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	const void* data, 
	ECS_COMPONENT_TYPE component_type,
	Stream<LinkComponentAssetField> asset_fields
) {
	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, unregister_elements, 512);

	if (asset_fields.size == 0) {
		GetEditorComponentAssets(editor_state, data, component, component_type, &unregister_elements);
	}
	else {
		const Reflection::ReflectionType* component_reflection_type = editor_state->editor_components.GetType(component, component_type);
		ECS_STACK_CAPACITY_STREAM(unsigned int, handles, 512);
		GetLinkComponentTargetHandles(component_reflection_type, editor_state->asset_database, data, asset_fields, handles.buffer);

		for (unsigned int index = 0; index < asset_fields.size; index++) {
			unregister_elements[index].handle = handles[index];
			unregister_elements[index].type = asset_fields[index].type.type;
		}
		unregister_elements.size = asset_fields.size;
	}

	// Unregister these assets
	UnregisterSandboxAsset(editor_state, sandbox_index, unregister_elements);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool RemoveSandboxSelectedEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	unsigned int selected_index = FindSandboxSelectedEntityIndex(editor_state, sandbox_index, entity);
	if (selected_index != -1) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		sandbox->selected_entities.RemoveSwapBack(selected_index);
		sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
		SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------------------------------------

void ResetSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(component_name);
	bool is_shared = editor_state->editor_components.IsSharedComponent(target.size > 0 ? target : component_name);
	Stream<char> initial_component = component_name;
	if (target.size > 0) {
		component_name = target;
	}

	if (!is_shared) {
		// We also need to remove the sandbox asset references
		if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
			Component component = editor_state->editor_components.GetComponentID(component_name);
			const void* component_data = GetSandboxEntityComponent(editor_state, sandbox_index, entity, component, viewport);
			RemoveSandboxComponentAssets(editor_state, sandbox_index, component, component_data, ECS_COMPONENT_UNIQUE);
		}

		// It has built in checks for not crashing
		editor_state->editor_components.ResetComponentFromManager(entity_manager, component_name, entity);
	}
	else {
		// Remove it and then add it again with the default component
		// This takes care of the asset handles being removed
		RemoveSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, viewport);
		AddSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, { -1 }, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ResetSandboxGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	void* component_data = entity_manager->GetGlobalComponent(component);
	if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
		RemoveSandboxComponentAssets(editor_state, sandbox_index, component, component_data, ECS_COMPONENT_GLOBAL);
	}
	editor_state->editor_components.ResetComponent(component, component_data, ECS_COMPONENT_GLOBAL);
}

// -----------------------------------------------------------------------------------------------------------------------------

void RotateSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, QuaternionScalar rotation_delta)
{
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM(TransformGizmoPointers, transform_gizmo_pointers, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(Entity, transform_gizmo_entities, ECS_KB);
	GetSandboxSelectedVirtualEntitiesTransformPointers(editor_state, sandbox_index, &transform_gizmo_pointers, &transform_gizmo_entities);

	auto apply_delta = [rotation_delta](QuaternionScalar* rotation_storage) {
		// We need to use local rotation regardless of the transform space
		// The transform tool takes care of the correct rotation delta
		QuaternionScalar new_quaternion = AddLocalRotation(*rotation_storage, rotation_delta);
		*rotation_storage = new_quaternion;
	};

	for (size_t index = 0; index < selected_entities.size; index++) {
		Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, selected_entities[index]);
		if (rotation != nullptr) {
			apply_delta((QuaternionScalar*)&rotation->value);
		}
		else {
			// Check the virtual entities
			size_t gizmo_index = SearchBytes(transform_gizmo_entities.ToStream(), selected_entities[index]);
			if (gizmo_index != -1) {
				if (transform_gizmo_pointers[gizmo_index].euler_rotation != nullptr) {
					if (transform_gizmo_pointers[gizmo_index].is_euler_rotation) {
						QuaternionScalar rotation_storage = QuaternionFromEuler(*transform_gizmo_pointers[gizmo_index].euler_rotation);
						apply_delta(&rotation_storage);
						*transform_gizmo_pointers[gizmo_index].euler_rotation = QuaternionToEuler(rotation_storage);
					}
					else {
						apply_delta(transform_gizmo_pointers[gizmo_index].quaternion_rotation);
					}
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void SandboxForEachEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ForEachEntityUntypedFunctor functor, 
	void* functor_data, 
	const ArchetypeQueryDescriptor& query_descriptor,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	World temp_world;
	memcpy(&temp_world, &GetSandbox(editor_state, sandbox_index)->sandbox_world, sizeof(temp_world));

	temp_world.entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	ForEachEntityCommitFunctor(&temp_world, functor, functor_data, query_descriptor);
}

// ------------------------------------------------------------------------------------------------------------------------------

struct SplatLinkComponentBasicData {
	const void* link_component;
	const Reflection::ReflectionType* target_type;
	Stream<LinkComponentAssetField> asset_fields;
	Stream<Stream<void>> assets;
};

void SplatLinkComponentBasic(ForEachEntityUntypedFunctorData* functor_data) {
	SplatLinkComponentBasicData* data = (SplatLinkComponentBasicData*)functor_data->data;
	for (size_t index = 0; index < data->asset_fields.size; index++) {
		SetAssetTargetFieldFromReflection(
			data->target_type, 
			data->asset_fields[index].field_index, 
			functor_data->unique_components[0], 
			data->assets[index], 
			data->asset_fields[index].type.type
		);
	}
}

struct SplatLinkComponentBuildData {
	const void* link_component;
	Stream<Stream<void>> assets;
	ModuleLinkComponentFunction link_function;
};

void SplatLinkComponentBuild(ForEachEntityUntypedFunctorData* functor_data) {
	SplatLinkComponentBuildData* data = (SplatLinkComponentBuildData*)functor_data->data;

	ModuleLinkComponentFunctionData function_data;
	function_data.assets = data->assets;
	function_data.component = functor_data->unique_components[0];
	function_data.link_component = data->link_component;
	data->link_function(&function_data);
}

// ------------------------------------------------------------------------------------------------------------------------------

//bool SandboxSplatLinkComponentAssetFields(
//	EditorState* editor_state, 
//	unsigned int sandbox_index, 
//	const void* link_component, 
//	Stream<char> component_name, 
//	bool give_error_when_failing,
//	EDITOR_SANDBOX_VIEWPORT viewport
//)
//{
//	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
//	if (type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component doesn't exist.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
//
//	ECS_STACK_CAPACITY_STREAM(Stream<void>, assets, 512);
//	ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
//	GetAssetFieldsFromLinkComponent(type, asset_fields);
//	if (asset_fields.size == 0) {
//		return true;
//	}
//
//	GetLinkComponentAssetData(type, link_component, editor_state->asset_database, asset_fields, &assets);
//
//	if (GetReflectionTypeLinkComponentNeedsDLL(type)) {
//		unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, component_name);
//		if (module_index == -1) {
//			if (give_error_when_failing) {
//				ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component was not found in the loaded modules.", 
//					component_name);
//				EditorSetConsoleError(error_message);
//			}
//			return false;
//		}
//		ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, module_index, component_name);
//		if (link_target.build_function == nullptr) {
//			if (give_error_when_failing) {
//				ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component requires a DLL function but none was found.", 
//					component_name);
//				EditorSetConsoleError(error_message);
//			}
//			return false;
//		}
//
//		SplatLinkComponentBuildData splat_data;
//		splat_data.link_component = link_component;
//		splat_data.assets = assets;
//		splat_data.link_function = link_target.build_function;
//		SandboxForEachEntity(editor_state, sandbox_index, SplatLinkComponentBuild, &splat_data, { &component, 1 }, { nullptr, 0 }, {}, {}, viewport);
//		return true;
//	}
//
//	SplatLinkComponentBasicData splat_data;
//	splat_data.link_component = link_component;
//	splat_data.asset_fields = asset_fields;
//	splat_data.assets = assets;
//	
//	splat_data.target_type = editor_state->editor_components.GetType(editor_state->editor_components.GetComponentFromLink(component_name));
//	if (splat_data.target_type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The target component was not found.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	if (!ValidateLinkComponent(type, splat_data.target_type)) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The link component {#} cannot be resolved with "
//				"default conversion to {#}.", component_name, splat_data.target_type->name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//	SandboxForEachEntity(editor_state, sandbox_index, SplatLinkComponentBasic, &splat_data, { &component, 1 }, { nullptr, 0 }, {}, {}, viewport);
//	return true;
//}

// ------------------------------------------------------------------------------------------------------------------------------

// The functor receives the component as parameter
//template<typename GetData>
//bool UpdateLinkComponentBaseAssetsOnly(
//	EditorState* editor_state,
//	unsigned int sandbox_index,
//	const void* link_component,
//	Stream<char> component_name,
//	const void* previous_link_component,
//	const void* previous_target_component,
//	bool give_error_when_failing,
//	GetData&& get_data
//) {
//	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
//	if (type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The component doesn't exist.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	Stream<char> target = editor_state->editor_components.GetComponentFromLink(component_name);
//	if (target.size == 0) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The component does not have a target.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//	const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target);
//	if (target_type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The target {#} does not exist.", component_name, target);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, component_name);
//	if (module_index == -1) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The module from which it came could not be identified.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, module_index, component_name);
//	Component component = { (short)target_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
//
//	void* converted_data = get_data(component);
//	if (converted_data == nullptr) {
//		return false;
//	}
//
//	ConvertToAndFromLinkBaseData convert_data;
//	convert_data.reflection_manager = editor_state->GlobalReflectionManager();
//	convert_data.link_type = type;
//	convert_data.target_type = target_type;
//	convert_data.asset_database = editor_state->asset_database;
//	convert_data.module_link = link_target;
//	bool success = ConvertLinkComponentToTargetAssetsOnly(&convert_data, link_component, converted_data, previous_link_data, previous_target_component);
//	if (!success) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The dll function is missing or the default link is incompatible.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//	}
//	return success;
//}
//
//bool SandboxUpdateLinkComponentSharedInstance(
//	EditorState* editor_state,
//	unsigned int sandbox_index,
//	const void* link_component, 
//	Stream<char> component_name,
//	SharedInstance instance,
//	bool give_error_when_failing,
//	EDITOR_SANDBOX_VIEWPORT viewport
//)
//{
//	return UpdateLinkComponentBaseAssetsOnly(editor_state, sandbox_index, link_component, component_name, give_error_when_failing, [=](Component component) {
//		void* shared_data = GetSandboxSharedInstance(editor_state, sandbox_index, component, instance, viewport);
//		if (shared_data == nullptr) {
//			if (give_error_when_failing) {
//				ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The given shared instance {#} is invalid for {#}.", 
//					component_name, instance.value, ViewportString(viewport));
//				EditorSetConsoleError(error_message);
//			}
//		}
//		return shared_data;
//	});
//}

// ------------------------------------------------------------------------------------------------------------------------------

bool SandboxUpdateUniqueLinkComponentForEntity(
	EditorState* editor_state,
	unsigned int sandbox_index,
	const void* link_component,
	Stream<char> link_name,
	Entity entity,
	const void* previous_link_component,
	SandboxUpdateLinkComponentForEntityInfo info
) {
	Component component = editor_state->editor_components.GetComponentIDWithLink(link_name);
	AllocatorPolymorphic component_allocator = GetSandboxComponentAllocator(editor_state, sandbox_index, component, info.viewport);
	bool success = ConvertSandboxLinkComponentToTarget(
		editor_state, 
		sandbox_index, 
		link_name, 
		entity, 
		link_component, 
		previous_link_component, 
		info.apply_modifier_function,
		info.target_previous_data,
		component_allocator, 
		info.viewport
	);
	if (!success) {
		if (info.give_error_when_failing) {
			ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 512);
			Stream<char> entity_name = GetSandboxEntityName(editor_state, sandbox_index, entity, entity_name_storage);

			ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert link component {#} to target for entity {#} in sandbox {#} for {#}.",
				link_name, entity_name, sandbox_index, ViewportString(info.viewport));
			EditorSetConsoleError(error_message);
		}
	}
	return success;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool SandboxUpdateSharedLinkComponentForEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	const void* link_component, 
	Stream<char> link_name, 
	Entity entity,
	const void* previous_link_component,
	SandboxUpdateLinkComponentForEntityInfo info
)
{
	// Get the component data first to help the conversion
	Component target_component = editor_state->editor_components.GetComponentIDWithLink(link_name);
	SharedInstance current_instance = SandboxEntitySharedInstance(editor_state, sandbox_index, entity, target_component, info.viewport);

	const void* previous_shared_data = nullptr;
	if (info.target_previous_data != nullptr) {
		previous_shared_data = info.target_previous_data;
	}
	else {
		previous_shared_data = GetSandboxSharedInstance(editor_state, sandbox_index, target_component, current_instance, info.viewport);
	}

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
	size_t converted_link_storage[ECS_KB * 4];
	bool success = ConvertEditorLinkComponentToTarget(
		editor_state, 
		link_name, 
		converted_link_storage, 
		link_component, 
		previous_shared_data,
		previous_link_component,
		info.apply_modifier_function,
		&stack_allocator
	);
	if (!success) {
		if (info.give_error_when_failing) {
			ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 256);
			Stream<char> entity_name = GetSandboxEntityName(editor_state, sandbox_index, entity, entity_name_storage);
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert link component {#} to target for entity {#} in sandbox {#} for {#}.", 
				link_name, entity_name, sandbox_index, ViewportString(info.viewport));
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	// Find or create the shared instance that matches this data
	SharedInstance new_shared_instance = FindOrCreateSandboxSharedComponentInstance(editor_state, sandbox_index, target_component, converted_link_storage, info.viewport);

	if (new_shared_instance != current_instance) {
		EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, info.viewport);
		SharedInstance old_instance = entity_manager->ChangeEntitySharedInstanceCommit(entity, target_component, new_shared_instance, true);

		// Unregister the shared instance if it longer is referenced
		entity_manager->UnregisterUnreferencedSharedInstanceCommit(target_component, old_instance);
	}

	// It can happen that the handle is not modified and so it will, in fact, have the same shared instance
	// Only change the shared instance in that case
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SandboxApplyEntityChanges(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	Stream<Entity> entities_to_be_updated, 
	ComponentSignature unique_signature, 
	const void** unique_components, 
	ComponentSignature shared_signature, 
	const void** shared_components, 
	Stream<EntityChange> changes
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	ApplyEntityChanges(
		editor_state->GlobalReflectionManager(),
		entity_manager,
		entities_to_be_updated,
		unique_components,
		unique_signature,
		shared_components,
		shared_signature,
		changes
	);
}

// -----------------------------------------------------------------------------------------------------------------------------

void ScaleSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, float3 scale_delta)
{
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM(TransformGizmoPointers, transform_gizmo_pointers, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(Entity, transform_gizmo_entities, ECS_KB);
	GetSandboxSelectedVirtualEntitiesTransformPointers(editor_state, sandbox_index, &transform_gizmo_pointers, &transform_gizmo_entities);

	for (size_t index = 0; index < selected_entities.size; index++) {
		Scale* scale = GetSandboxEntityComponent<Scale>(editor_state, sandbox_index, selected_entities[index]);
		if (scale != nullptr) {
			// It is a real entity
			scale->value += scale_delta;
		}
		else {
			// Check the virtual entity gizmos
			size_t gizmo_index = SearchBytes(transform_gizmo_entities.ToStream(), selected_entities[index]);
			if (gizmo_index != -1) {
				if (transform_gizmo_pointers[gizmo_index].scale != nullptr) {
					*transform_gizmo_pointers[gizmo_index].scale += scale_delta;
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TranslateSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, float3 delta)
{
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM(TransformGizmoPointers, transform_gizmo_pointers, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(Entity, transform_gizmo_entities, ECS_KB);
	GetSandboxSelectedVirtualEntitiesTransformPointers(editor_state, sandbox_index, &transform_gizmo_pointers, &transform_gizmo_entities);

	for (size_t index = 0; index < selected_entities.size; index++) {
		Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, selected_entities[index]);
		if (translation != nullptr) {
			// It is a real entity
			translation->value += delta;
		}
		else {
			// Check the virtual entity gizmos
			size_t gizmo_index = SearchBytes(transform_gizmo_entities.ToStream(), selected_entities[index]);
			if (gizmo_index != -1) {
				if (transform_gizmo_pointers[gizmo_index].position != nullptr) {
					*transform_gizmo_pointers[gizmo_index].position += delta;
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------
