#include "editorpch.h"
#include "ProjectFolders.h"
#include "ProjectFile.h"
#include "../Editor/EditorState.h"

using namespace ECSEngine;
ECS_CONTAINERS;

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
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.AddStream(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(ToStream(PROJECT_DEBUG_RELATIVE_PATH));
	path[path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectAssetsFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.AddStream(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(ToStream(PROJECT_ASSETS_RELATIVE_PATH));
	path[path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectMetafilesFolder(const EditorState* editor_state, ECSEngine::containers::CapacityStream<wchar_t>& path)
{
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.AddStream(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(ToStream(PROJECT_METAFILES_RELATIVE_PATH));
	path[path.size] = L'\0';
}
