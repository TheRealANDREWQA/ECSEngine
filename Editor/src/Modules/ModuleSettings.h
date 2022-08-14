#pragma once
#include "editorpch.h"
#include "ModuleDefinition.h"

struct EditorState;

#define MODULE_SETTINGS_EXTENSION L".config"

// Allocates the memory needed for the reflected settings. It does not allocate the buffers
// No UI reflection types are created. If you want to deallocate the settings, then free the
// allocator. It is not worth keeping track of each and every allocation made
void AllocateModuleSettings(
	const EditorState* editor_state,
	unsigned int module_index,
	ECSEngine::CapacityStream<EditorModuleReflectedSetting>& settings,
	ECSEngine::AllocatorPolymorphic allocator
);

// Only creates the UIReflection instances in order to be displayed and drawn
// The settings will be filled with the name and the data will be allocated from the allocator
// The id is needed to be appended to the instance names in order to make them unique
// All the necessary allocations will be made from the given allocator
void CreateModuleSettings(
	const EditorState* editor_state,
	unsigned int module_index, 
	ECSEngine::CapacityStream<EditorModuleReflectedSetting>& settings, 
	ECSEngine::AllocatorPolymorphic allocator,
	unsigned int settings_id
);

// It will create the folder where the settings for this module will be kept
bool CreateModuleSettingsFolder(const EditorState* editor_state, unsigned int module_index);

// It will create a settings file for that module using the currently loaded reflection types
bool CreateModuleSettingsFile(const EditorState* editor_state, unsigned int module_index, ECSEngine::Stream<wchar_t> name);

// Only removes the UIReflection instances in order to free the memory of those systems
// The id is needed in order to determine the correct instances
// In order to free the associated settings memory, just destroy the allocator 
// from which these allocations where made from
void DestroyModuleSettings(
	const EditorState* editor_state,
	unsigned int module_index, 
	ECSEngine::Stream<EditorModuleReflectedSetting> settings,
	unsigned int settings_id
);

// Filename should be only the name e.g. Example. The extensions will be added automatically
// It will place the absolute path in the full path
void GetModuleSettingsFilePath(
	const EditorState* editor_state,
	unsigned int module_index,
	ECSEngine::Stream<wchar_t> filename,
	ECSEngine::CapacityStream<wchar_t>& full_path
);

void GetModuleSettingsFolderPath(
	const EditorState* editor_state,
	unsigned int module_index,
	ECSEngine::CapacityStream<wchar_t>& path
);

// It will eliminate all the decorations and the absolute parts and copy only the
// part that would be assigned to an actual module. i.e. from 
// C:\Users\MyName\Project\Configuration\Modules\MyModule\Default.config -> Default (Only the stem)
void GetModuleAvailableSettings(
	const EditorState* editor_state,
	unsigned int module_index,
	ECSEngine::CapacityStream<ECSEngine::Stream<wchar_t>>& paths,
	ECSEngine::AllocatorPolymorphic allocator
);

void GetModuleSettingsUITypesIndices(
	const EditorState* editor_state,
	unsigned int module_index,
	ECSEngine::CapacityStream<unsigned int>& indices
);

void GetModuleSettingsUIInstancesIndices(
	const EditorState* editor_state,
	unsigned int module_index,
	unsigned int settings_id,
	ECSEngine::CapacityStream<unsigned int>& indices
);

// Loads the settings for a certain module. It checks that the data is valid
// All the necessary allocations will be made from the given allocator
bool LoadModuleSettings(
	const EditorState* editor_state, 
	unsigned int module_index, 
	ECSEngine::Stream<wchar_t> path,
	ECSEngine::Stream<EditorModuleReflectedSetting> settings,
	ECSEngine::AllocatorPolymorphic allocator
);

void RemoveModuleSettingsFileAndFolder(const EditorState* editor_state, unsigned int module_index);

// Saves the settings for a certain module.
bool SaveModuleSettings(
	const EditorState* editor_state, 
	unsigned int module_index,
	ECSEngine::Stream<wchar_t> path,
	ECSEngine::Stream<EditorModuleReflectedSetting> settings
);

// If a field does not have default values, it will be made 0, except for the streams
// which will only be reset to size 0
void SetModuleDefaultSettings(
	const EditorState* editor_state, 
	unsigned int module_index,
	ECSEngine::Stream<EditorModuleReflectedSetting> settings
);