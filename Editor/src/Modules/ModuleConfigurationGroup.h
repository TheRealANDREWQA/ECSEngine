#pragma once
#include "ModuleDefinition.h"

// The configuration at libraries.size is the fallback configuration
struct ModuleConfigurationGroup {
	ECSEngine::Stream<char> name;
	ECSEngine::Stream<ECSEngine::Stream<wchar_t>> libraries;
	EditorModuleConfiguration* configurations;
};

struct EditorState;

void AddModuleConfigurationGroup(
	EditorState* editor_state, 
	ECSEngine::Stream<char> name,
	ECSEngine::Stream<unsigned int> library_masks, 
	EditorModuleConfiguration* configurations
);

// If a group was created externally, i.e. from loading a file, it can be added directly
void AddModuleConfigurationGroup(EditorState* editor_state, ModuleConfigurationGroup group);

void ApplyModuleConfigurationGroup(EditorState* editor_state, unsigned int group_index);

ModuleConfigurationGroup CreateModuleConfigurationGroup(
	EditorState* editor_state,
	ECSEngine::Stream<char> name,
	ECSEngine::Stream<unsigned int> library_masks,
	EditorModuleConfiguration* configurations
);

ModuleConfigurationGroup CreateModuleConfigurationGroup(
	EditorState* editor_state,
	ECSEngine::Stream<char> name,
	ECSEngine::Stream<ECSEngine::Stream<wchar_t>> libraries,
	EditorModuleConfiguration* configurations
);

void DeallocateModuleConfigurationGroup(EditorState* editor_state, unsigned int index);

void DeleteModuleConfigurationGroupFile(EditorState* editor_state);

void GetProjectModuleConfigurationGroupFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

unsigned int GetModuleConfigurationGroupIndex(const EditorState* editor_state, ECSEngine::Stream<char> name);

void RemoveModuleConfigurationGroup(EditorState* editor_state, unsigned int index);

void RemoveModuleConfigurationGroup(EditorState* editor_state, ECSEngine::Stream<char> name);

void ResetModuleConfigurationGroups(EditorState* editor_state);

// Returns whether or not it succeded in loading the file
bool LoadModuleConfigurationGroupFile(EditorState* editor_state);

// Returns whether or not it succeded in saving the file
bool SaveModuleConfigurationGroupFile(EditorState* editor_state);

// Checks to see if all configurations are valid, i.e. are in bounds [0, 2], if not, they are changed to debug,
// And if the libraries exist, if some do not, they will simply be removed, 
void ValidateModuleConfigurationGroups(EditorState* editor_state, ECSEngine::Stream<ModuleConfigurationGroup> groups);

void UpdateModuleConfigurationGroup(EditorState* editor_state, unsigned int index, ModuleConfigurationGroup group);

// Group index -1 means create a new group instead of editing an existing one
unsigned int CreateModuleConfigurationGroupAddWizard(EditorState* editor_state, unsigned int group_index);