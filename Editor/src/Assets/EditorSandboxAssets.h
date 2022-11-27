#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineAssets.h"
#include "../Editor/EditorEvent.h"

struct EditorState;

using namespace ECSEngine;

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes link components
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is reset
void DeallocateAssetWithRemapping(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes link components
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is reset
void DeallocateAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// It fills in assets which are missing. There must be ECS_ASSET_TYPE_COUNT missing assets streams, one for each type
void GetSandboxMissingAssets(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets);

// -------------------------------------------------------------------------------------------------------------

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// runtime start new sandboxes
void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets);

// -------------------------------------------------------------------------------------------------------------

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// editor start runtimes. It will copy the current asset handles before forwarding to the event
void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void LoadAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles);

// -------------------------------------------------------------------------------------------------------------

// This should be called when the asset is used in a sandbox. The file is ignored for materials and samplers
// It writes the handle of the asset. If the deserialization failed, it will write -1
// The reason for which it takes a pointer to the handle is that this may not run when called
// due to a prevention of resource loads. It returns true if the resource already exists and no asynchronous load is done
// Can optionally give a callback to call when the load is finalized. The handle will be given in the additional_data field
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

// A stream corresponds to an asset type. The ints are handles. Does not check to see if there are
// no assets to be reloaded to quit early
void ReloadAssets(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload);

// -------------------------------------------------------------------------------------------------------------

// Can optionally give a callback to call before the unload is finalized. The handle will be given in the additional_data field
// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback = {});

// -------------------------------------------------------------------------------------------------------------

struct UnregisterSandboxAssetElement {
	unsigned int handle;
	ECS_ASSET_TYPE type;
};

// Can optionally give a callback to call when the unload is finalized. The handle will be given in the additional_data field
// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<UnregisterSandboxAssetElement> elements, UIActionHandler callback = {});

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

// -------------------------------------------------------------------------------------------------------------