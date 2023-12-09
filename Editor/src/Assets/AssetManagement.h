#pragma once
#include "ECSEngineAssets.h"
#include "ECSEngineUI.h"
#include "../Editor/EditorEventDef.h"

using namespace ECSEngine;
ECS_TOOLS;

struct EditorState;

struct CreateAssetInternalDependenciesElement {
	// Returns true if the asset was actually created and the pointer was changed
	ECS_INLINE bool WasChanged() const {
		return success && (old_asset.buffer != new_asset.buffer || old_asset.size != new_asset.size);
	}

	unsigned int handle;
	ECS_ASSET_TYPE type;
	bool success;
	Stream<void> old_asset;
	Stream<void> new_asset;
};

struct RegisterAssetEventCallbackInfo {
	// This is the handle value before the selection
	unsigned int previous_handle;
	unsigned int handle;
	ECS_ASSET_TYPE type;
	bool success;
};

struct UnregisterAssetEventCallbackInfo {
	unsigned int handle;
	ECS_ASSET_TYPE type;
};

void AddLoadingAssets(EditorState* editor_state, Stream<Stream<unsigned int>> handles);

// There must be ECS_ASSET_TYPE_COUNT entries for the handles pointer
void AddLoadingAssets(EditorState* editor_state, CapacityStream<unsigned int>* handles);

void AddLoadingAssets(EditorState* editor_state, Stream<AssetTypedHandle> handles);

// The sandbox_assets boolean should be true if the assets belong to a sandbox
// If they belong to sandboxes and the sandbox_index is -1 then it will remove it
// from every sandbox that contains it. The callback receives in the additional info field
// a structure of type UnregisterAssetEventCallbackInfo
void AddUnregisterAssetEvent(
	EditorState* editor_state,
	Stream<AssetTypedHandle> elements,
	bool sandbox_assets,
	unsigned int sandbox_index = -1,
	UIActionHandler callback = {}
);

// If the database reference is specified, then it will remove occurences from it as well
// The callback receives in the additional info field
// a structure of type UnregisterAssetEventCallbackInfo
void AddUnregisterAssetEvent(
	EditorState* editor_state,
	Stream<AssetTypedHandle> elements,
	AssetDatabaseReference* database_reference = nullptr,
	UIActionHandler callback = {}
);

// The sandbox_assets boolean should be true if the assets belong to a sandbox
// If they belong to sandboxes and the sandbox_index is -1 then it will remove it
// from every sandbox that contains it. The callback receives in the additional info field
// a structure of type UnregisterAssetEventCallbackInfo
void AddUnregisterAssetEventHomogeneous(
	EditorState* editor_state,
	Stream<Stream<unsigned int>> elements,
	bool sandbox_assets,
	unsigned int sandbox_index = -1,
	UIActionHandler callback = {}
);

// The sandbox_assets boolean should be true if the assets belong to a sandbox
// If they belong to sandboxes and the sandbox_index is -1 then it will remove it
// from every sandbox that contains it. The callback receives in the additional info field
// a structure of type UnregisterAssetEventCallbackInfo
void AddUnregisterAssetEventHomogeneous(
	EditorState* editor_state,
	Stream<Stream<unsigned int>> elements,
	AssetDatabaseReference* database_reference = nullptr,
	UIActionHandler callback = {}
);

// If the sandbox index is -1, then it will not add it to a sandbox database
// It returns true if the resource already exists and no asynchronous load is done
bool AddRegisterAssetEvent(
	EditorState* editor_state,
	Stream<char> name,
	Stream<wchar_t> file,
	ECS_ASSET_TYPE type,
	unsigned int* handle,
	unsigned int sandbox_index = -1,
	bool unload_if_existing = false,
	UIActionHandler callback = {}
);

// The path does not need to have the extension. It will be appended automatically
// This is a thunk file (it does not contain anything)
bool CreateMaterialFile(const EditorState* editor_state, Stream<wchar_t> relative_path);

// The path does not need to have the extension. It will be appended automatically
// This is a thunk file (it does not contain anything)
bool CreateSamplerFile(const EditorState* editor_state, Stream<wchar_t> relative_path);

// The path does not need to have the extension. It will be appended automatically
// If you set the shader_type to ECS_SHADER_TYPE_COUNT, it will not append any extension
bool CreateShaderFile(const EditorState* editor_state, Stream<wchar_t> relative_path, ECS_SHADER_TYPE shader_type);

// The path does not need to have the extension. It will be appended automatically
bool CreateMiscFile(const EditorState* editor_state, Stream<wchar_t> relative_path);

// This only creates the disk file for the new setting. It does not add it to the database
// Returns true if it succeeded else false
bool CreateAssetSetting(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// This only creates the disk file for the new setting. It does not add it to the database
// Returns true if it succeeded else false
bool CreateShaderSetting(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_SHADER_TYPE shader_type);

// Returns true if it succeeded, else false.
// This doesn't check the prevent resource load flag and wait until is cleared
bool CreateAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// This will run on a background thread. The callback receives as additional_data a structure
// of type CreateAssetAsyncCallbackInfo.
void CreateAssetAsync(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback = {});

// Returns true if all internal dependencies were successfully created, else false. You can optionally request
// the dependency elements to be filled in
bool CreateAssetInternalDependencies(
	EditorState* editor_state, 
	unsigned int handle, 
	ECS_ASSET_TYPE type, 
	CapacityStream<CreateAssetInternalDependenciesElement>* dependency_elements = nullptr
);

// Returns true if all internal dependencies were successfully created, else false. If a dependency is already loaded
// it will simply skip it You can optionally request the dependency elements to be filled in. 
bool CreateAssetInternalDependencies(
	EditorState* editor_state,
	const void* metadata,
	ECS_ASSET_TYPE type,
	CapacityStream<CreateAssetInternalDependenciesElement>* dependency_elements = nullptr
);

// If an asset like a mesh or a texture doesn't have a default setting, it creates one
void CreateAssetDefaultSetting(const EditorState* editor_state);

void ChangeAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, size_t new_stamp);

void ChangeAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, size_t new_stamp);

struct DeallocateAssetDependency {
	unsigned int handle;
	ECS_ASSET_TYPE type;
	// This is the previous value of the built asset
	Stream<void> previous_pointer;
	// The new randomized pointer value
	Stream<void> new_pointer;
};

// Returns true if it succeeded, else false. It can fail if the resource doesn't exist
// This doesn't check the prevent resource load flag and wait until is cleared.
// If the recurse flag is set to true, then it will deallocate all assets that depend on it
// If the recurse flag is true and the dependent_assets is given it will fill in the assets that were deallocated
// during the recursion (those deallocations that were successful, including the current handle)
bool DeallocateAsset(
	EditorState* editor_state, 
	unsigned int handle, 
	ECS_ASSET_TYPE type, 
	bool recurse = true, 
	CapacityStream<DeallocateAssetDependency>* dependent_assets = nullptr
);

// Returns true if it succeeded, else false. It can fail if the resource doesn't exist
// This doesn't check the prevent resource load flag and wait until is cleared
// If the recurse flag is set to true, then it will deallocate all assets that depend on it
// If the recurse flag is true and the dependent_assets is given it will fill in the assets that were deallocated
// during the recursion (those deallocations that were successful, including the current metadata (if the metadata doesn't exist in the database it will have the handle as -1))
bool DeallocateAsset(EditorState* editor_state, void* metadata, ECS_ASSET_TYPE type, bool recurse = true, CapacityStream<DeallocateAssetDependency>* dependent_assets = nullptr);

// It may not run immediately since it may be prevented from loading resources
void DeleteAssetSetting(EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// Deletes all asset metadata files which have their asset missing
void DeleteMissingAssetSettings(const EditorState* editor_state);

// Returns true if the asset was successfully decremented. It can fail if it is evicted and it could not be deallocated
// If the sandbox index is different from -1, it will also remove a reference from the given sandbox
bool DecrementAssetReference(
	EditorState* editor_state, 
	unsigned int handle, 
	ECS_ASSET_TYPE type, 
	unsigned int sandbox_index = -1, 
	unsigned int decrement_count = 1,
	bool* was_removed = nullptr
);

// Returns true if the asset was successfully decremented. It can fail if it is evicted and it could not be deallocated
// If the database reference is not nullptr, it will remove a reference from it as well
bool DecrementAssetReference(
	EditorState* editor_state, 
	unsigned int handle, 
	ECS_ASSET_TYPE type, 
	AssetDatabaseReference* database_reference,
	unsigned int decrement_count = 1,
	bool* was_removed = nullptr
);

// Returns true if the asset already exists in the asset database or not. For materials and samplers the file parameter is ignored
bool ExistsAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// Returns true if the asset exists in the resource manager
bool ExistsAssetInResourceManager(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// Returns true if the assets exists in the resource manager
bool ExistsAssetInResourceManager(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// It will remove the asset from the asset database without deallocating it from the resource manager
// Also, it will remove the time stamp associated with this asset from the database
void EvictAssetFromDatabase(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// It will remove in place all the assets that are already loaded
Stream<Stream<unsigned int>> FilterUnloadedAssets(const EditorState* editor_state, Stream<Stream<unsigned int>> handles);

// Returns the handle of the asset. Returns -1 if it doesn't exist
unsigned int FindAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// Returns the handle to that asset. If it doesn't exist, it will add it without creating the runtime asset
// Can optionally specify whether or not to update the reference count (when it already exists)
unsigned int FindOrAddAsset(EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool increment_reference_count = false);

// Returns the index in the background loading array, -1 if it is not being loaded
unsigned int FindAssetBeingLoaded(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// Fills in the relative path to the asset from the name (materials, gpu samplers, shaders and misc are available at the moment)
void FromAssetNameToThunkOrForwardingFile(Stream<char> name, Stream<wchar_t> extension, CapacityStream<wchar_t>& relative_path);

// Fills in the relative path to the asset from the name (materials, gpu samplers, shaders and misc are available at the moment)
void FromAssetNameToThunkOrForwardingFile(Stream<char> name, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& relative_path);

// Thunk files are those for materials and gpu samplers and forwarding files are shaders and misc
void GetAssetNameFromThunkOrForwardingFile(const EditorState* editor_state, Stream<wchar_t> absolute_path, CapacityStream<char>& name);

// Thunk files are those for materials and gpu samplers and forwarding files are shaders and misc
void GetAssetNameFromThunkOrForwardingFileRelative(const EditorState* editor_state, Stream<wchar_t> relative_path, CapacityStream<char>& name);

// Returns true if it finds a file that targets the given metadata. If the absolute_path is set to true, then it will give the absolute path
// to the file (e.g. C:\ProjectPath\Assets\Texture.jpg), else it will only write the relative path to the assets folder
bool GetAssetFileFromAssetMetadata(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& path, bool absolute_path = true);

// Returns true if it finds a file that targets the given metadata. If the absolute_path is set to true, then it will give the absolute path
// to the file (e.g. C:\ProjectPath\Assets\Texture.jpg), else it will only write the relative path to the assets folder
bool GetAssetFileFromAssetMetadata(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& path, bool absolute_path = true);

// This is used only for meshes and textures (these are a special case. They are not thunk files nor forwarding files)
Stream<Stream<char>> GetAssetCorrespondingMetadata(const EditorState* editor_state, Stream<wchar_t> file, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator);

// Fills in the handles of the assets that are out of date (have been changed from outside, like a mesh or a texture, ECS files as well)
// If the update stamp flag is true then it will also update the time stamps stored into the resource manager
// If the include dependencies is set to true, if an assets has changed and another asset depends on it, it will also add the latter to the
// outdated list of assets
Stream<Stream<unsigned int>> GetOutOfDateAssets(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp = true, bool include_dependencies = true);

// Fills in the handles of the assets for which the metadata file has changed since last time
// If the update stamp flag is true then it will also update the time stamps stored into the resource manager
// If the include dependencies is set to true, if an assets has changed and another asset depends on it, it will also add the latter to the
// outdated list of assets
Stream<Stream<unsigned int>> GetOutOfDateAssetsMetadata(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp = true, bool include_dependencies = true);

// Fills in the handles of the assets that have their target changed
// If the update stamp flag is true then it will also update the time stamps stored into the resource manager
// If the include dependencies is set to true, if an assets has changed and another asset depends on it, it will also add the latter to the
// outdated list of assets
Stream<Stream<unsigned int>> GetOutOfDateAssetsTargetFile(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp = true, bool include_dependencies = true);

Stream<Stream<unsigned int>> GetDependentAssetsFor(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator, bool include_itself = false);

// For meshes, textures, shaders and misc assets it will get the time stamp for both the metadata and the 
// actual target file and return the maximum of both. It returns the time stamp stored in the resource manager
size_t GetAssetRuntimeTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// It returns the time stamp of the target file (if it has one) and that of the metadata from the OS,
// not the internally stored one
size_t GetAssetExternalTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// It returns the time stamp of the target file (if it has one) and that of the metadata from the OS,
// not the internally stored one
size_t GetAssetExternalTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> assets_folder);

// It returns the time stamp of the metadata file that corresponds to the asset
size_t GetAssetMetadataTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// Returns the time stamp of the target asset (like for meshes or textures)
size_t GetAssetTargetFileTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// Returns the time stamp of the target asset (like for meshes or textures)
size_t GetAssetTargetFileTimeStamp(const void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> assets_folder);

unsigned int GetAssetReferenceCount(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// Returns an array with all the assets files (those like .mat, .vshader, .pshader or .sampler) found in the assets folder.
// These paths are relative paths to the assets folder. To deallocate them, you need to deallocate each string and then
// Then buffer itself
Stream<Stream<wchar_t>> GetAssetsFromAssetsFolder(const EditorState* editor_state, AllocatorPolymorphic allocator);

// Using the stack_allocator, it will return back the handles which are not already being loaded in background
// In the boolean pointer it will set the value to true if there are entries, else false
Stream<Stream<unsigned int>> GetNotLoadedAssets(
	const EditorState* editor_state, 
	AllocatorPolymorphic stack_allocator, 
	Stream<Stream<unsigned int>> handles, 
	bool* has_entries
);

bool HasAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

bool HasAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

ECS_INLINE bool IsThunkOrForwardingFile(ECS_ASSET_TYPE type) {
	return type == ECS_ASSET_GPU_SAMPLER || type == ECS_ASSET_SHADER || type == ECS_ASSET_MATERIAL || type == ECS_ASSET_MISC;
}

// Inserts a time stamp into the resource manager. If the is_missing flag is set to true it will only
// insert the time stamp if it doesn't exist
void InsertAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool if_is_missing = false);

// Inserts a time stamp into the resource manager. If the is_missing flag is set to true it will only
// insert the time stamp if it doesn't exist
void InsertAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, bool if_is_missing = false);

// Returns true if the asset is being loaded in the background, else false
bool IsAssetBeingLoaded(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

enum LOAD_EDITOR_ASSETS_STATE : unsigned char {
	LOAD_EDITOR_ASSETS_SUCCESS,
	LOAD_EDITOR_ASSETS_FAILURE,
	LOAD_EDITOR_ASSETS_PENDING
};


struct LoadEditorAssetsOptionalData {
	// If specified, it will update the link components
	// Only from this entity manager. Else it will update
	// The link components in the editor (sandboxes and/or
	// system settings)
	EntityManager* update_link_entity_manager = nullptr;
	EditorEventFunction callback = nullptr;
	void* callback_data = nullptr;
	size_t callback_data_size = 0;
	std::atomic<LOAD_EDITOR_ASSETS_STATE>* state = nullptr;
	unsigned int sandbox_index = -1;
};

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// editor start sandboxes. It will copy the current asset handles before forwarding to the event.
// After the load is finalized (with success or not), it will call the callback (if there is one)
void LoadEditorAssets(
	EditorState* editor_state,
	Stream<Stream<unsigned int>> handles,
	LoadEditorAssetsOptionalData* optional_data
);

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// editor start sandboxes. It will copy the current asset handles before forwarding to the event.
// After the load is finalized (with success or not), it will call the callback (if there is one specified,
// It can be nullptr if you don't need one)
void LoadEditorAssets(
	EditorState* editor_state,
	CapacityStream<unsigned int>* handles,
	LoadEditorAssetsOptionalData* optional_data
);

void LoadAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles);

// The file is ignored for materials and samplers
// It writes the handle of the asset. If the deserialization failed, it will write -1
// The reason for which it takes a pointer to the handle is that this may not run when called
// due to a prevention of resource loads. It returns true if the resource already exists and no asynchronous load is done
// Can optionally give a callback to call when the load is finalized. A structure of type CreateAssetAsyncCallbackInfo
// will be given in the additional_data member
bool RegisterGlobalAsset(
	EditorState* editor_state, 
	Stream<char> name,
	Stream<wchar_t> file,
	ECS_ASSET_TYPE type,
	unsigned int* handle,
	bool unload_if_existing = false,
	UIActionHandler callback = {}
);

// It will push an event to remove the handles from the loading assets array
void RegisterRemoveLoadingAssetsEvent(EditorState* editor_state, Stream<AssetTypedHandle> handles);

// It will push an event to remove the handles from the loading assets array
void RegisterRemoveLoadingAssetsEvent(EditorState* editor_state, Stream<Stream<unsigned int>> handles);

void RemoveAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

void RemoveAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// Deallocates the asset and removes the time stamp from the resource manager. Returns true if the deallocation
// was successful, else false.
bool RemoveAsset(EditorState* editor_state, void* metadata, ECS_ASSET_TYPE type);

// Deallocates the asset and removes the time stamp from the resource manager, it will also eliminate it from the
// asset database. If the recurse flag is set to true, then it will recursively called for each dependency
// that is maintained alive only by this asset
bool RemoveAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool recurse = true);

// Removes the dependencies and also the time stamps
void RemoveAssetDependencies(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

void RemoveLoadingAsset(EditorState* editor_state, AssetTypedHandle handle);

void RemoveLoadingAssets(EditorState* editor_state, Stream<Stream<unsigned int>> handles);

void RemoveLoadingAssets(EditorState* editor_state, Stream<AssetTypedHandle> handles);

// This unregister is for assets that are not tied down to a sandbox.
// Useful for example for inspectors. It will wait until the flag EDITOR_STATE_PREVENT_RESOURCE_LOADING is cleared
void UnregisterGlobalAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback = {});

// This unregister is for assets that are not tied down to a sandbox.
// Useful for example for inspectors. It will wait until the flag EDITOR_STATE_PREVENT_RESOURCE_LOADING is cleared
void UnregisterGlobalAsset(EditorState* editor_state, Stream<AssetTypedHandle> elements, UIActionHandler callback = {});

// This unregister is for assets that are not tied down to a sandbox.
// Useful for example for inspectors. It will wait until the flag EDITOR_STATE_PREVENT_RESOURCE_LOADING is cleared
void UnregisterGlobalAsset(EditorState* editor_state, Stream<Stream<unsigned int>> elements, UIActionHandler callback = {});