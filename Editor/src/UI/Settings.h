#pragma once
#include "ECSEngineUI.h"

#define SETTINGS_WINDOW_NAME "Settings"

struct EditorState;

void SettingsWindowSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void SettingsDraw(void* window_data, void* drawer_descriptor, bool initialize);

// Creates the dockspace and the window if it doesn't exist, else it will set it as the active window
void CreateSettingsWindow(EditorState* editor_state);

void CreateSettingsWindowAction(ECSEngine::Tools::ActionData* action_data);