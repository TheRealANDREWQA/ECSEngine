#pragma once

struct EditorState;

enum EDITOR_INPUT_MAPPING : unsigned char {
	EDITOR_INPUT_NEW_PROJECT,
	EDITOR_INPUT_OPEN_PROJECT,
	EDITOR_INPUT_SAVE_PROJECT,
	EDITOR_INPUT_CHANGE_TO_TRANSLATION_TOOL,
	EDITOR_INPUT_CHANGE_TO_ROTATION_TOOL,
	EDITOR_INPUT_CHANGE_TO_SCALE_TOOL,
	EDITOR_INPUT_AXIS_X,
	EDITOR_INPUT_AXIS_Y,
	EDITOR_INPUT_AXIS_Z,
	EDITOR_INPUT_INITIATE_TRANSLATION,
	EDITOR_INPUT_INITIATE_ROTATION,
	EDITOR_INPUT_INITIATE_SCALE,
	EDITOR_INPUT_CHANGE_TRANSFORM_SPACE,
	EDITOR_INPUT_MAPPING_COUNT
};

void InitializeInputMapping(EditorState* editor_state);