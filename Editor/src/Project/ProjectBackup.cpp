#include "editorpch.h"
#include "ProjectBackup.h"
#include "Editor/EditorState.h"
#include "ProjectFolders.h"
#include "../Modules/ModuleFile.h"
#include "../Assets/AssetManagement.h"
#include "../Sandbox/SandboxScene.h"
#include "../Assets/AssetExtensions.h"

using namespace ECSEngine;

Stream<char> PROJECT_BACKUP_FILE_NAMES[] = {
	"UI Folder",
	"Project File",
	"Modules File",
	"Sandbox File",
	"Configuration Folder",
	"Asset Metadata Files",
	"Scene Files"
};

static_assert(PROJECT_BACKUP_COUNT == std::size(PROJECT_BACKUP_FILE_NAMES));

// Every 20 minutes save a backup of the project
#define BACKUP_PROJECT_LAZY_COUNTER ECS_MINUTES_AS_MILLISECONDS(20)

// -------------------------------------------------------------------------------------------------

bool ProjectNeedsBackup(EditorState* editor_state)
{
	return editor_state->lazy_evalution_timer.GetDuration(ECS_TIMER_DURATION_MS) > BACKUP_PROJECT_LAZY_COUNTER;
}

// -------------------------------------------------------------------------------------------------

void ResetProjectNeedsBackup(EditorState* editor_state)
{
	editor_state->lazy_evalution_timer.SetNewStart();
}

// -------------------------------------------------------------------------------------------------

void AddProjectNeedsBackupTime(EditorState* editor_state, size_t second_count)
{
	editor_state->lazy_evalution_timer.DelayStart(second_count, ECS_TIMER_DURATION_S);
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
	ConvertDateToString(date, path, ECS_FORMAT_DATE_ALL_FROM_MINUTES);

	auto error_lambda = [&](Stream<char> reason) {
		ECS_STACK_CAPACITY_STREAM(char, message, 1024);
		message.CopyOther("An error has occured when trying to save a project backup. Reason: ");
		message.AddStreamSafe(reason);

		EditorSetConsoleError(message);
		bool success = RemoveFolder(path);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error occured when trying to remove the backup folder which failed. Consider doing this manually. "
				"File name is {#}.", PathFilename(path));
			EditorSetConsoleError(error_message);
		}
	};

	// Create the folder
	bool success = CreateFolder(path);
	if (!success) {
		EditorSetConsoleError("An error has occured when a project backup was saved. The folder could not be created.");
		return false;
	}

	// Copy the .ecsproj file
	ECS_STACK_CAPACITY_STREAM(wchar_t, file_or_folder_to_copy, 512);
	GetProjectFilePath(editor_state->project_file, file_or_folder_to_copy);
	success = FileCopy(file_or_folder_to_copy, path, true);
	if (!success) {
		error_lambda("Could not copy the project file.");
		return false;
	}

	// Copy the .ecsmodules file
	file_or_folder_to_copy.size = 0;
	GetProjectModulesFilePath(editor_state, file_or_folder_to_copy);
	success = FileCopy(file_or_folder_to_copy, path, true);
	if (!success) {
		error_lambda("Could not copy the project modules file");
		return false;
	}

	// Copy the .ecss file
	file_or_folder_to_copy.size = 0;
	GetProjectSandboxFile(editor_state, file_or_folder_to_copy);
	success = FileCopy(file_or_folder_to_copy, path, true);
	if (!success) {
		error_lambda("Could not copy the project sandbox file");
		return false;
	}

	// Copy the UI folder
	file_or_folder_to_copy.size = 0;
	GetProjectUIFolder(editor_state, file_or_folder_to_copy);
	success = FolderCopy(file_or_folder_to_copy, path);
	if (!success) {
		error_lambda("The project's UI folder could not be copied.");
		return false;
	}

	// Copy the Configuration folder
	file_or_folder_to_copy.size = 0;
	GetProjectConfigurationFolder(editor_state, file_or_folder_to_copy);
	success = FolderCopy(file_or_folder_to_copy, path);
	if (!success) {
		error_lambda("The project's Configuration folder could not be copied.");
		return false;
	}

	// Copy the Metadata folder
	file_or_folder_to_copy.size = 0;
	GetProjectMetadataFolder(editor_state, file_or_folder_to_copy);
	success = FolderCopy(file_or_folder_to_copy, path);
	if (!success) {
		error_lambda("The project's Metadata folder could not be copied.");
		return false;
	}

	// Copy the asset files now - we need to gather them
	CapacityStream<wchar_t> backup_assets_path = path;
	backup_assets_path.Add(ECS_OS_PATH_SEPARATOR);
	backup_assets_path.AddStreamAssert(PROJECT_ASSETS_RELATIVE_PATH);

	success = CreateFolder(backup_assets_path);
	if (!success) {
		error_lambda("The project's Assets directory could not be created.");
		return false;
	}

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);

	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, assets_folder_directories_storage, ECS_KB);
	AdditionStream<Stream<wchar_t>> asset_folder_directories = &assets_folder_directories_storage;

	// Create all the folders in the assets folder such that we don't need to check manually for each asset file
	// If its parent exist or not
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);
	GetDirectoriesOrFilesOptions get_directories_options;
	get_directories_options.relative_root = assets_folder;

	GetDirectoriesRecursive(assets_folder, &stack_allocator, asset_folder_directories, get_directories_options);

	// Create the directories first
	unsigned int backup_asset_folder_base_size = backup_assets_path.size;
	for (unsigned int index = 0; index < asset_folder_directories.Size(); index++) {
		backup_assets_path.size = backup_asset_folder_base_size;
		backup_assets_path.Add(ECS_OS_PATH_SEPARATOR);
		backup_assets_path.AddStreamAssert(asset_folder_directories[index]);
		ReplaceCharacter(backup_assets_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);

		success = CreateFolder(backup_assets_path);
		if (!success) {
			error_lambda("The project's Assets child directories could not be created.");
			return false;
		}
	}

	stack_allocator.Clear();
	
	Stream<Stream<wchar_t>> asset_paths = GetAssetsFromAssetsFolder(editor_state, &stack_allocator);
	unsigned int asset_folder_size = assets_folder.size;
	// Now copy the asset thunk or forwarding files
	for (size_t index = 0; index < asset_paths.size; index++) {
		backup_assets_path.size = backup_asset_folder_base_size;
		backup_assets_path.Add(ECS_OS_PATH_SEPARATOR);
		backup_assets_path.AddStreamAssert(asset_paths[index]);

		assets_folder.size = asset_folder_size;
		assets_folder.Add(ECS_OS_PATH_SEPARATOR);
		assets_folder.AddStreamAssert(asset_paths[index]);

		success = FileCopy(assets_folder, backup_assets_path, false);
		if (!success) {
			error_lambda("The project's Asset forward facing files could not be copied.");
			return false;
		}
	}

	assets_folder.size = asset_folder_size;
	backup_assets_path.size = backup_asset_folder_base_size;

	// In this array, we record the stem of the scenes that were already copied
	// Such that we don't try to copy again the same scene
	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, copied_sandbox_scenes, 512);

	ECS_STACK_CAPACITY_STREAM(wchar_t, sandbox_scene_path, 512);
	// Now copy the scenes in use by the sandboxes
	// Exclude temporary sandboxes
	unsigned int sandbox_count = GetSandboxCount(editor_state, true);
	for (unsigned int index = 0; index < sandbox_count; index++) {
		sandbox_scene_path.size = 0;
		Stream<wchar_t> scene_path = GetSandbox(editor_state, index)->scene_path;
		if (FindString(scene_path, copied_sandbox_scenes.ToStream()) == -1) {
			copied_sandbox_scenes.AddAssert(scene_path);
			GetSandboxScenePath(editor_state, index, sandbox_scene_path);
			if (sandbox_scene_path.size > 0) {
				// Copy the scene
				Stream<wchar_t> relative_path = PathRelativeToAbsolute(sandbox_scene_path, assets_folder);
				backup_assets_path.size = backup_asset_folder_base_size;
				backup_assets_path.Add(ECS_OS_PATH_SEPARATOR);
				backup_assets_path.AddStreamAssert(relative_path);

				success = FileCopy(sandbox_scene_path, backup_assets_path, false);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(error_message, "The project's scene {#} could not be copied.", relative_path);
					error_lambda(error_message);
					return false;
				}
			}
		}
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

	auto rename_file_or_folder_temporary = [](Stream<wchar_t> to_path, Stream<wchar_t> temporary_name) {
		bool rename_success = RenameFileOrFolder(to_path, temporary_name);
		if (!rename_success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error has occured when trying to rename a file/folder to temporary during backup. "
				"The file / folder {#} was not recovered from the backup.", to_path);
			EditorSetConsoleWarn(error_message);
		}
		return rename_success;
	};

	auto recover_file_or_folder_temporary = [](Stream<wchar_t> to_path, Stream<wchar_t> temporary_name) {
		Stream<wchar_t> temp_name = temporary_name;
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_temp_path, 512);

		Stream<wchar_t> parent_path = PathParent(to_path);
		absolute_temp_path.CopyOther(parent_path);
		absolute_temp_path.Add(ECS_OS_PATH_SEPARATOR);
		absolute_temp_path.AddStreamSafe(temp_name);
		absolute_temp_path[absolute_temp_path.size] = L'\0';

		Stream<wchar_t> path_filename = PathFilename(to_path);
		bool rename_success = RenameFileOrFolder(absolute_temp_path, path_filename);
		if (!rename_success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error has occured when trying to rename a temporary file/folder to it's initial name. "
				"The file/folder {#} should be renamed manually to {#}.", absolute_temp_path, path_filename);
			EditorSetConsoleError(error_message);
		}
	};

	auto delete_file_or_folder_temporary = [](Stream<wchar_t> to_path, Stream<wchar_t> temporary_name) {
		Stream<wchar_t> temp_name = temporary_name;
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_temp_path, 512);

		Stream<wchar_t> parent_path = PathParent(to_path);
		absolute_temp_path.CopyOther(parent_path);
		absolute_temp_path.Add(ECS_OS_PATH_SEPARATOR);
		absolute_temp_path.AddStreamSafe(temp_name);
		absolute_temp_path[absolute_temp_path.size] = L'\0';

		size_t extension_size = PathExtensionSize(absolute_temp_path);
		bool delete_success;
		if (extension_size == 0) {
			delete_success = RemoveFolder(absolute_temp_path);
		}
		else {
			delete_success = RemoveFile(absolute_temp_path);
		}

		if (!delete_success) {
			ECS_FORMAT_TEMP_STRING(error_message, "An error has occured when trying to delete a temporary file/folder during backup restore. " 
				"Consider deleting{#} manually.", absolute_temp_path);
			EditorSetConsoleWarn(error_message);
		}
	};

	// We can handle all the files and folder copies generally by abstracting away some information
	// The scenes and the assets forwarding or thunk files cannot be copied like that

	const wchar_t* temporary_names[PROJECT_BACKUP_COUNT] = {
		L"temporary.ui",
		L"temporary.ecsproj",
		L"temporary.ecsmodules",
		L"temporary.ecss",
		L"temporary_configuration",
		L"temporary_metadata",
	};

	const char* base_error_string = "An error occured when trying to recover the project's {#} from the backup. "
		"The backup will continue with the rest of the specified files";

	const char* error_strings[PROJECT_BACKUP_COUNT - 1] = {
		"UI folder",
		"file",
		"module file",
		"sandbox file",
		"Configuration folder",
		"Metadata folder"
	};

	bool file_copy[PROJECT_BACKUP_COUNT - 1] = {
		false,
		true,
		true,
		true,
		false,
		false
	};

	auto get_project_file_path = [](const EditorState* editor_state, CapacityStream<wchar_t>& path) {
		GetProjectFilePath(editor_state->project_file, path);
	};

	typedef void (*GetPath)(const EditorState* editor_state, CapacityStream<wchar_t>& path);
	GetPath get_paths[PROJECT_BACKUP_COUNT - 1] = {
		GetProjectUIFolder,
		get_project_file_path,
		GetProjectModulesFilePath,
		GetProjectSandboxFile,
		GetProjectConfigurationFolder,
		GetProjectMetadataFolder
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, backup_file_or_folder_copy, 512);
	backup_file_or_folder_copy.CopyOther(folder);
	unsigned int backup_file_or_folder_copy_size = backup_file_or_folder_copy.size;

	bool success = true;
	for (size_t index = 0; index < PROJECT_BACKUP_COUNT - 1; index++) {
		to_path.size = 0;
		if (valid_files[index]) {
			get_paths[index](editor_state, to_path);
			if (rename_file_or_folder_temporary(to_path, temporary_names[index])) {
				Stream<wchar_t> parent_path = PathParent(to_path);
				Stream<wchar_t> filename = { parent_path.buffer + parent_path.size + 1, to_path.size - parent_path.size - 1 };
				backup_file_or_folder_copy.size = backup_file_or_folder_copy_size;
				backup_file_or_folder_copy.Add(ECS_OS_PATH_SEPARATOR);
				backup_file_or_folder_copy.AddStreamSafe(filename);

				bool current_success;
				if (file_copy[index]) {
					current_success = FileCopy(backup_file_or_folder_copy, parent_path, true, true);
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

	if (valid_files[PROJECT_BACKUP_ASSETS_METADATA]) {
		// We need to copy the assets forwaring or thunking files as well
		ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, assets_valid_extensions, 64);
		GetAssetExtensionsWithThunkOrForwardingFile(assets_valid_extensions);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);

		AllocatorPolymorphic stack_allocator = &_stack_allocator;
		ResizableStream<Stream<wchar_t>> asset_files_paths(stack_allocator, 0);
		AdditionStream<Stream<wchar_t>> asset_files_paths_addition = &asset_files_paths;

		ECS_STACK_CAPACITY_STREAM(wchar_t, backup_assets_folder, 512);
		backup_assets_folder.CopyOther(folder);
		backup_assets_folder.Add(ECS_OS_PATH_SEPARATOR);
		backup_assets_folder.AddStreamAssert(PROJECT_ASSETS_RELATIVE_PATH);

		ECS_STACK_CAPACITY_STREAM(wchar_t, project_assets_folder, 512);
		GetProjectAssetsFolder(editor_state, project_assets_folder);

		GetDirectoriesOrFilesOptions get_files_options;
		get_files_options.relative_root = backup_assets_folder;
		GetDirectoryFilesWithExtensionRecursive(folder, stack_allocator, asset_files_paths_addition, assets_valid_extensions, get_files_options);

		unsigned int base_project_assets_folder_size = project_assets_folder.size;
		unsigned int base_backup_assets_folder_size = backup_assets_folder.size;
		for (unsigned int index = 0; index < asset_files_paths.size; index++) {
			project_assets_folder.size = base_project_assets_folder_size;
			project_assets_folder.Add(ECS_OS_PATH_SEPARATOR);
			project_assets_folder.AddStreamAssert(asset_files_paths[index]);

			backup_assets_folder.size = base_backup_assets_folder_size;
			backup_assets_folder.Add(ECS_OS_PATH_SEPARATOR);
			backup_assets_folder.AddStreamAssert(asset_files_paths[index]);

			bool current_success = FileCopy(backup_assets_folder, project_assets_folder, false, true);
			success &= current_success;
			if (!current_success) {
				ECS_FORMAT_TEMP_STRING(error_message, "An error occured when trying to recover the asset file {#} from the backup. "
					"The backup will continue with the rest of the specified files", asset_files_paths[index]);
				EditorSetConsoleWarn(error_message);
			}
		}
	}

	if (valid_files[PROJECT_BACKUP_SCENE]) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, backup_assets_folder, 512);
		backup_assets_folder.CopyOther(folder);
		backup_assets_folder.Add(ECS_OS_PATH_SEPARATOR);
		backup_assets_folder.AddStreamAssert(PROJECT_ASSETS_RELATIVE_PATH);

		ECS_STACK_CAPACITY_STREAM(wchar_t, project_assets_folder, 512);
		GetProjectAssetsFolder(editor_state, project_assets_folder);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
		AllocatorPolymorphic stack_allocator = &_stack_allocator;

		ResizableStream<Stream<wchar_t>> scene_files(stack_allocator, 0);
		AdditionStream<Stream<wchar_t>> scene_files_addition = &scene_files;
		Stream<wchar_t> scene_extension = EDITOR_SCENE_EXTENSION;
		GetDirectoriesOrFilesOptions get_options;
		get_options.relative_root = backup_assets_folder;
		GetDirectoryFilesWithExtensionRecursive(backup_assets_folder, stack_allocator, scene_files_addition, { &scene_extension, 1 }, get_options);

		unsigned int backup_assets_folder_base_size = backup_assets_folder.size;
		unsigned int project_assets_folder_base_size = project_assets_folder.size;
		for (unsigned int index = 0; index < scene_files.size; index++) {
			backup_assets_folder.size = backup_assets_folder_base_size;
			backup_assets_folder.Add(ECS_OS_PATH_SEPARATOR);
			backup_assets_folder.AddStreamAssert(scene_files[index]);

			project_assets_folder.size = project_assets_folder_base_size;
			project_assets_folder.Add(ECS_OS_PATH_SEPARATOR);
			project_assets_folder.AddStreamAssert(scene_files[index]);

			bool current_success = FileCopy(backup_assets_folder, project_assets_folder, false, true);
			success &= current_success;
			if (!current_success) {
				ECS_FORMAT_TEMP_STRING(error_message, "An error occured when trying to recover the scene file {#} from the backup. "
					"The backup will continue with the rest of the specified files", scene_files[index]);
				EditorSetConsoleWarn(error_message);
			}
		}
	}

	return success;
}

// -------------------------------------------------------------------------------------------------
