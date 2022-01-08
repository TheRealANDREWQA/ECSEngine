#pragma once
#include "editorpch.h"

struct EditorState;

enum EDTIOR_METAFILE {
	EDITOR_METAFILE_TEXTURE,
	EDITOR_METAFILE_SHADER,
	EDITOR_METAFILE_MATERIAL,
	EDITOR_METAFILE_MESH
};

void RemoveProjectUnbindedMetafiles(EditorState* editor_state);

// Creates a metafile with default values for the given path
// It will infer the metafile type
void CreateProjectMetafile(EditorState* editor_state, ECSEngine::containers::CapacityStream<wchar_t> path);