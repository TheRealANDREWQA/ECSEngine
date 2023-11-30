#include "editorpch.h"
#include "EditorBaseEntityOperations.h"
#include "EditorState.h"
#include "ECSEngineComponentsAll.h"
#include "../Modules/Module.h"

// ------------------------------------------------------------------------------------------------------------------------------

void GetComponentAssets(
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

static void GetSandboxEntityComponentAssetsImplementation(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles,
	ECS_COMPONENT_TYPE component_type
) {
	if (component_type == ECS_COMPONENT_GLOBAL) {
		const void* component_data = entity_manager->TryGetGlobalComponent(component);
		GetComponentAssets(editor_state, component_data, component, component_type, handles);
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
			GetComponentAssets(editor_state, component_data, component, component_type, handles);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEntityComponentAssets(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles
)
{
	GetSandboxEntityComponentAssetsImplementation(editor_state, entity_manager, entity, component, handles, ECS_COMPONENT_UNIQUE);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEntitySharedComponentAssets(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles
)
{
	GetSandboxEntityComponentAssetsImplementation(editor_state, entity_manager, entity, component, handles, ECS_COMPONENT_SHARED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEntityAssets(const EditorState* editor_state, const EntityManager* entity_manager, Entity entity, CapacityStream<AssetTypedHandle>* handles)
{
	ComponentSignature unique_signature = entity_manager->GetEntitySignatureStable(entity);
	SharedComponentSignature shared_signature = entity_manager->GetEntitySharedSignatureStable(entity);

	for (unsigned char index = 0; index < unique_signature.count; index++) {
		GetEntityComponentAssets(editor_state, entity_manager, entity, unique_signature[index], handles);
	}

	for (unsigned char index = 0; index < shared_signature.count; index++) {
		GetEntitySharedComponentAssets(editor_state, entity_manager, entity, shared_signature.indices[index], handles);
	}
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

// -----------------------------------------------------------------------------------------------------------------------------