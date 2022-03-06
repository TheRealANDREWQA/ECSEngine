#pragma once
#include "editorpch.h"

struct EditorState;

// This is an array of extensions that can generate metafile extensions
extern const wchar_t* METAFILE_EXTENSIONS[];
extern size_t METAFILE_EXTENSION_COUNT;

enum EDTIOR_METAFILE {
	EDITOR_METAFILE_TEXTURE,
	EDITOR_METAFILE_SHADER,
	EDITOR_METAFILE_MESH
};

void RemoveProjectUnbindedMetafiles(const EditorState* editor_state);

// Creates a metafile with default values for the given path only if the metafile doesn't exist
// It will infer the metafile type from the path extension
// It will return if the metafile already exists
bool CreateProjectMetafile(EditorState* editor_state, ECSEngine::CapacityStream<wchar_t> path);

void CreateProjectMissingMetafiles(EditorState* editor_state);

// It changes the Metafile folder to Assets and removes the .meta
void FromMetafilePathToAssetsPath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& demangled_path, ECSEngine::Stream<wchar_t> path);

// It changes the Assets folder to Metafile and add .meta
void FromAssetsPathToMetafilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& mangled_path, ECSEngine::Stream<wchar_t> path);

// Writes into the file the default values for that metafile
void WriteMetafileDefaultValues(ECSEngine::ECS_FILE_HANDLE file_handle, ECSEngine::Stream<wchar_t> path);