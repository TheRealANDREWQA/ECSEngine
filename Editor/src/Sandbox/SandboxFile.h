#pragma once

struct EditorState;

// -------------------------------------------------------------------------------------------------------------

bool LoadEditorSandboxFile(
	EditorState* editor_state
);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeded. On error it will print an error message
bool SaveEditorSandboxFile(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------