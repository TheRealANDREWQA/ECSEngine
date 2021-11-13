#include "ModuleConfigurationGroup.h"
#include "../Editor/EditorState.h"
#include "Module.h"
#include "../Project/ProjectOperations.h"
#include "../Editor/EditorEvent.h"

constexpr const wchar_t* MODULE_CONFIGURATION_GROUP_FILE_EXTENSION = L".ecsmodulegroup";
constexpr const char* MODULE_CONFIGURATION_GROUP_FILE_VERSION = 0;

using namespace ECSEngine;
ECS_CONTAINERS;

// ---------------------------------------------------------------------------------------------------------------------------

void AddModuleConfigurationGroup(
	EditorState* editor_state,
	Stream<char> name,
	Stream<unsigned int> library_masks,
	EditorModuleConfiguration* configurations
) {
	editor_state->module_configuration_groups.Add(CreateModuleConfigurationGroup(editor_state, name, library_masks, configurations));
}

// ---------------------------------------------------------------------------------------------------------------------------

ModuleConfigurationGroup CreateModuleConfigurationGroup(
	EditorState* editor_state,
	Stream<char> name,
	Stream<unsigned int> library_masks,
	EditorModuleConfiguration* configurations
) {
	ModuleConfigurationGroup group;

	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;

	// Coallesced allocation
	// Null terminated string for the name
	size_t total_memory = sizeof(char) * (name.size + 1);
	for (size_t index = 0; index < library_masks.size; index++) {
		total_memory += sizeof(wchar_t) * modules->buffer[library_masks[index]].library_name.size;
	}
	total_memory += sizeof(Stream<wchar_t>) * library_masks.size;
	total_memory += sizeof(EditorModuleConfiguration) * library_masks.size;

	void* allocation = editor_state->editor_allocator->Allocate(total_memory);
	uintptr_t ptr = (uintptr_t)allocation;
	group.libraries.InitializeFromBuffer(ptr, library_masks.size);
	for (size_t index = 0; index < library_masks.size; index++) {
		group.libraries[index].InitializeAndCopy(ptr, modules->buffer[library_masks[index]].library_name);
	}
	group.name.InitializeAndCopy(ptr, name);
	group.name[name.size] = '\0';
	ptr += 1;
	group.configurations = (EditorModuleConfiguration*)ptr;
	memcpy(group.configurations, configurations, sizeof(EditorModuleConfiguration) * library_masks.size);

	return group;
}

// ---------------------------------------------------------------------------------------------------------------------------

void DeallocateModuleConfigurationGroup(EditorState* editor_state, unsigned int index) {
	ECS_ASSERT(index < editor_state->module_configuration_groups.size);
	editor_state->editor_allocator->Deallocate(editor_state->module_configuration_groups[index].libraries.buffer);
}

// ---------------------------------------------------------------------------------------------------------------------------

void GetModuleConfigurationGroupFilePath(EditorState* editor_state, CapacityStream<wchar_t>& path) {
	path.size = 0;
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(project_file->project_name);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(ToStream(MODULE_CONFIGURATION_GROUP_FILE_EXTENSION));
	path[path.size] = L'\0';
}

// ---------------------------------------------------------------------------------------------------------------------------

unsigned int GetModuleConfigurationGroupIndex(EditorState* editor_state, Stream<char> name) {
	for (size_t index = 0; index < editor_state->module_configuration_groups.size; index++) {
		if (function::CompareStrings(name, editor_state->module_configuration_groups[index].name)) {
			return index;
		}
	}
	return -1;
}

// ---------------------------------------------------------------------------------------------------------------------------

void RemoveModuleConfigurationGroup(EditorState* editor_state, unsigned int index) {
	DeallocateModuleConfigurationGroup(editor_state, index);
	editor_state->module_configuration_groups.RemoveSwapBack(index);
}

// ---------------------------------------------------------------------------------------------------------------------------

void RemoveModuleConfigurationGroup(EditorState* editor_state, Stream<char> name) {
	unsigned int group_index = GetModuleConfigurationGroupIndex(editor_state, name);
	if (group_index != -1) {
		RemoveModuleConfigurationGroup(editor_state, group_index);
	}
	ECS_FORMAT_TEMP_STRING(error_message, "Could not find {0} module configuration group.", name);
	EditorSetConsoleError(editor_state, error_message);
}

// ---------------------------------------------------------------------------------------------------------------------------

void LoadModuleConfigurationGroupFile(EditorState* editor_state) {
	ECS_TEMP_STRING(file_path, 256);
	GetModuleConfigurationGroupFilePath(editor_state, file_path);

	
}

// ---------------------------------------------------------------------------------------------------------------------------

void SaveModuleConfigurationGroupFile(EditorState* editor_state) {

}

// ---------------------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------------------------------------