#pragma once
#include "ECSEngineUI.h"

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
void FileExplorerSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory);

void FileExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

void CreateFileExplorer(EditorState* editor_state);

unsigned int CreateFileExplorerWindow(EditorState* editor_state);

void CreateFileExplorerAction(ECSEngine::Tools::ActionData* action_data);

void TickFileExplorer(EditorState* editor_state);