#pragma once
#include "editorpch.h"

struct EditorState;

#define DEFAULT_SETTINGS_FILE_NAME L"Settings.config"

// It can be used to create multiple files, for example if multiple configurations are to be recorded
// The filename will be concatenated with the Configuration folder path
bool SaveWorldParametersFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> filename, const ECSEngine::WorldDescriptor* descriptor);

// If the load fails, the world descriptor thread count will contain 0
// The filename will be concatenated with the Configuration folder path
ECSEngine::WorldDescriptor LoadWorldParametersFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> filename, bool print_detailed_error_message = false);

// Concatenates the Configuration folder with the default name for the configuration
void GetDefaultSettingFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

ECSEngine::WorldDescriptor GetDefaultSettings(const EditorState* editor_state);

// Returns whether or not it succeded. It prints and error message if it couldn't create the file
bool SaveDefaultSettingsFile(EditorState* editor_state);

// Returns whether or not the project settings exist
bool ExistsProjectSettings(const EditorState* editor_state);

// If the configuration path doesn't exist, it will attempt to create a default one. If even that one fails, it will set the configuration
// path undefined but it will set the project descriptor to the default one.
bool CheckProjectSettingsValidity(EditorState* editor_state);

void SetSettingsPath(EditorState* editor_state, ECSEngine::Stream<wchar_t> name);