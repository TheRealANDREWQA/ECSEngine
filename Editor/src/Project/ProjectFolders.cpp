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
	PROJECT_BACKUP_RELATIVE_PATH
};

size_t PROJECT_DIRECTORIES_SIZE()
{
	return std::size(PROJECT_DIRECTORIES);
}

void GetProjectOrganizationFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path, const wchar_t* relative_path) {
	const ProjectFile* project_file = editor_state->project_file;
	path.AddStream(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(relative_path);
	path[path.size] = L'\0';
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

void GetProjectDebugFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_DEBUG_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectUIFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_UI_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectSandboxFile(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& characters)
{
	characters.AddStream(editor_state->project_file->path);
	characters.Add(ECS_OS_PATH_SEPARATOR);
	characters.AddStream(PROJECT_SANDBOX_FILE_EXTENSION);
	characters[characters.size] = L'\0';
	characters.AssertCapacity();
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectAssetsFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_ASSETS_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectModulesFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	const ProjectFile* project_file = editor_state->project_file;
	path.CopyOther(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStreamSafe(PROJECT_MODULES_FILE_EXTENSION);
	path[path.size] = L'\0';
	path.AssertCapacity();
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectMetadataFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_METADATA_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectConfigurationFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_CONFIGURATION_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectConfigurationModuleFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_CONFIGURATION_MODULES_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectBackupFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_BACKUP_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectConfigurationRuntimeFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_CONFIGURATION_RUNTIME_RELATIVE_PATH);
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
