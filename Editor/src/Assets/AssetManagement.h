#pragma once
#include "ECSEngineAssets.h"

using namespace ECSEngine;

struct EditorState;

// The path does not need to have the extension. It will be appended automatically
// This is a thunk file (it does not contain anything)
bool CreateMaterialFile(const EditorState* editor_state, Stream<wchar_t> relative_path);

// The path does not need to have the extension. It will be appended automatically
// This is a thunk file (it does not contain anything)
bool CreateSamplerFile(const EditorState* editor_state, Stream<wchar_t> relative_path);

// The path does not need to have the extension. It will be appended automatically
// This is a forwarding file (it contains the path to the target)
bool CreateShaderFile(const EditorState* editor_state, Stream<wchar_t> relative_path);

// The path does not need to have the extension. It will be appended automatically
// This is a forwarding file (it contains the path to the target)
bool CreateMiscFile(const EditorState* editor_state, Stream<wchar_t> relative_path);

// This only creates the disk file for the new setting. It does not add it to the database
// Returns true if it succeeded else false
bool CreateAssetSetting(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// Returns true if it succeeded, else false.
// This doesn't check the prevent resource load flag and wait until is cleared
bool CreateAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// If an asset like a mesh or a texture doesn't have a default setting, it creates one
void CreateAssetDefaultSetting(const EditorState* editor_state);

// When the target file of an asset is changed this needs to be called
// This may run delayed because of the prevent resource load flag
// The new asset pointer needs to be stable. It will set the status to 1 if it completed successfully, 0 if it failed.
// It will change the full name internally
void ChangeAssetFile(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, const void* new_asset, unsigned char* status);

// When the name of an asset is changed this needs to be called
// It will create the full name internally
void ChangeAssetName(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, const void* new_asset);

void ChangeAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, size_t new_stamp);

void ChangeAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, size_t new_stamp);

// Returns true if it succeeded, else false. It can fail if the resource doesn't exist
// This doesn't check the prevent resource load flag and wait until is cleared
bool DeallocateAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// Returns true if it succeeded, else false. It can fail if the resource doesn't exist
// This doesn't check the prevent resource load flag and wait until is cleared
bool DeallocateAsset(EditorState* editor_state, void* metadata, ECS_ASSET_TYPE type);

// It may not run immediately since it may be prevented from loading resources
void DeleteAssetSetting(EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// Deletes all asset metadata files which have their asset missing
void DeleteMissingAssetSettings(const EditorState* editor_state);

// Returns true if the asset already exists or not. For materials and samplers the file parameter is ignored
bool ExistsAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// Returns the handle of the asset. Returns -1 if it doesn't exist
unsigned int FindAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

// Returns the handle to that asset. If it doesn't exist, it will add it without creating the runtime asset
// Can optionally specify whether or not to update the reference count (when it already exists)
unsigned int FindOrAddAsset(EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool increment_reference_count = false);

// Fills in the relative path to the asset from the name (materials, gpu samplers, shaders and misc are available at the moment)
void FromAssetNameToThunkOrForwardingFile(Stream<char> name, Stream<wchar_t> extension, CapacityStream<wchar_t>& relative_path);

// Fills in the relative path to the asset from the name (materials, gpu samplers, shaders and misc are available at the moment)
void FromAssetNameToThunkOrForwardingFile(Stream<char> name, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& relative_path);

// Thunk files are those for materials and gpu samplers and forwarding files are shaders and misc
void GetAssetNameFromThunkOrForwardingFile(const EditorState* editor_state, Stream<wchar_t> absolute_path, CapacityStream<char>& name);

// Thunk files are those for materials and gpu samplers and forwarding files are shaders and misc
void GetAssetNameFromThunkOrForwardingFileRelative(const EditorState* editor_state, Stream<wchar_t> relative_path, CapacityStream<char>& name);

// Returns true if it managed to read the file. A target path of size 0 but with a return of true it means that the target is not yet assigned
bool GetAssetFileFromForwardingFile(Stream<wchar_t> absolute_path, CapacityStream<wchar_t>& target_path);

// This is used only for meshes and textures (these are a special case. They are not thunk files nor forwarding files)
Stream<Stream<char>> GetAssetCorrespondingMetadata(const EditorState* editor_state, Stream<wchar_t> file, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator);

// Fills in the handles of the assets that are out of date (have been changed from outside, like a mesh or a texture, ECS files as well)
// If the update stamp flag is true then it will also update the time stamps stored into the resource manager
Stream<Stream<unsigned int>> GetOutOfDateAssets(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp = true);

Stream<Stream<unsigned int>> GetDependentAssetsFor(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator);

// For meshes, textures, shaders and misc assets it will get the time stamp for both the metadata and the 
// actual target file and return the maximum of both. It returns the time stamp stored in the resource manager
size_t GetAssetRuntimeTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// It returns the time stamp of the target file (if it has one) and that of the metadata from the OS,
// not the internally stored one
size_t GetAssetExternalTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// Inserts a time stamp into the resource manager
void InsertAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// Inserts a time stamp into the resource manager
void InsertAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

void RemoveAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

void RemoveAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type);

// Deallocates the asset and removes the time stamp from the resource manager. Returns true if the deallocation
// was successful, else false
bool RemoveAsset(EditorState* editor_state, void* metadata, ECS_ASSET_TYPE type);

// Deallocates the asset and removes the time stamp from the resource manager, it will also eliminate it from the
// asset database
bool RemoveAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type);

// Returns true if it managed to write
bool WriteForwardingFile(Stream<wchar_t> absolute_path, Stream<wchar_t> target_path);
