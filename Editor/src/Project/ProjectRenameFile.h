#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

// This function is meant to be used for renamings inside the assets
// Folder where we have references to files from assets/prefabs or other
// Resources and we want to maintain those references
// Returns true if it succeeded, else false
bool ProjectRenameFile(EditorState* editor_state, Stream<wchar_t> path, Stream<wchar_t> rename_filename);

// This function is meant to be used for renamings inside the assets
// Folder where we have references to files from assets/prefabs or other
// Resources and we want to maintain those references
// Returns true if it succeeded, else false
bool ProjectRenameFileAbsolute(EditorState* editor_state, Stream<wchar_t> path, Stream<wchar_t> rename_absolute_path);