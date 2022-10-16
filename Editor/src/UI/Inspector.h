#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
ECS_TOOLS;

#define INSPECTOR_WINDOW_NAME "Inspector "
#define MAX_INSPECTOR_WINDOWS 8

struct EditorState;

enum EDITOR_MODULE_LOAD_STATUS : unsigned char;

// In the stack memory the first 4 bytes should be the inspector index
void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void InspectorWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

// Only creates the UI window, not the dockspace. Returns the window index
unsigned int CreateInspectorWindow(EditorState* editor_state, unsigned int inspector_index);

// Creates the dockspace and the window. Returns the window index
unsigned int CreateInspectorDockspace(EditorState* editor_state, unsigned int inspector_index);

void CreateInspectorAction(ActionData* action_data);

// Inspector index means default behaviour - first round robin inspector
void ChangeInspectorToNothing(EditorState* editor_state, unsigned int inspector_index = -1);

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index = -1);

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index, unsigned int inspector_index = -1);

// If inspector index is different from -1, it will change that inspector into the settings for the bound sandbox
// If sandbox index is different from -1, it will find an inspector suitable or create one if it doesn't exist
void ChangeInspectorToSandboxSettings(EditorState* editor_state, unsigned int inspector_index = -1, unsigned int sandbox_index = -1);

void ChangeInspectorToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int inspector_index = -1);

// Returns the index of the sandbox that is being referenced by the inspector
unsigned int GetInspectorTargetSandbox(const EditorState* editor_state, unsigned int inspector_index);

bool IsInspectorLocked(const EditorState* editor_state, unsigned int inspector_index);

// If the index is already known, can be used to directly index into the array
void LockInspector(EditorState* editor_state, unsigned int inspector_index);

// If the index is already known, can be used to directly index into the array
void UnlockInspector(EditorState* editor_state, unsigned int inspector_index);