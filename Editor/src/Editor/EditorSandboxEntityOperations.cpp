#include "editorpch.h"
#include "EditorSandboxEntityOperations.h"
#include "EditorSandbox.h"
#include "EditorState.h"
#include "../Modules/Module.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Assets/AssetManagement.h"

#include "ECSEngineComponentsAll.h"
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
				instance = GetSharedComponentDefaultInstance(editor_state, sandbox_index, component, viewport);
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
	SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
}

// ------------------------------------------------------------------------------------------------------------------------------

void AttachEntityName(
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
		AllocatorPolymorphic allocator = entity_manager->GetComponentAllocatorPolymorphic(name_component);

		Name name_data = { function::StringCopy(allocator, name) };
		entity_manager->AddComponentCommit(entity, name_component, &name_data);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ChangeEntityName(
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
			AllocatorPolymorphic allocator = entity_manager->GetComponentAllocatorPolymorphic(name_component);
			if (name_data->name.size > 0) {
				Deallocate(allocator, name_data->name.buffer);
			}
			name_data->name = function::StringCopy(allocator, new_name);
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, Stream<Entity> entities)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->selected_entities.Copy(entities);
	SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
}

// ------------------------------------------------------------------------------------------------------------------------------

void ClearSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index)
{
	ChangeSandboxSelectedEntities(editor_state, sandbox_index, { nullptr,0 });
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
	ChangeEntityName(editor_state, sandbox_index, entity, entity_name, viewport);
	SetSandboxSceneDirty(editor_state, sandbox_index, viewport);

	return entity;
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
		entity_manager->CopyEntityCommit(entity, count, copied_entities);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);

		// Increment the reference count for all assets that this entity references
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, entity_assets, 128);
		GetSandboxEntityAssets(editor_state, sandbox_index, entity, &entity_assets, viewport);

		for (unsigned int index = 0; index < entity_assets.size; index++) {
			IncrementAssetReferenceInSandbox(editor_state, entity_assets[index].handle, entity_assets[index].type, sandbox_index, count);
		}

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
	bool is_shared_component;
};

ConvertToOrFromLinkData GetConvertToOrFromLinkData(
	EditorState* editor_state, 
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

	bool is_shared = IsReflectionTypeSharedComponent(target_type);

	Component target_id = { (short)target_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

	convert_data.base_data.allocator = allocator;
	convert_data.base_data.asset_database = editor_state->asset_database;
	convert_data.base_data.link_type = link_type;
	convert_data.base_data.module_link = link_target;
	convert_data.base_data.reflection_manager = editor_state->editor_components.internal_manager;
	convert_data.base_data.target_type = target_type;
	convert_data.component = target_id;
	convert_data.is_shared_component = is_shared;

	return convert_data;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertTargetToLinkComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<char> link_component,
	Entity entity,
	void* link_data, 
	AllocatorPolymorphic allocator,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	const EntityManager* active_entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	const void* target_data = nullptr;
	if (convert_data.is_shared_component) {
		target_data = active_entity_manager->GetSharedData(convert_data.component, active_entity_manager->GetSharedComponentInstance(convert_data.component, entity));
	}
	else {
		target_data = active_entity_manager->GetComponent(entity, convert_data.component);
	}

	return ConvertFromTargetToLinkComponent(
		&convert_data.base_data,
		target_data,
		link_data
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertTargetToLinkComponent(
	EditorState* editor_state,
	Stream<char> link_component,
	const void* target_data,
	void* link_data,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	return ConvertFromTargetToLinkComponent(&convert_data.base_data, target_data, link_data);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertLinkComponentToTarget(
	EditorState* editor_state, 
	unsigned int sandbox_index,
	Stream<char> link_component, 
	Entity entity, 
	const void* link_data, 
	AllocatorPolymorphic allocator,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	EntityManager* active_entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	void* target_data = nullptr;
	if (convert_data.is_shared_component) {
		target_data = active_entity_manager->GetSharedData(convert_data.component, active_entity_manager->GetSharedComponentInstance(convert_data.component, entity));
	}
	else {
		target_data = active_entity_manager->GetComponent(entity, convert_data.component);
	}

	return ConvertLinkComponentToTarget(
		&convert_data.base_data,
		link_data,
		target_data
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertLinkComponentToTarget(
	EditorState* editor_state,
	Stream<char> link_component,
	void* target_data,
	const void* link_data,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	return ConvertLinkComponentToTarget(&convert_data.base_data, link_data, target_data);
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

		ECS_STACK_CAPACITY_STREAM(UnregisterSandboxAssetElement, unregister_elements, 128);
		for (unsigned int index = 0; index < entity_assets.size; index++) {
			unregister_elements[index].handle = entity_assets[index].handle;
			unregister_elements[index].type = entity_assets[index].type;
		}
		unregister_elements.size = entity_assets.size;
		UnregisterSandboxAsset(editor_state, sandbox_index, unregister_elements);

		entity_manager->DeleteEntityCommit(entity);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);

		// If this entity belongs to the selected group, remove it from there as well
		RemoveSandboxSelectedEntity(editor_state, sandbox_index, entity);

		RenderSandboxViewports(editor_state, sandbox_index, { 0, 0 }, true);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

ComponentSignature EntityUniqueComponents(
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

ComponentSignature EntitySharedComponents(
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

SharedComponentSignature EntitySharedInstances(
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

SharedInstance EntitySharedInstance(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	SharedComponentSignature signature = EntitySharedInstances(editor_state, sandbox_index, entity, viewport);
	for (unsigned char index = 0; index < signature.count; index++) {
		if (signature.indices[index] == component) {
			return signature.instances[index];
		}
	}
	return { -1 };
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedInstance FindOrCreateSharedComponentInstance(
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

SharedInstance GetSharedComponentDefaultInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	size_t component_storage[ECS_KB * 8];
	editor_state->editor_components.ResetComponent(component, component_storage, true);
	return FindOrCreateSharedComponentInstance(editor_state, sandbox_index, component, component_storage, viewport);
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

MemoryArena* GetComponentAllocator(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return GetSandboxEntityManager(editor_state, sandbox_index, viewport)->GetComponentAllocator(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

MemoryArena* GetSharedComponentAllocator(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return GetSandboxEntityManager(editor_state, sandbox_index)->GetSharedComponentAllocator(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

Stream<char> GetEntityName(
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

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxComponentAssetsImplementation(
	const EditorState* editor_state,
	const void* component_data,
	Component component,
	bool shared,
	CapacityStream<AssetTypedHandle>* handles
) {
	if (component_data != nullptr) {
		const Reflection::ReflectionType* component_type = editor_state->editor_components.GetType(component, shared);

		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 128);
		ECS_STACK_CAPACITY_STREAM(unsigned int, int_handles, 128);

		GetAssetFieldsFromLinkComponentTarget(component_type, asset_fields);
		GetLinkComponentTargetHandles(component_type, editor_state->asset_database, component_data, asset_fields, int_handles.buffer);

		for (unsigned int index = 0; index < asset_fields.size; index++) {
			handles->AddAssert({ int_handles[index], asset_fields[index].type.type });
		}
	}
}

void GetSandboxEntityComponentAssetsImplementation(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles,
	bool shared,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		const void* component_data = GetSandboxEntityComponentEx(editor_state, sandbox_index, entity, component, shared, viewport);
		GetSandboxComponentAssetsImplementation(editor_state, component_data, component, shared, handles);
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
	GetSandboxEntityComponentAssetsImplementation(editor_state, sandbox_index, entity, component, handles, false, viewport);
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
	GetSandboxEntityComponentAssetsImplementation(editor_state, sandbox_index, entity, component, handles, true, viewport);
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
	GetSandboxComponentAssetsImplementation(editor_state, instance_data, component, true, handles);
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
	ComponentSignature unique_signature = EntityUniqueComponents(editor_state, sandbox_index, entity, viewport);
	ComponentSignature shared_signature = EntitySharedComponents(editor_state, sandbox_index, entity, viewport);

	for (unsigned char index = 0; index < unique_signature.count; index++) {
		GetSandboxEntityComponentAssets(editor_state, sandbox_index, entity, unique_signature[index], handles, viewport);
	}

	for (unsigned char index = 0; index < shared_signature.count; index++) {
		GetSandboxEntitySharedComponentAssets(editor_state, sandbox_index, entity, shared_signature[index], handles, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

template<bool has_translation, bool has_rotation>
void GetSandboxEntitiesMidpointImpl(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	Stream<Entity> entities,
	float3* translation_midpoint,
	Quaternion* rotation_midpoint,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	float3 translation_average = float3::Splat(0.0f);
	Quaternion rotation_average = QuaternionAverageCumulatorInitialize();

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

	if constexpr (has_translation) {
		*translation_midpoint = translation_average * float3::Splat(1.0f / entities.size);
	}
	if constexpr (has_rotation) {
		*rotation_midpoint = QuaternionAverageFromCumulator(rotation_average, entities.size);
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
	GetSandboxEntitiesMidpointImpl<true, false>(editor_state, sandbox_index, entities, &midpoint, nullptr, viewport);
	return midpoint;
}

// ------------------------------------------------------------------------------------------------------------------------------

Quaternion ECS_VECTORCALL GetSandboxEntitiesRotationMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	Quaternion midpoint;
	GetSandboxEntitiesMidpointImpl<false, true>(editor_state, sandbox_index, entities, nullptr, &midpoint, viewport);
	return midpoint;
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntitiesMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	float3* translation_midpoint, 
	Quaternion* rotation_midpoint, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetSandboxEntitiesMidpointImpl<true, true>(editor_state, sandbox_index, entities, translation_midpoint, rotation_midpoint, viewport);
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
			RemoveSandboxComponentAssets(editor_state, sandbox_index, component, data, false);

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
			SharedInstance previous_instance = EntitySharedInstance(editor_state, sandbox_index, entity, component, viewport);
			entity_manager->RemoveSharedComponentCommit(entity, { &component, 1 });
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);

			bool is_unreferenced = entity_manager->IsUnreferencedSharedInstance(component, previous_instance);
			if (is_unreferenced) {
				const void* instance_data = entity_manager->GetSharedData(component, previous_instance);
				RemoveSandboxComponentAssets(editor_state, sandbox_index, component, instance_data, true);
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
	bool shared,
	Stream<LinkComponentAssetField> asset_fields
) {
	ECS_STACK_CAPACITY_STREAM(UnregisterSandboxAssetElement, unregister_elements, 512);

	if (asset_fields.size == 0) {
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, component_asset_handles, 512);
		GetSandboxComponentAssetsImplementation(editor_state, data, component, shared, &component_asset_handles);

		for (unsigned int index = 0; index < component_asset_handles.size; index++) {
			unregister_elements[index].handle = component_asset_handles[index].handle;
			unregister_elements[index].type = component_asset_handles[index].type;
		}
		unregister_elements.size = component_asset_handles.size;
	}
	else {
		const Reflection::ReflectionType* component_type = editor_state->editor_components.GetType(component, shared);
		ECS_STACK_CAPACITY_STREAM(unsigned int, handles, 512);
		GetLinkComponentTargetHandles(component_type, editor_state->asset_database, data, asset_fields, handles.buffer);

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
		// It has built in checks for not crashing
		editor_state->editor_components.ResetComponentFromManager(entity_manager, component_name, entity);
	}
	else {
		// Remove it and then add it again with the default component
		RemoveSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, viewport);
		AddSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, { -1 }, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void SandboxForEachEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ForEachEntityFunctor functor, 
	void* functor_data, 
	ComponentSignature unique_signature, 
	ComponentSignature shared_signature, 
	ComponentSignature unique_exclude_signature, 
	ComponentSignature shared_exclude_signature,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	World temp_world;
	memcpy(&temp_world, &GetSandbox(editor_state, sandbox_index)->sandbox_world, sizeof(temp_world));

	temp_world.entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	ForEachEntityCommitFunctor(&temp_world, functor, functor_data, unique_signature, shared_signature, unique_exclude_signature, shared_exclude_signature);
}

// ------------------------------------------------------------------------------------------------------------------------------

struct SplatLinkComponentBasicData {
	const void* link_component;
	const Reflection::ReflectionType* target_type;
	Stream<LinkComponentAssetField> asset_fields;
	Stream<Stream<void>> assets;
};

void SplatLinkComponentBasic(ForEachEntityFunctorData* functor_data) {
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

void SplatLinkComponentBuild(ForEachEntityFunctorData* functor_data) {
	SplatLinkComponentBuildData* data = (SplatLinkComponentBuildData*)functor_data->data;

	ModuleLinkComponentFunctionData function_data;
	function_data.assets = data->assets;
	function_data.component = functor_data->unique_components[0];
	function_data.link_component = data->link_component;
	data->link_function(&function_data);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool SandboxSplatLinkComponentAssetFields(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	const void* link_component, 
	Stream<char> component_name, 
	bool give_error_when_failing,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
	if (type == nullptr) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component doesn't exist.", component_name);
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

	ECS_STACK_CAPACITY_STREAM(Stream<void>, assets, 512);
	ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
	GetAssetFieldsFromLinkComponent(type, asset_fields);
	if (asset_fields.size == 0) {
		return true;
	}

	GetLinkComponentAssetData(type, link_component, editor_state->asset_database, asset_fields, &assets);

	if (GetReflectionTypeLinkComponentNeedsDLL(type)) {
		unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, component_name);
		if (module_index == -1) {
			if (give_error_when_failing) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component was not found in the loaded modules.", component_name);
				EditorSetConsoleError(error_message);
			}
			return false;
		}
		ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, module_index, component_name);
		if (link_target.build_function == nullptr) {
			if (give_error_when_failing) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component requires a DLL function but none was found.", component_name);
				EditorSetConsoleError(error_message);
			}
			return false;
		}

		SplatLinkComponentBuildData splat_data;
		splat_data.link_component = link_component;
		splat_data.assets = assets;
		splat_data.link_function = link_target.build_function;
		SandboxForEachEntity(editor_state, sandbox_index, SplatLinkComponentBuild, &splat_data, { &component, 1 }, { nullptr, 0 }, {}, {}, viewport);
		return true;
	}

	SplatLinkComponentBasicData splat_data;
	splat_data.link_component = link_component;
	splat_data.asset_fields = asset_fields;
	splat_data.assets = assets;
	
	splat_data.target_type = editor_state->editor_components.GetType(editor_state->editor_components.GetComponentFromLink(component_name));
	if (splat_data.target_type == nullptr) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The target component was not found.", component_name);
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	if (!ValidateLinkComponent(type, splat_data.target_type)) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The link component {#} cannot be resolved with default conversion to {#}.", component_name, splat_data.target_type->name);
			EditorSetConsoleError(error_message);
		}
		return false;
	}
	SandboxForEachEntity(editor_state, sandbox_index, SplatLinkComponentBasic, &splat_data, { &component, 1 }, { nullptr, 0 }, {}, {}, viewport);
	return true;
}

// ------------------------------------------------------------------------------------------------------------------------------

// The functor receives the component as parameter
template<typename GetData>
bool UpdateLinkComponentBase(
	EditorState* editor_state,
	unsigned int sandbox_index,
	const void* link_component,
	Stream<char> component_name,
	bool give_error_when_failing,
	GetData&& get_data
) {
	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
	if (type == nullptr) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The component doesn't exist.", component_name);
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(component_name);
	if (target.size == 0) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The component does not have a target.", component_name);
			EditorSetConsoleError(error_message);
		}
		return false;
	}
	const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target);
	if (target_type == nullptr) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The target {#} does not exist.", component_name, target);
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, component_name);
	if (module_index == -1) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The module from which it came could not be identified.", component_name);
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, module_index, component_name);
	Component component = { (short)target_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

	void* converted_data = get_data(component);
	if (converted_data == nullptr) {
		return false;
	}

	ConvertToAndFromLinkBaseData convert_data;
	convert_data.reflection_manager = editor_state->GlobalReflectionManager();
	convert_data.link_type = type;
	convert_data.target_type = target_type;
	convert_data.asset_database = editor_state->asset_database;
	convert_data.module_link = link_target;
	bool success = ConvertLinkComponentToTargetAssetsOnly(&convert_data, link_component, converted_data);
	if (!success) {
		if (give_error_when_failing) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The dll function is missing or the default link is incompatible.", component_name);
			EditorSetConsoleError(error_message);
		}
	}
	return success;
}

bool SandboxUpdateLinkComponentSharedInstance(
	EditorState* editor_state,
	unsigned int sandbox_index,
	const void* link_component, 
	Stream<char> component_name,
	SharedInstance instance,
	bool give_error_when_failing,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return UpdateLinkComponentBase(editor_state, sandbox_index, link_component, component_name, give_error_when_failing, [=](Component component) {
		void* shared_data = GetSandboxSharedInstance(editor_state, sandbox_index, component, instance, viewport);
		if (shared_data == nullptr) {
			if (give_error_when_failing) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The given shared instance {#} is invalid for {#}.", 
					component_name, instance.value, ViewportString(viewport));
				EditorSetConsoleError(error_message);
			}
		}
		return shared_data;
	});
}

// ------------------------------------------------------------------------------------------------------------------------------

bool SandboxUpdateLinkComponentForEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	const void* link_component, 
	Stream<char> component_name, 
	Entity entity,
	bool give_error_when_failing,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	size_t converted_link_storage[ECS_KB * 4];
	bool success = ConvertLinkComponentToTarget(editor_state, component_name, converted_link_storage, link_component);
	if (!success) {
		if (give_error_when_failing) {
			ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 256);
			Stream<char> entity_name = GetEntityName(editor_state, sandbox_index, entity, entity_name_storage);
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert linked component {#} to target for entity {#} in sandbox {#} for {#}.", 
				component_name, entity_name, sandbox_index, ViewportString(viewport));
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	// Find or create the shared instance that matches this data
	Component target_component = editor_state->editor_components.GetComponentIDWithLink(component_name);
	SharedInstance new_shared_instance = FindOrCreateSharedComponentInstance(editor_state, sandbox_index, target_component, converted_link_storage, viewport);
	SharedInstance current_instance = EntitySharedInstance(editor_state, sandbox_index, entity, target_component, viewport);

	if (new_shared_instance != current_instance) {
		EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
		SharedInstance old_instance = entity_manager->ChangeEntitySharedInstanceCommit(entity, target_component, new_shared_instance, true);

		// Unregister the shared instance if it longer is referenced
		entity_manager->UnregisterUnreferencedSharedInstanceCommit(target_component, old_instance);
	}

	// It can happen that the handle is not modified and so it will, in fact, have the same shared instance
	// Only change the shared instance in that case
	return true;
}

// ------------------------------------------------------------------------------------------------------------------------------
