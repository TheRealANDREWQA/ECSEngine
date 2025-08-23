#include "editorpch.h"
#include "ProjectFolders.h"
#include "ProjectFile.h"
#include "../Editor/EditorState.h"

using namespace ECSEngine;

const wchar_t* PROJECT_DIRECTORIES[] = {
	PROJECT_DEBUG_RELATIVE_PATH,
	PROJECT_UI_RELATIVE_PATH,
	PROJECT_ASSETS_RELATIVE_PATH,
	PROJECT_MODULES_RELATIVE_PATH,
	PROJECT_METADATA_RELATIVE_PATH,
	PROJECT_CONFIGURATION_RELATIVE_PATH,
	PROJECT_CONFIGURATION_MODULES_RELATIVE_PATH,
	PROJECT_CONFIGURATION_RUNTIME_RELATIVE_PATH,
	PROJECT_BACKUP_RELATIVE_PATH,
	PROJECT_CRASH_RELATIVE_PATH,
	PROJECT_PREFABS_RELATIVE_PATH
};

size_t PROJECT_DIRECTORIES_SIZE()
{
	return std::size(PROJECT_DIRECTORIES);
}

static void GetProjectRootPath(const EditorState* editor_state, CapacityStream<wchar_t>& path, const wchar_t* relative_path) {
	const ProjectFile* project_file = editor_state->project_file;
	path.AddStream(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(relative_path);
	path[path.size] = L'\0';
	path.AssertCapacity();
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectFilePath(const ProjectFile* project_file, CapacityStream<wchar_t>& characters) {
	characters.AddStream(project_file->path);
	characters.Add(ECS_OS_PATH_SEPARATOR);
	Stream<wchar_t> extension = PROJECT_EXTENSION;
	characters.AddStreamSafe(extension);
	characters[characters.size] = L'\0';
	characters.AssertCapacity();
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectSettingsFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_SETTINGS_FILE_EXTENSION);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectDebugFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_DEBUG_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectUIFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_UI_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectSandboxFile(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& characters)
{
	GetProjectRootPath(editor_state, characters, PROJECT_SANDBOX_FILE_EXTENSION);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectAssetsFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_ASSETS_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectModulesFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_MODULES_FILE_EXTENSION);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectMetadataFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_METADATA_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectConfigurationFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_CONFIGURATION_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectModulesFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_MODULES_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectConfigurationModuleFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_CONFIGURATION_MODULES_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectBackupFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_BACKUP_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectConfigurationRuntimeFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_CONFIGURATION_RUNTIME_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectCrashFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectRootPath(editor_state, path, PROJECT_CRASH_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectPrefabFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path) {
	GetProjectRootPath(editor_state, path, PROJECT_PREFABS_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectRootPath(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	path.CopyOther(editor_state->project_file->path);
}

// -------------------------------------------------------------------------------------------------------------

Stream<wchar_t> GetProjectAssetRelativePath(const EditorState* editor_state, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);
	return PathRelativeToAbsolute(path, assets_folder);
}

// -------------------------------------------------------------------------------------------------------------

Stream<wchar_t> GetProjectAssetRelativePathWithSeparatorReplacement(const EditorState* editor_state, Stream<wchar_t> path, CapacityStream<wchar_t>& storage) {
	Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, path);
	unsigned int add_index = storage.AddStreamAssert(relative_path);
	ReplaceCharacter(storage.SliceAt(add_index), ECS_OS_PATH_SEPARATOR, ECS_OS_PATH_SEPARATOR_REL);
	return storage.SliceAt(add_index);
}

// -------------------------------------------------------------------------------------------------------------

Stream<wchar_t> GetProjectPathFromAssetRelative(const EditorState* editor_state, CapacityStream<wchar_t>& storage, Stream<wchar_t> relative_path) {
	unsigned int storage_size = storage.size;
	GetProjectAssetsFolder(editor_state, storage);
	storage.AddAssert(ECS_OS_PATH_SEPARATOR);
	storage.AddStreamAssert(relative_path);
	Stream<wchar_t> path = storage.SliceAt(storage_size);
	ReplaceCharacter(path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
	return path;
}

// -------------------------------------------------------------------------------------------------------------

Stream<wchar_t> MakeAbsolutePathFromProjectAssetRelative(const EditorState* editor_state, Stream<wchar_t> relative_path, CapacityStream<wchar_t>& storage) {
	unsigned int storage_size = storage.size;

	GetProjectAssetsFolder(editor_state, storage);
	storage.AddAssert(ECS_OS_PATH_SEPARATOR);
	// Note: At the moment, the separators are not replaced
	storage.AddStreamAssert(relative_path);

	return storage.SliceAt(storage_size);
}

// -------------------------------------------------------------------------------------------------------------
