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
	EDITOR_INPUT_CAMERA_WALK,
	EDITOR_INPUT_WASD_W,
	EDITOR_INPUT_WASD_A,
	EDITOR_INPUT_WASD_S,
	EDITOR_INPUT_WASD_D,
	
	// The WASD increase/decrease speed have 2 modes
	// When shift is pressed, the speed will double/halve
	// When shift is not pressed, the speed will increase/decrease
	// with a constant step

	EDITOR_INPUT_WASD_INCREASE_SPEED,
	EDITOR_INPUT_WASD_DECREASE_SPEED,
	EDITOR_INPUT_WASD_RESET_SPEED,
	EDITOR_INPUT_FOCUS_OBJECT,
	EDITOR_INPUT_SANDBOX_STATISTICS_TOGGLE,
	EDITOR_INPUT_MAPPING_COUNT
};

void InitializeInputMapping(EditorState* editor_state);