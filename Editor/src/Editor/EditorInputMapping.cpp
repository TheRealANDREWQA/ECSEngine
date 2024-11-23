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
	for (size_t index = 0; index < EDITOR_INPUT_MAPPING_COUNT; index++) {
		mappings[index] = InputMappingElement{};
	}

	mappings[EDITOR_INPUT_NEW_PROJECT].first.is_key = true;
	mappings[EDITOR_INPUT_NEW_PROJECT].first.key = ECS_KEY_LEFT_CTRL;
	mappings[EDITOR_INPUT_NEW_PROJECT].first.state = ECS_BUTTON_HELD;
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
	mappings[EDITOR_INPUT_INITIATE_TRANSLATION].SetExcludeCtrl();
	mappings[EDITOR_INPUT_INITIATE_ROTATION].SetFirstKeyPressed(ECS_KEY_R);
	mappings[EDITOR_INPUT_INITIATE_ROTATION].SetExcludeCtrl();
	mappings[EDITOR_INPUT_INITIATE_SCALE].SetFirstKeyPressed(ECS_KEY_S);
	mappings[EDITOR_INPUT_INITIATE_SCALE].SetExcludeCtrl();

	mappings[EDITOR_INPUT_CHANGE_TRANSFORM_SPACE].SetFirstKeyPressed(ECS_KEY_L);
	mappings[EDITOR_INPUT_CHANGE_TRANSFORM_SPACE].SetExcludeCtrl();
	mappings[EDITOR_INPUT_FOCUS_OBJECT].SetFirstKeyPressed(ECS_KEY_F);
	mappings[EDITOR_INPUT_FOCUS_OBJECT].SetExcludeCtrl();

	mappings[EDITOR_INPUT_CAMERA_WALK].SetFirstKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_CAMERA_WALK].SetSecondKey(ECS_KEY_SHARP_QUOTE, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_WASD_W].SetFirstKey(ECS_KEY_W, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_WASD_A].SetFirstKey(ECS_KEY_A, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_WASD_S].SetFirstKey(ECS_KEY_S, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_WASD_D].SetFirstKey(ECS_KEY_D, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_WASD_INCREASE_SPEED].SetFirstKeyPressed(ECS_KEY_E);
	mappings[EDITOR_INPUT_WASD_DECREASE_SPEED].SetFirstKeyPressed(ECS_KEY_Q);
	mappings[EDITOR_INPUT_WASD_RESET_SPEED].SetFirstKeyPressed(ECS_KEY_R);

	mappings[EDITOR_INPUT_SANDBOX_STATISTICS_TOGGLE].SetCtrlWith(ECS_KEY_P, ECS_BUTTON_PRESSED);

	mappings[EDITOR_INPUT_INSPECTOR_PREVIOUS_TARGET].SetCtrlWith(ECS_KEY_LEFT, ECS_BUTTON_PRESSED);
	mappings[EDITOR_INPUT_INSPECTOR_NEXT_TARGET].SetCtrlWith(ECS_KEY_RIGHT, ECS_BUTTON_PRESSED);

	mappings[EDITOR_INPUT_PLAY_SIMULATIONS].SetFirstKeyPressed(ECS_KEY_F5).SetExcludeCtrl();
	mappings[EDITOR_INPUT_PAUSE_SIMULATIONS].SetFirstKeyPressed(ECS_KEY_F6).SetExcludeCtrl();
	mappings[EDITOR_INPUT_STEP_SIMULATIONS].SetFirstKeyPressed(ECS_KEY_F7).SetExcludeCtrl();

	mappings[EDITOR_INPUT_PLAY_CURRENT_SANDBOX].SetCtrlWith(ECS_KEY_F5, ECS_BUTTON_PRESSED);
	mappings[EDITOR_INPUT_PAUSE_CURRENT_SANDBOX].SetCtrlWith(ECS_KEY_F6, ECS_BUTTON_PRESSED);
	mappings[EDITOR_INPUT_STEP_CURRENT_SANDBOX].SetCtrlWith(ECS_KEY_F7, ECS_BUTTON_PRESSED);

	mappings[EDITOR_INPUT_OPEN_SANDBOX_SETTINGS].SetCtrlWith(ECS_KEY_M, ECS_BUTTON_PRESSED);
	
	mappings[EDITOR_INPUT_OPEN_SANDBOX_SETTINGS_WITH_MOUSE].SetFirstKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_OPEN_SANDBOX_SETTINGS_WITH_MOUSE].SetSecondMouse(ECS_MOUSE_X1, ECS_BUTTON_HELD);

	mappings[EDITOR_INPUT_DUPLICATE_SANDBOX].SetFirstKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_DUPLICATE_SANDBOX].SetSecondKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD);
	mappings[EDITOR_INPUT_DUPLICATE_SANDBOX].SetThirdKey(ECS_KEY_D, ECS_BUTTON_HELD);

	editor_state->input_mapping.ChangeMapping(mappings);
}