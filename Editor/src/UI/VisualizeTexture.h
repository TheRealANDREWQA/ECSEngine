#pragma once
#include "ECSEngineUI.h"

struct EditorState;

#define VISUALIZE_TEXTURE_WINDOW_NAME "Visualize Texture"

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the sandbox index
void VisualizeTextureUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

// It creates the dockspace and the window
// It returns the index of the visualize index window. Returns -1 if the limit for visualize windows was reached
unsigned int CreateVisualizeTextureUIWindow(EditorState* editor_state);

// It creates the dockspace and the window
// It must take as parameter the EditorState*
void CreateVisualizeTextureUIWindowAction(ActionData* action_data);

void GetVisualizeTextureUIWindowName(unsigned int index, ECSEngine::CapacityStream<char>& name);

// Returns the max index of the visualize texture
// Returns -1 if no windows was created
unsigned int GetMaxVisualizeTextureUIIndex(const EditorState* editor_state);