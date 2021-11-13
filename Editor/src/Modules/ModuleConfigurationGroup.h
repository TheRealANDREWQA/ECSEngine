#pragma once
#include "ModuleDefinition.h"

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

void LoadModuleConfigurationGroupFile(EditorState* editor_state);

void SaveModuleConfigurationGroupFile(EditorState* editor_state);