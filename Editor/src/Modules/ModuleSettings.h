#pragma once
#include "editorpch.h"

struct EditorState;

#define MODULE_DEFAULT_SETTINGS_PATH L"Default.config"

// Only creates the UIReflection types and instances in order to be displayed and drawn
void CreateModuleSettings(EditorState* editor_state, unsigned int module_index);

// Only removes the UIReflection types and instances in order to free the memory of those systems
void DestroyModuleSettings(EditorState* editor_state, unsigned int module_index);

// Loads the settings for a certain module. It checks that the data is valid
bool LoadModuleSettings(EditorState* editor_state, unsigned int module_index);

// Saves the settings for a certain module.
bool SaveModuleSettings(EditorState* editor_state, unsigned int module_index);

// If a field does not have default values, it will be made 0, except for the streams
// which will only be reset to size 0
void SetModuleDefaultSettings(EditorState* editor_state, unsigned int module_index);

// Only changes the path - does not affect the stored settings
// New path should only be the filename, not the absolute path
void ChangeModuleSettings(EditorState* editor_state, ECSEngine::Stream<wchar_t> new_path, unsigned int module_index);

// Destroys the current settings, changes the path, creates new settings
// New path should only be the filename, not the absolute path
void SwitchModuleSettings(EditorState* editor_state, ECSEngine::Stream<wchar_t> new_path, unsigned int module_index);

void GetModuleSettingsFilePath(const EditorState* editor_state, unsigned int module_index, ECSEngine::CapacityStream<wchar_t>& path);