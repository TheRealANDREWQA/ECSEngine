#pragma once

struct EditorState;

enum EDITOR_INPUT_MAPPING : unsigned char {
	EDITOR_INPUT_NEW_PROJECT,
	EDITOR_INPUT_OPEN_PROJECT,
	EDITOR_INPUT_SAVE_PROJECT,
	EDITOR_INPUT_MAPPING_COUNT
};

void InitializeInputMapping(EditorState* editor_state);

// Handles all the editor related input
void TickEditorInput(EditorState* editor_state);