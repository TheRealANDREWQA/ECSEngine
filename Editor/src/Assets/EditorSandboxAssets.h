#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineAssets.h"
#include "ECSEngineComponentsAll.h"
#include "../Editor/EditorEvent.h"
#include "Assets/AssetManagement.h"

struct EditorState;

using namespace ECSEngine;
ECS_TOOLS;

struct LinkComponentWithAssetFields {
	const Reflection::ReflectionType* type;
	Stream<LinkComponentAssetField> asset_fields;
};

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes link components
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is cleared
void DeallocateAssetWithRemapping(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes link components
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is cleared. This version will copy the old_metadata and deallocate the asset based
// on that metadata instead of the current version. It will still remap the pointer tho
void DeallocateAssetWithRemapping(EditorState* editor_state, const void* old_metadata, ECS_ASSET_TYPE type, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes link components
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is reset.
void DeallocateAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// If the runtime version of the metadata is different from the file version, then it will deallocate the asset.
// If they match it will not do anything.
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is reset.
void DeallocateAssetsWithRemappingMetadataChange(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// Fills in the sandboxes which use this asset
void GetAssetSandboxesInUse(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, CapacityStream<unsigned int>* sandboxes);

// -------------------------------------------------------------------------------------------------------------

// It fills in assets which are missing. There must be ECS_ASSET_TYPE_COUNT missing assets streams, one for each type
void GetSandboxMissingAssets(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets);

// -------------------------------------------------------------------------------------------------------------

// There must be unique_count elements (can be retrieved with GetMaxComponent() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures and samplers by materials) it will report those fields as well
void GetLinkComponentsWithAssetFieldsUnique(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	LinkComponentWithAssetFields* link_with_fields, 
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// There must be unique_count elements (can be retrieved with GetMaxComponent() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures and samplers by materials) it will report those fields as well
void GetLinkComponentsWithAssetFieldsUnique(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	LinkComponentWithAssetFields* link_with_fields,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// -------------------------------------------------------------------------------------------------------------

// There must be shared_count elements (can be retrieved with GetMaxSharedComponent() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetLinkComponentsWithAssetFieldsShared(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	LinkComponentWithAssetFields* link_with_fields,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// There must be shared_count elements (can be retrieved with GetMaxSharedComponent() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetLinkComponentsWithAssetFieldsShared(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	LinkComponentWithAssetFields* link_with_fields,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// -------------------------------------------------------------------------------------------------------------

// There must be global_count elements (can be retrieved with GetGlobalComponentCount() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetLinkComponentsWithAssetFieldsGlobal(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	LinkComponentWithAssetFields* link_with_fields,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// There must be global_count elements (can be retrieved with GetGlobalComponentCount() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetLinkComponentsWithAssetFieldsGlobal(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	LinkComponentWithAssetFields* link_with_fields,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// -------------------------------------------------------------------------------------------------------------

// Increments the reference count for that asset in the given sandbox
void IncrementAssetReferenceInSandbox(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, unsigned int sandbox_index, unsigned int count = 1);

// -------------------------------------------------------------------------------------------------------------

// This check the database associated with the sandbox to see if the asset appears in that database
bool IsAssetReferencedInSandbox(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, unsigned int sandbox_index = -1);

// -------------------------------------------------------------------------------------------------------------

// This check the database associated with the sandbox to see if the asset appears in that database
bool IsAssetReferencedInSandbox(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, unsigned int sandbox_index = -1);

// -------------------------------------------------------------------------------------------------------------

// This will search the entities in the sandbox to see if their components reference the asset
// If the sandbox index is left to -1, then it will search all sandboxes.
bool IsAssetReferencedInSandboxEntities(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, unsigned int sandbox_index = -1);

// -------------------------------------------------------------------------------------------------------------

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// runtime start new sandboxes
void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets);

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// runtime start new sandboxes. It will call the callback once the load is finished (both when it is successful and when it failed)
void LoadSandboxMissingAssets(
	EditorState* editor_state,
	unsigned int sandbox_index,
	CapacityStream<unsigned int>* missing_assets,
	EditorEventFunction callback,
	void* callback_data,
	size_t callback_data_size
);

// -------------------------------------------------------------------------------------------------------------

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// editor start sandboxes. It will copy the current asset handles before forwarding to the event
void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index);

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// editor start sandboxes. It will copy the current asset handles before forwarding to the event.
// After the load is finalized (with success or not), it will call the callback
void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index, EditorEventFunction callback, void* callback_data, size_t callback_data_size);

// -------------------------------------------------------------------------------------------------------------

// This should be called when the asset is used in a sandbox. The file is ignored for materials and samplers
// It writes the handle of the asset. If the deserialization failed, it will write -1
// The reason for which it takes a pointer to the handle is that this may not run when called
// due to a prevention of resource loads. It returns true if the resource already exists and no asynchronous load is done
// Can optionally give a callback to call when the load is finalized. A structure of type RegisterAssetEventCallbackInfo
// will be given in the additional_data member
bool RegisterSandboxAsset(
	EditorState* editor_state,
	unsigned int sandbox_index, 
	Stream<char> name,
	Stream<wchar_t> file,
	ECS_ASSET_TYPE type, 
	unsigned int* handle, 
	bool unload_if_existing = false,
	UIActionHandler callback = {}
);

// -------------------------------------------------------------------------------------------------------------

// A stream corresponds to an asset type. The ints are handles. It checks to see if there is anything to reload
// and quit early if there is none
void ReloadAssets(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload);

// -------------------------------------------------------------------------------------------------------------

// A stream corresponds to an asset type. The ints are handles. It checks to see if there is anything to reload
// and quit early if there is none. This version will load the metadata from the file and if it matches
// the in-memory version then it will only create the asset, it will not attempt to deallocate it. If it is different,
// then it will also unload the asset first and remap the pointer
void ReloadAssetsMetadataChange(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload);

// -------------------------------------------------------------------------------------------------------------

// Can optionally give a callback to call before the unload is finalized. The handle will be given in the additional_data field
// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback = {});

// -------------------------------------------------------------------------------------------------------------

// Can optionally give a callback to call when the unload is finalized. The handle will be given in the additional_data field
// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<UnregisterSandboxAssetElement> elements, UIActionHandler callback = {});

// Can optionally give a callback to call when the unload is finalized. The handle will be given in the additional_data field
// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<AssetTypedHandle> elements, UIActionHandler callback = {});

// Can optionally give a callback to call when the unload is finalized. The handle will be given in the additional_data field
// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<Stream<unsigned int>> elements, UIActionHandler callback = {});

// -------------------------------------------------------------------------------------------------------------

// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxLinkComponent(EditorState* editor_state, unsigned int sandbox_index, const void* link_component, Stream<char> component_name);

// -------------------------------------------------------------------------------------------------------------

// Unloads all assets in use by the given sandbox. It will copy the current handles before forwarding to the event
// If the clear database reference is set to true, then it will clear the database after the assets have been unloaded
// It will report any failures that have happened. (The clear database reference is helpful when destroying a sandbox
// and the database reference no longer exists and the flag set to false will prevent an access to it)
void UnloadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index, bool clear_database_reference = false);

// -------------------------------------------------------------------------------------------------------------

// Finds all unique and shared components that reference this asset and updates their values
// If the sandbox index is not specified, it will then apply the operation to all sandboxes
void UpdateAssetToComponents(
	EditorState* editor_state,
	Stream<void> old_asset,
	Stream<void> new_asset,
	ECS_ASSET_TYPE asset_type,
	unsigned int sandbox_index = -1
);

// -------------------------------------------------------------------------------------------------------------

struct UpdateAssetToComponentElement {
	ECS_INLINE bool IsAssetDifferent() const {
		return old_asset.buffer != new_asset.buffer || old_asset.size != new_asset.size;
	}

	Stream<void> old_asset;
	Stream<void> new_asset;
	ECS_ASSET_TYPE type;
};

// Finds all unique and shared components that reference this asset and updates their values
// If the sandbox index is not specified, it will then apply the operation to all sandboxes
void UpdateAssetsToComponents(
	EditorState* editor_state,
	Stream<UpdateAssetToComponentElement> elements,
	unsigned int sandbox_index = -1
);

// Finds all unique and shared components that reference this asset and updates their values
void UpdateAssetsToComponents(
	EditorState* editor_state,
	Stream<UpdateAssetToComponentElement> elements,
	EntityManager* entity_manager
);

// -------------------------------------------------------------------------------------------------------------