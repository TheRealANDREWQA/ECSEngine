#include "editorpch.h"
#include "ProjectFolders.h"
#include "ProjectFile.h"
#include "../Editor/EditorState.h"

using namespace ECSEngine;

void GetProjectOrganizationFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path, const wchar_t* relative_path) {
	const ProjectFile* project_file = editor_state->project_file;
	path.AddStream(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(ToStream(relative_path));
	path[path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectFilePath(CapacityStream<wchar_t>& characters, const ProjectFile* project_file) {
	characters.AddStream(project_file->path);
	characters.Add(ECS_OS_PATH_SEPARATOR);
	characters.AddStream(project_file->project_name);
	Stream<wchar_t> extension = Stream<wchar_t>(PROJECT_EXTENSION, std::size(PROJECT_EXTENSION) - 1);
	characters.AddStreamSafe(extension);
	characters[characters.size] = L'\0';
	ECS_ASSERT(characters.size < characters.capacity);
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

void GetProjectAssetsFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_ASSETS_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectMetafilesFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_METAFILES_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectConfigurationFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path)
{
	GetProjectOrganizationFolder(editor_state, path, PROJECT_CONFIGURATION_RELATIVE_PATH);
}

// -------------------------------------------------------------------------------------------------------------

void GetProjectSettingsPath(const EditorState* editor_state, CapacityStream<wchar_t>& path) {
	GetProjectConfigurationFolder(editor_state, path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStreamSafe(editor_state->project_file->project_settings);
	path[path.size] = L'\0';
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
