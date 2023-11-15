#pragma once
#include "ECSEngineUI.h"

#define SANDBOX_EXPLORER_WINDOW_NAME "Sandbox Explorer"

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
void SandboxExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void SandboxExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateSandboxExplorer(EditorState* editor_state);

// It creates the dockspace and the window
// It needs in the data field the editor_state
void CreateSandboxExplorerAction(ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateSandboxExplorerWindow(EditorState* editor_state);