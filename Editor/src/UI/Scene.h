#pragma once
#include "ECSEngineUI.h"

#define SCENE_WINDOW_NAME "Scene "
#define MAX_SCENE_WINDOWS 8u

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the sandbox index
void SceneUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory);

void SceneUIWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateSceneUIWindow(EditorState* editor_state, unsigned int index);

struct CreateSceneUIWindowActionData {
	EditorState* editor_state;
	unsigned int index;
};

// It creates the dockspace and the window
// It must have as data a CreateSceneUIWindowActionData*
void CreateSceneUIWindowAction(ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateSceneUIWindowOnly(EditorState* editor_state, unsigned int index);

// Destroys all scenes whose sandbox no longer exists
void DestroyInvalidSceneUIWindows(EditorState* editor_state);

void GetSceneUIWindowName(unsigned int index, ECSEngine::CapacityStream<char>& name);

// Returns the UI index of the scene window (-1 if it doesn't exist)
unsigned int GetSceneUIWindowIndex(const EditorState* editor_state, unsigned int sandbox_index);

// If the window is present, it will disable the UI rendering
// Returns true if the window is present, else false
bool DisableSceneUIRendering(EditorState* editor_state, unsigned int sandbox_index);

// If the window is present, it will enable the UI rendering
// Returns true if the window is present, else false. You can choose between having the
// Window be actual visible to the user, or just that it exists
bool EnableSceneUIRendering(EditorState* editor_state, unsigned int sandbox_index, bool must_be_visible);

void FocusSceneUIOnSelection(EditorState* editor_state, unsigned int sandbox_index);

// Does nothing if the old_index doesn't exist
void UpdateSceneUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index);

// Returns the target sandbox index from that window_index. If the window is not a scene UI window,
// it will return -1
unsigned int SceneUITargetSandbox(const EditorState* editor_state, unsigned int window_index);