#pragma once
#include "ECSEngineUI.h"

constexpr const char* DIRECTORY_EXPLORER_WINDOW_NAME = "Directory Explorer";

struct EditorState;

// Stack memory size should be at least 512
void DirectoryExplorerSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void DirectoryExplorerDraw(void* window_data, void* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateDirectoryExplorer(EditorState* editor_state);

// It creates the dockspace and the window
void CreateDirectoryExplorerAction(ECSEngine::Tools::ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateDirectoryExplorerWindow(EditorState* editor_state);

void DirectoryExplorerTick(EditorState* editor_state);