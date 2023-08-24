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

	mappings[EDITOR_INPUT_CHANGE_TO_TRANSLATION_TOOL].SetFirstKeyPressed(ECS_KEY_1);
	mappings[EDITOR_INPUT_CHANGE_TO_ROTATION_TOOL].SetFirstKeyPressed(ECS_KEY_2);
	mappings[EDITOR_INPUT_CHANGE_TO_SCALE_TOOL].SetFirstKeyPressed(ECS_KEY_3);

	mappings[EDITOR_INPUT_AXIS_X].SetFirstKeyPressed(ECS_KEY_X);
	mappings[EDITOR_INPUT_AXIS_Y].SetFirstKeyPressed(ECS_KEY_Y);
	mappings[EDITOR_INPUT_AXIS_Z].SetFirstKeyPressed(ECS_KEY_Z);

	mappings[EDITOR_INPUT_INITIATE_TRANSLATION].SetFirstKeyPressed(ECS_KEY_G);
	mappings[EDITOR_INPUT_INITIATE_ROTATION].SetFirstKeyPressed(ECS_KEY_R);
	mappings[EDITOR_INPUT_INITIATE_SCALE].SetFirstKeyPressed(ECS_KEY_S);

	mappings[EDITOR_INPUT_CHANGE_TRANSFORM_SPACE].SetFirstKeyPressed(ECS_KEY_L);
	mappings[EDITOR_INPUT_FOCUS_OBJECT].SetFirstKeyPressed(ECS_KEY_F);

	mappings[EDITOR_INPUT_CAMERA_WALK].SetFirstKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_DOWN);
	mappings[EDITOR_INPUT_CAMERA_WALK].SetSecondKey(ECS_KEY_SHARP_QUOTE, ECS_BUTTON_DOWN);

	editor_state->input_mapping.ChangeMapping(mappings);
}