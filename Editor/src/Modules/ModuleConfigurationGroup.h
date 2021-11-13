#pragma once
#include "ModuleDefinition.h"

// The configuration at libraries.size is the fallback configuration
struct ModuleConfigurationGroup {
	ECSEngine::containers::Stream<char> name;
	ECSEngine::containers::Stream<ECSEngine::containers::Stream<wchar_t>> libraries;
	EditorModuleConfiguration* configurations;
};

struct EditorState;

void AddModuleConfigurationGroup(
	EditorState* editor_state, 
	ECSEngine::containers::Stream<char> name,
	ECSEngine::containers::Stream<unsigned int> library_masks, 
	EditorModuleConfiguration* configurations
);

// If a group was created externally, i.e. from loading a file, it can be added directly
void AddModuleConfigurationGroup(EditorState* editor_state, ModuleConfigurationGroup group);

ModuleConfigurationGroup CreateModuleConfigurationGroup(
	EditorState* editor_state,
	ECSEngine::containers::Stream<char> name,
	ECSEngine::containers::Stream<unsigned int> library_masks,
	EditorModuleConfiguration* configurations
);

void DeallocateModuleConfigurationGroup(EditorState* editor_state, unsigned int index);

void GetModuleConfigurationGroupFilePath(EditorState* editor_state, ECSEngine::containers::CapacityStream<wchar_t>& path);

unsigned int GetModuleConfigurationGroupIndex(EditorState* editor_state, ECSEngine::containers::Stream<char> name);

void RemoveModuleConfigurationGroup(EditorState* editor_state, unsigned int index);

void RemoveModuleConfigurationGroup(EditorState* editor_state, ECSEngine::containers::Stream<char> name);

// Returns whether or not it succeded in loading the file
bool LoadModuleConfigurationGroupFile(EditorState* editor_state);

// Returns whether or not it succeded in saving the file
bool SaveModuleConfigurationGroupFile(EditorState* editor_state);

// Checks to see if all configurations are valid, i.e. are in bounds [0, 2], if not, they are changed to debug,
// And if the libraries exist, if some do not, they will simply be removed, 
void ValidateModuleConfigurationGroups(EditorState* editor_state, ECSEngine::containers::Stream<ModuleConfigurationGroup> groups);