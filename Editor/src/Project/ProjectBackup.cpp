#include "editorpch.h"
#include "ProjectBackup.h"
#include "Editor\EditorState.h"
#include "ProjectFolders.h"
#include "..\Modules\ModuleFile.h"

using namespace ECSEngine;

// Every 20 minutes save a backup of the project
#define BACKUP_PROJECT_LAZY_COUNTER (1000 * 60 * 20)

// -------------------------------------------------------------------------------------------------

bool ProjectNeedsBackup(EditorState* editor_state)
{
	return editor_state->lazy_evalution_timer.GetDuration_ms() > BACKUP_PROJECT_LAZY_COUNTER;
}

// -------------------------------------------------------------------------------------------------

void ResetProjectNeedsBackup(EditorState* editor_state)
{
	editor_state->lazy_evalution_timer.SetNewStart();
}

// -------------------------------------------------------------------------------------------------

void AddProjectNeedsBackupTime(EditorState* editor_state, size_t second_count)
{
	editor_state->lazy_evalution_timer.DelayStart(second_count * 1'000'000'000);
}

// -------------------------------------------------------------------------------------------------

bool SaveProjectBackup(const EditorState* editor_state)
{
	// Copy the files for singular one and the folders for the rest
	
	// The name of the backup will reflect the date at which the backup is realized
	ECS_STACK_CAPACITY_STREAM(wchar_t, path, 512);
	GetProjectBackupFolder(editor_state, path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	Date date = OS::GetLocalTime();
	function::ConvertDateToString(date, path, ECS_LOCAL_TIME_FORMAT_HOUR | ECS_LOCAL_TIME_FORMAT_MINUTES | ECS_LOCAL_TIME_FORMAT_DAY
		| ECS_LOCAL_TIME_FORMAT_MONTH | ECS_LOCAL_TIME_FORMAT_YEAR | ECS_LOCAL_TIME_FORMAT_DASH_INSTEAD_OF_COLON);

	auto error_lambda = [&](Stream<char> reason) {
		ECS_STACK_CAPACITY_STREAM(char, message, 1024);
		message.Copy(ToStream("An error has occured when trying to save a project backup. Reason: "));
		message.AddStreamSafe(reason);

		EditorSetConsoleError(message);
		bool success = RemoveFolder(path);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error occured when trying to remove the backup folder which failed. Conside doing this manually. File name is {#}.", function::PathFilename(path));
			EditorSetConsoleError(error_message);
		}
	};

	// Create the folder
	bool success = CreateFolder(path);
	if (!success) {
		EditorSetConsoleError(ToStream("An error has occured when a project backup was saved. The folder could not be created."));
		return false;
	}

	// Copy the .ecsproj file
	ECS_STACK_CAPACITY_STREAM(wchar_t, file_or_folder_to_copy, 512);
	GetProjectFilePath(file_or_folder_to_copy, editor_state->project_file);
	success = FileCopy(file_or_folder_to_copy, path, true);
	if (!success) {
		error_lambda(ToStream("Could not copy the project file."));
		return false;
	}

	file_or_folder_to_copy.size = 0;
	GetProjectModuleFilePath(editor_state, file_or_folder_to_copy);
	success = FileCopy(file_or_folder_to_copy, path, true);
	if (!success) {
		error_lambda(ToStream("Could not copy the module file."));
		return false;
	}

	// Now copy the UI and Configuration folders
	file_or_folder_to_copy.size = 0;
	GetProjectUIFolder(editor_state, file_or_folder_to_copy);
	success = FolderCopy(file_or_folder_to_copy, path);
	if (!success) {
		error_lambda(ToStream("An error has occured when a project backup was saved. The project's UI folder could not be copied."));
		return false;
	}

	file_or_folder_to_copy.size = 0;
	GetProjectConfigurationFolder(editor_state, file_or_folder_to_copy);
	success = FolderCopy(file_or_folder_to_copy, path);
	if (!success) {
		error_lambda(ToStream("An error has occured when a project backup was saved. The project's Configuration folder could not be copied."));
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------------------------

bool LoadProjectBackup(const EditorState* editor_state, Stream<wchar_t> folder)
{
	ProjectBackupFiles files_mask[PROJECT_BACKUP_COUNT];
	for (size_t index = 0; index < PROJECT_BACKUP_COUNT; index++) {
		files_mask[index] = (ProjectBackupFiles)index;
	}

	return LoadProjectBackup(editor_state, folder, { files_mask, PROJECT_BACKUP_COUNT });
}

// -------------------------------------------------------------------------------------------------

bool LoadProjectBackup(const EditorState* editor_state, Stream<wchar_t> folder, Stream<ProjectBackupFiles> file_mask)
{
	bool valid_files[PROJECT_BACKUP_COUNT] = { false };

	for (size_t index = 0; index < file_mask.size; index++) {
		valid_files[file_mask[index]] = true;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, to_path, 512);

	auto rename_file_or_folder_temporary = [](Stream<wchar_t> to_path, const wchar_t* temporary_name) {
		bool rename_success = RenameFolderOrFile(to_path, ToStream(temporary_name));
		if (!rename_success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error has occured when trying to rename a file/folder to temporary during backup. The file/folder {#} was not recovered from the backup.", to_path);
			EditorSetConsoleWarn(error_message);
		}
		return rename_success;
	};

	auto recover_file_or_folder_temporary = [](Stream<wchar_t> to_path, const wchar_t* temporary_name) {
		Stream<wchar_t> temp_name = ToStream(temporary_name);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_temp_path, 512);
		Stream<wchar_t> parent_path = function::PathParent(to_path);
		absolute_temp_path.Copy(parent_path);
		absolute_temp_path.Add(ECS_OS_PATH_SEPARATOR);
		absolute_temp_path.AddStreamSafe(temp_name);
		absolute_temp_path[absolute_temp_path.size] = L'\0';

		Stream<wchar_t> path_filename = function::PathFilename(to_path);
		bool rename_success = RenameFolderOrFile(absolute_temp_path, path_filename);
		if (!rename_success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error has occured when trying to rename a temporary file/folder to it's initial name. "
				"The file/folder {#} should be renamed manually to {1}.", absolute_temp_path, path_filename);
			EditorSetConsoleError(error_message);
		}
	};

	auto delete_file_or_folder_temporary = [](Stream<wchar_t> to_path, const wchar_t* temporary_name) {
		Stream<wchar_t> temp_name = ToStream(temporary_name);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_temp_path, 512);
		Stream<wchar_t> parent_path = function::PathParent(to_path);
		absolute_temp_path.Copy(parent_path);
		absolute_temp_path.Add(ECS_OS_PATH_SEPARATOR);
		absolute_temp_path.AddStreamSafe(temp_name);
		absolute_temp_path[absolute_temp_path.size] = L'\0';

		size_t extension_size = function::PathExtensionSize(absolute_temp_path);
		bool delete_success;
		if (extension_size == 0) {
			delete_success = RemoveFolder(absolute_temp_path);
		}
		else {
			delete_success = RemoveFile(absolute_temp_path);
		}

		if (!delete_success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error has occured when trying to delete a temporary file/folder during backup restore. Consider deleting {#} manually.", absolute_temp_path);
			EditorSetConsoleWarn(error_message);
		}
	};

	const wchar_t* temporary_names[PROJECT_BACKUP_COUNT] = {
		L"temporary_ui",
		L"temporary.ecsproj",
		L"temporary.ecsmodules",
		L"temporary_configuration"
	};

	const char* base_error_string = "An error occured when trying to recover the project's {#} from the backup. The backup will continue with the rest of the specified files.";

	const char* error_strings[PROJECT_BACKUP_COUNT] = {
		"UI folder",
		"file",
		"module file",
		"Configuration folder"
	};

	bool file_copy[PROJECT_BACKUP_COUNT] = {
		false,
		true,
		true,
		false
	};

	auto get_project_file_path = [](const EditorState* editor_state, CapacityStream<wchar_t>& path) {
		GetProjectFilePath(path, editor_state->project_file);
	};

	typedef void (*GetPath)(const EditorState* editor_state, CapacityStream<wchar_t>& path);
	GetPath get_paths[PROJECT_BACKUP_COUNT] = {
		GetProjectUIFolder,
		get_project_file_path,
		GetProjectModuleFilePath,
		GetProjectConfigurationFolder
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, backup_file_or_folder_copy, 512);
	backup_file_or_folder_copy.Copy(folder);
	unsigned int backup_file_or_folder_copy_size = backup_file_or_folder_copy.size;

	bool success = true;
	for (size_t index = 0; index < PROJECT_BACKUP_COUNT; index++) {
		to_path.size = 0;
		if (valid_files[index]) {
			get_paths[index](editor_state, to_path);
			if (rename_file_or_folder_temporary(to_path, temporary_names[index])) {
				Stream<wchar_t> parent_path = function::PathParent(to_path);
				Stream<wchar_t> filename = { parent_path.buffer + parent_path.size + 1, to_path.size - parent_path.size - 1 };
				backup_file_or_folder_copy.size = backup_file_or_folder_copy_size;
				backup_file_or_folder_copy.Add(ECS_OS_PATH_SEPARATOR);
				backup_file_or_folder_copy.AddStreamSafe(filename);

				bool current_success;
				if (file_copy[index]) {
					current_success = FileCopy(backup_file_or_folder_copy, parent_path);
				}
				else {
					current_success = FolderCopy(backup_file_or_folder_copy, parent_path);
				}

				if (!current_success) {
					ECS_FORMAT_TEMP_STRING(error_message, base_error_string, error_strings[index]);
					EditorSetConsoleWarn(error_message);
					recover_file_or_folder_temporary(to_path, temporary_names[index]);
				}
				else {
					delete_file_or_folder_temporary(to_path, temporary_names[index]);
				}
				success &= current_success;
			}
		}
	}

	return success;
}

// -------------------------------------------------------------------------------------------------
