#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineAssets.h"
#include "ECSEngineComponentsAll.h"
#include "../Editor/EditorEvent.h"
#include "Assets/AssetManagement.h"

struct EditorState;

using namespace ECSEngine;
ECS_TOOLS;

struct ComponentWithAssetFields {
	const Reflection::ReflectionType* type;
	Stream<ComponentAssetField> asset_fields;
};

namespace ECSEngine {
	struct SceneDeltaReaderAssetCallbackData;
}

// -------------------------------------------------------------------------------------------------------------

void CopySandboxAssetReferences(EditorState* editor_state, unsigned int source_sandbox_index, unsigned int destination_sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes components that reference this asset
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is cleared
void DeallocateAssetWithRemapping(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes components that reference this asset
// If the commit flag is set to true then it will execute it immediately, otherwise it will postpone until
// the resource loading editor state flag is cleared. This version will copy the old_metadata and deallocate the asset based
// on that metadata instead of the current version. It will still remap the pointer tho
void DeallocateAssetWithRemapping(EditorState* editor_state, const void* old_metadata, ECS_ASSET_TYPE type, bool commit = false);

// -------------------------------------------------------------------------------------------------------------

// It will just deallocate the runtime asset and modify the sandbox scenes components that reference this asset
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
void GetComponentsWithAssetFieldsUnique(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	ComponentWithAssetFields* component_with_assets, 
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// There must be unique_count elements (can be retrieved with GetMaxComponent() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures and samplers by materials) it will report those fields as well
void GetComponentsWithAssetFieldsUnique(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// -------------------------------------------------------------------------------------------------------------

// There must be shared_count elements (can be retrieved with GetMaxSharedComponent() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetComponentsWithAssetFieldsShared(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// There must be shared_count elements (can be retrieved with GetMaxSharedComponent() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetComponentsWithAssetFieldsShared(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// -------------------------------------------------------------------------------------------------------------

// There must be global_count elements (can be retrieved with GetGlobalComponentCount() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetComponentsWithAssetFieldsGlobal(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// There must be global_count elements (can be retrieved with GetGlobalComponentCount() on the entity_manager) in the given pointer
// These are mapped to the component value - can index directly
// When the deep search is set to true, for assets that can be referenced by other assets
// (i.e. textures, samplers and shaders by materials) it will report those fields as well
void GetComponentsWithAssetFieldsGlobal(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search = true
);

// -------------------------------------------------------------------------------------------------------------

struct SandboxReferenceCountsFromEntities {
	ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
		DeallocateAssetReferenceCountsFromEntities(counts, allocator);
		counts.Deallocate(allocator);
	}
	
	Stream<Stream<unsigned int>> counts;
};

// Viewport can be count for the active entity manager
// If you want to deallocate this 
SandboxReferenceCountsFromEntities GetSandboxAssetReferenceCountsFromEntities(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport,
	AllocatorPolymorphic allocator
);

// The functor receives as arguments (ECS_ASSET_TYPE type, unsigned int handle, int reference_count_change)
template<typename Functor>
void ForEachSandboxAssetReferenceDifference(
	const EditorState* editor_state,
	SandboxReferenceCountsFromEntities previous_counts,
	SandboxReferenceCountsFromEntities current_counts,
	Functor functor
) {
	ForEachAssetReferenceDifference(editor_state->asset_database, previous_counts.counts, current_counts.counts, functor);
}

// -------------------------------------------------------------------------------------------------------------

void IncrementAssetReference(unsigned int handle, ECS_ASSET_TYPE type, AssetDatabaseReference* reference, unsigned int count = 1);

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
	LoadEditorAssetsOptionalData* optional_data
);

// -------------------------------------------------------------------------------------------------------------

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// editor start sandboxes. It will copy the current asset handles before forwarding to the event
void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index);

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// editor start sandboxes. It will copy the current asset handles before forwarding to the event.
// After the load is finalized (with success or not), it will call the callback
void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index, LoadEditorAssetsOptionalData* optional_data);

// -------------------------------------------------------------------------------------------------------------

// This should be called when the asset is used in a sandbox. The file is ignored for materials and samplers
// It writes the handle of the asset. If the deserialization failed, it will write -1
// The reason for which it takes a pointer to the handle is that this may not run when called
// due to a prevention of resource loads. It returns true if the resource already exists and no asynchronous load is done
// Can optionally give a callback to call when the load is finalized. A structure of type RegisterAssetEventCallbackInfo
// will be given in the additional_data member. The last parameter indicates whether or not the callback needs to run
// On the main thread or not. If true, it will run on the main thread, else it can run on worker threads
bool RegisterSandboxAsset(
	EditorState* editor_state,
	unsigned int sandbox_index, 
	Stream<char> name,
	Stream<wchar_t> file,
	ECS_ASSET_TYPE type, 
	unsigned int* handle, 
	bool unload_if_existing = false,
	UIActionHandler callback = {},
	bool callback_is_single_threaded = false
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

// This is the data that a replay sandbox callback needs to operate for a sandbox. A structure will be allocated
// From the temporary allocator and returned.
Stream<void> GetReplaySandboxAssetsCallbackData(EditorState* editor_state, unsigned int sandbox_index, AllocatorPolymorphic temporary_allocator);

// This is a callback that the sandbox replay feature can use to load assets for a specific sandbox
bool ReplaySandboxAssetsCallback(SceneDeltaReaderAssetCallbackData* functor_data);

// -------------------------------------------------------------------------------------------------------------

// Can optionally give a callback to call before the unload is finalized. The handle will be given in the additional_data field
// Can give -1 as the sandbox index in order to eliminate the asset from all sandboxes
void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback = {});

// -------------------------------------------------------------------------------------------------------------

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

// Unloads all assets in use by the given sandbox. It will copy the current handles before forwarding to an event.
// It will report any failures that have happened
void UnloadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index);

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