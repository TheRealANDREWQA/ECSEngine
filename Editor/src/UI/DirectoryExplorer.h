#pragma once
#include "ECSEngineUI.h"

#define DIRECTORY_EXPLORER_WINDOW_NAME "Directory Explorer"

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
void DirectoryExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory);

void DirectoryExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateDirectoryExplorer(EditorState* editor_state);

// It creates the dockspace and the window
void CreateDirectoryExplorerAction(ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateDirectoryExplorerWindow(EditorState* editor_state);

void TickDirectoryExplorer(EditorState* editor_state);