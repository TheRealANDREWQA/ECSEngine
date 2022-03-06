#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
;

constexpr const char* MODULE_EXPLORER_WINDOW_NAME = "Module Explorer";

struct EditorState;

void ModuleExplorerSetDescriptor(Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void ModuleExplorerDraw(void* window_data, void* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateModuleExplorer(EditorState* editor_state);

// It creates the dockspace and the window
void CreateModuleExplorerAction(Tools::ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateModuleExplorerWindow(EditorState* editor_state);

void ModuleExplorerTick(EditorState* editor_state);