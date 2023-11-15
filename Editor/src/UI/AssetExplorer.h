#pragma once
#include "ECSEngineUI.h"

#define ASSET_EXPLORER_WINDOW_NAME "Asset Explorer"

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
void AssetExplorerSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void AssetExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateAssetExplorer(EditorState* editor_state);

// It creates the dockspace and the window
// Must have the EditorState* as data pointer
void CreateAssetExplorerAction(ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateAssetExplorerWindow(EditorState* editor_state);