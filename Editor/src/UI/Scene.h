#pragma once
#include "ECSEngineUI.h"

#define SCENE_WINDOW_NAME "Scene "
#define MAX_SCENE_WINDOWS 8u

struct EditorState;

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the sandbox index
void SceneUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

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

// Does nothing if the old_index doesn't exist
void UpdateSceneUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index);

// Returns the target sandbox index from that window_index. If the window is not a scene UI window,
// it will return -1
unsigned int SceneUITargetSandbox(const EditorState* editor_state, unsigned int window_index);

void HandleTransformTool(EditorState* editor_state, unsigned int sandbox_index);