#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
ECS_TOOLS;

#define MODULE_EXPLORER_WINDOW_NAME "Module Explorer"

struct EditorState;

void ModuleExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory);

void ModuleExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateModuleExplorer(EditorState* editor_state);

// It creates the dockspace and the window
void CreateModuleExplorerAction(ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateModuleExplorerWindow(EditorState* editor_state);