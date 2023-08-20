#include "editorpch.h"
#include "EditorInputMapping.h"
#include "EditorState.h"
#include "../UI/HelperWindows.h"

using namespace ECSEngine;

void InitializeInputMapping(EditorState* editor_state) {
	editor_state->input_mapping.Initialize(editor_state->EditorAllocator(), EDITOR_INPUT_MAPPING_COUNT);
	editor_state->input_mapping.mouse = editor_state->Mouse();
	editor_state->input_mapping.keyboard = editor_state->Keyboard();

	InputMappingElement mappings[EDITOR_INPUT_MAPPING_COUNT];
	memset(mappings, 0, sizeof(mappings));

	mappings[EDITOR_INPUT_NEW_PROJECT].first.is_key = true;
	mappings[EDITOR_INPUT_NEW_PROJECT].first.key = ECS_KEY_LEFT_CTRL;
	mappings[EDITOR_INPUT_NEW_PROJECT].first.state = ECS_BUTTON_DOWN;
	mappings[EDITOR_INPUT_NEW_PROJECT].second.is_key = true;
	mappings[EDITOR_INPUT_NEW_PROJECT].second.key = ECS_KEY_N;
	mappings[EDITOR_INPUT_NEW_PROJECT].second.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_OPEN_PROJECT] = mappings[EDITOR_INPUT_NEW_PROJECT];
	mappings[EDITOR_INPUT_OPEN_PROJECT].second.key = ECS_KEY_O;

	mappings[EDITOR_INPUT_SAVE_PROJECT] = mappings[EDITOR_INPUT_NEW_PROJECT];
	mappings[EDITOR_INPUT_SAVE_PROJECT].second.key = ECS_KEY_S;

	mappings[EDITOR_INPUT_CHANGE_TO_TRANSLATION_TOOL].first.key = ECS_KEY_1;
	mappings[EDITOR_INPUT_CHANGE_TO_TRANSLATION_TOOL].first.is_key = true;
	mappings[EDITOR_INPUT_CHANGE_TO_TRANSLATION_TOOL].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_CHANGE_TO_ROTATION_TOOL].first.key = ECS_KEY_2;
	mappings[EDITOR_INPUT_CHANGE_TO_ROTATION_TOOL].first.is_key = true;
	mappings[EDITOR_INPUT_CHANGE_TO_ROTATION_TOOL].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_CHANGE_TO_SCALE_TOOL].first.key = ECS_KEY_3;
	mappings[EDITOR_INPUT_CHANGE_TO_SCALE_TOOL].first.is_key = true;
	mappings[EDITOR_INPUT_CHANGE_TO_SCALE_TOOL].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_AXIS_X].first.key = ECS_KEY_X;
	mappings[EDITOR_INPUT_AXIS_X].first.is_key = true;
	mappings[EDITOR_INPUT_AXIS_X].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_AXIS_Y].first.key = ECS_KEY_Y;
	mappings[EDITOR_INPUT_AXIS_Y].first.is_key = true;
	mappings[EDITOR_INPUT_AXIS_Y].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_AXIS_Z].first.key = ECS_KEY_Z;
	mappings[EDITOR_INPUT_AXIS_Z].first.is_key = true;
	mappings[EDITOR_INPUT_AXIS_Z].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_INITIATE_TRANSLATION].first.key = ECS_KEY_G;
	mappings[EDITOR_INPUT_INITIATE_TRANSLATION].first.is_key = true;
	mappings[EDITOR_INPUT_INITIATE_TRANSLATION].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_INITIATE_ROTATION].first.key = ECS_KEY_R;
	mappings[EDITOR_INPUT_INITIATE_ROTATION].first.is_key = true;
	mappings[EDITOR_INPUT_INITIATE_ROTATION].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_INITIATE_SCALE].first.key = ECS_KEY_S;
	mappings[EDITOR_INPUT_INITIATE_SCALE].first.is_key = true;
	mappings[EDITOR_INPUT_INITIATE_SCALE].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_CHANGE_TRANSFORM_SPACE].first.key = ECS_KEY_L;
	mappings[EDITOR_INPUT_CHANGE_TRANSFORM_SPACE].first.is_key = true;
	mappings[EDITOR_INPUT_CHANGE_TRANSFORM_SPACE].first.state = ECS_BUTTON_PRESSED;

	for (size_t index = 0; index < EDITOR_INPUT_MAPPING_COUNT; index++) {
		editor_state->input_mapping.ChangeMapping(index, mappings[index]);
	}
}