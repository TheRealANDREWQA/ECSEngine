#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineAssets.h"

struct EditorState;

using namespace ECSEngine;

// -------------------------------------------------------------------------------------------------------------

// It fills in assets which are missing. There must be ECS_ASSET_TYPE_COUNT missing assets streams, one for each type
void GetSandboxMissingAssets(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets);

// -------------------------------------------------------------------------------------------------------------

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// runtime start new sandboxes
void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets);

// -------------------------------------------------------------------------------------------------------------

// It will add an EditorEvent such that it will monitor the status of the load. When it has finished it will let the
// runtime start new sandboxes
void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// This should be called when the asset is used in a sandbox. The file is ignored for materials and samplers
// It writes the handle of the asset. If the deserialization failed, it can return -1
// The reason for which it takes a pointer to the handle is that this may not run when called
// due to a prevention of resource loads. It returns true if the resource already exists and no asynchronous load is done
bool RegisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, unsigned int* handle);

// -------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECS_ASSET_TYPE type);

// -------------------------------------------------------------------------------------------------------------

void UnloadSandboxLinkComponent(EditorState* editor_state, unsigned int sandbox_index, const void* link_component, Stream<char> component_name);

// -------------------------------------------------------------------------------------------------------------