#pragma once

struct EditorState;

// -------------------------------------------------------------------------------------------------------------

// Returns the previous value of the flag
bool DisableEditorSandboxFileSave(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

bool LoadEditorSandboxFile(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// It will restore the previous state of the flag and write the sandbox file if it
// Was set to true. Returns true if the file save was successful. On error it will
// Print an error message by default
bool RestoreAndSaveEditorSandboxFile(EditorState* editor_state, bool previous_state);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeded. On error it will print an error message
bool SaveEditorSandboxFile(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------