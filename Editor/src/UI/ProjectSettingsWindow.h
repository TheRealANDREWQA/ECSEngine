#pragma once
#include "ECSEngineUI.h"

struct EditorState;

#define PROJECT_SETTINGS_WINDOW_NAME "Project Settings"

void ProjectSettingsWindowSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void ProjectSettingsDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// Creates the dockspace and the window if it doesn't exist, else it will set it as the active window
void CreateProjectSettingsWindow(EditorState* editor_state);

void CreateProjectSettingsWindowAction(ECSEngine::Tools::ActionData* action_data);