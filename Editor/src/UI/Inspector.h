#pragma once
#include "ECSEngineUI.h"
#include "ECSEngineAssets.h"

/*
	Each inspector window that is implemented should perform the initialization
	Inside the window draw since when using the inspector targets the specific
	Initialization phase is not caught in that case
*/

using namespace ECSEngine;
ECS_TOOLS;

#define INSPECTOR_WINDOW_NAME "Inspector "

struct EditorState;

enum EDITOR_MODULE_LOAD_STATUS : unsigned char;

// In the stack memory the first 4 bytes should be the inspector index
void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void InspectorWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

void InspectorDrawEntity(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer);

// Only creates the UI window, not the dockspace. Returns the window index
unsigned int CreateInspectorWindow(EditorState* editor_state, unsigned int inspector_index);

// Creates the dockspace and the window. Returns the window index
unsigned int CreateInspectorDockspace(EditorState* editor_state, unsigned int inspector_index);

void CreateInspectorAction(ActionData* action_data);

// Inspector index means default behaviour - first round robin inspector
void ChangeInspectorToNothing(EditorState* editor_state, unsigned int inspector_index = -1, bool do_not_push_target_entry = false);

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index = -1);

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index, unsigned int inspector_index = -1, Stream<wchar_t> initial_settings = {});

// If inspector index is different from -1, it will change that inspector into the settings for the bound sandbox
// If sandbox index is different from -1, it will find an inspector suitable or create one if it doesn't exist
void ChangeInspectorToSandboxSettings(EditorState* editor_state, unsigned int inspector_index = -1, unsigned int sandbox_index = -1);

void ChangeInspectorToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int inspector_index = -1);

void ChangeInspectorToGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, unsigned int inspector_index = -1);

void ChangeInspectorMatchingSandbox(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index);

void ChangeInspectorToAsset(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE asset_type, unsigned int inspector_index = -1);

void ChangeInspectorToAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE asset_type, unsigned int inspector_index = -1);

// The initial setting is relevant only for meshes and textures
void ChangeInspectorToAsset(EditorState* editor_state, Stream<wchar_t> path, Stream<char> initial_setting, ECS_ASSET_TYPE asset_type, unsigned int inspector_index = -1);

// Returns true if the inspector at that index exists, else false
bool ExistsInspector(const EditorState* editor_state, unsigned int inspector_index);

unsigned int GetInspectorMatchingSandbox(const EditorState* editor_state, unsigned int inspector_index);

// Returns the index of the sandbox that is being referenced by the inspector
unsigned int GetInspectorTargetSandbox(const EditorState* editor_state, unsigned int inspector_index);

bool IsInspectorLocked(const EditorState* editor_state, unsigned int inspector_index);

// Returns true if the inspector display information about an entity.
// If include_global_components is set to true, then it will include global components as well
bool IsInspectorDrawEntity(const EditorState* editor_state, unsigned int inspector_index, bool include_global_components = false);

// If the index is already known, can be used to directly index into the array
void LockInspector(EditorState* editor_state, unsigned int inspector_index);

// If the index is already known, can be used to directly index into the array
void UnlockInspector(EditorState* editor_state, unsigned int inspector_index);

// It will change the inspector to nothing and back to the current asset such that a reload
// Will happen from the base metadata
void ReloadInspectorAssetFromMetadata(EditorState* editor_state, unsigned int inspector_index);

void SetInspectorRetainedMode(EditorState* editor_state, unsigned int inspector_index, WindowRetainedMode retained_mode);