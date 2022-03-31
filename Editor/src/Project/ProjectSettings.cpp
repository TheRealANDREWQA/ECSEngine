#include "editorpch.h"
#include "ProjectSettings.h"
#include "Editor\EditorState.h"
#include "ProjectFolders.h"

using namespace ECSEngine;

// -------------------------------------------------------------------------------------------------------------

bool SaveWorldParametersFile(EditorState* editor_state, Stream<wchar_t> filename, const WorldDescriptor* descriptor)
{
	// Get type reflection type
	Reflection::ReflectionType reflection_type = editor_state->ReflectionManager()->GetType(STRING(WorldDescriptor));

	// Get the final path
	ECS_STACK_CAPACITY_STREAM(wchar_t, file_path, 512);
	GetProjectConfigurationFolder(editor_state, file_path);
	file_path.Add(ECS_OS_PATH_SEPARATOR);
	file_path.AddStreamSafe(filename);
	file_path[file_path.size] = L'\0';

	// Use the Serialize Text API
	return TextSerialize(reflection_type, descriptor, file_path);
}

// -------------------------------------------------------------------------------------------------------------

WorldDescriptor LoadWorldParametersFile(EditorState* editor_state, Stream<wchar_t> filename, bool print_detailed_error_message)
{
	WorldDescriptor result = GetDefaultSettings(editor_state);

	// Get the reflection type
	Reflection::ReflectionType reflection_type = editor_state->ReflectionManager()->GetType(STRING(WorldDescriptor));

	// Get the final path
	ECS_STACK_CAPACITY_STREAM(wchar_t, file_path, 512);
	GetProjectConfigurationFolder(editor_state, file_path);
	file_path.Add(ECS_OS_PATH_SEPARATOR);
	file_path.AddStreamSafe(filename);
	file_path[file_path.size] = L'\0';

	// Use the Serialize Text API
	char temp_memory[4096];
	CapacityStream<void> memory_pool(temp_memory, 0, 4096);
	ECS_TEXT_DESERIALIZE_STATUS status = TextDeserialize(reflection_type, &result, memory_pool, file_path);

	// For missing fields try saving the file again after setting the default values
	bool save_file_again = false;
	if (status == ECS_TEXT_DESERIALIZE_COULD_NOT_READ) {
		ECS_FORMAT_TEMP_STRING(error_message, "Could not read settings from file {#}.", file_path);
		EditorSetConsoleError(error_message);

		// Signal that the load failed
		result.thread_count = 0;
	}
	else if ((status & ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS) != 0) {
		ECS_FORMAT_TEMP_STRING(error_message, "Some fields from setting {#} could not be read. They received default values.", file_path);
		EditorSetConsoleWarn(error_message);
		save_file_again = true;
	}
	if ((status & ECS_TEXT_DESERIALIZE_FIELD_DATA_MISSING) != 0) {
		ECS_FORMAT_TEMP_STRING(error_message, "Some fields from setting {#} were missing their data. They received default values.", file_path);
		save_file_again = true;
	}

	if (save_file_again) {
		bool success = SaveWorldParametersFile(editor_state, file_path, &result);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Trying to save settings after values were missing/could not be read failed. Path is {#}.", file_path);
			EditorSetConsoleWarn(error_message);
		}
	}

	return result;
}

// -------------------------------------------------------------------------------------------------------------

void GetDefaultSettingFilePath(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectConfigurationFolder(editor_state, path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStreamSafe(ToStream(DEFAULT_SETTINGS_FILE_NAME));
	path[path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------

WorldDescriptor GetDefaultSettings(const EditorState* editor_state)
{
	WorldDescriptor world_descriptor;

	world_descriptor.thread_count = std::thread::hardware_concurrency();
	world_descriptor.entity_manager_memory_new_allocation_size = ECS_MB * 100;
	world_descriptor.entity_manager_memory_pool_count = ECS_KB * 8;
	world_descriptor.entity_manager_memory_size = ECS_MB * 500;
	world_descriptor.entity_pool_power_of_two = 15;
	world_descriptor.global_memory_new_allocation_size = ECS_MB * 500;
	world_descriptor.global_memory_pool_count = ECS_KB * 4;
	world_descriptor.global_memory_size = ECS_GB;

	world_descriptor.mouse = editor_state->ui_system->m_mouse;
	world_descriptor.keyboard = editor_state->ui_system->m_keyboard;

	return world_descriptor;
}

// -------------------------------------------------------------------------------------------------------------

bool SaveDefaultSettingsFile(EditorState* editor_state)
{
	WorldDescriptor default_settings = GetDefaultSettings(editor_state);
	bool success = SaveWorldParametersFile(editor_state, ToStream(DEFAULT_SETTINGS_FILE_NAME), &default_settings);
	if (!success) {
		EditorSetConsoleError(ToStream("An error happened when saving the default settings."));
	}

	return success;
}

// -------------------------------------------------------------------------------------------------------------

bool ExistsProjectSettings(const EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, settings_path, 512);
	GetProjectSettingsPath(editor_state, settings_path);
	return ExistsFileOrFolder(settings_path);
}

// -------------------------------------------------------------------------------------------------------------

bool CheckProjectSettingsValidity(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	//// Check to see the project settings file integrity
	//if (!ExistsProjectSettings(editor_state)) {
	//	editor_allocator->Deallocate(editor_state->project_file->project_settings.buffer);
	//	editor_state->project_descriptor = GetDefaultSettings(editor_state);

	//	bool success_saved_default_file = SaveDefaultSettingsFile(editor_state);
	//	if (success_saved_default_file) {
	//		// Change the settings file to the default one
	//		size_t default_settings_size = wcslen(DEFAULT_SETTINGS_FILE_NAME);
	//		editor_state->project_file->project_settings.Initialize(editor_allocator, default_settings_size, default_settings_size);
	//		editor_state->project_file->project_settings.Copy(Stream<wchar_t>(DEFAULT_SETTINGS_FILE_NAME, default_settings_size));
	//	}
	//	else {
	//		editor_state->project_file->project_settings.buffer = nullptr;
	//		editor_state->project_file->project_settings.size = 0;
	//		editor_state->project_file->project_settings.capacity = 0;
	//	}

	//	return false;
	//}
	return true;
}

// -------------------------------------------------------------------------------------------------------------

void SetSettingsPath(EditorState* editor_state, Stream<wchar_t> name)
{
	EDITOR_STATE(editor_state);

	// Include the null terminator
	editor_state->project_file->project_settings.Initialize(editor_allocator, name.size + 1, name.size + 1);
	editor_state->project_file->project_settings.Copy(name);
	editor_state->project_file->project_settings[name.size] = L'\0';
	editor_state->project_file->project_settings.capacity--;
}

// -------------------------------------------------------------------------------------------------------------