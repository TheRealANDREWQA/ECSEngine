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

	mappings[EDITOR_INPUT_TRANSLATION_TOOL].first.key = ECS_KEY_1;
	mappings[EDITOR_INPUT_TRANSLATION_TOOL].first.is_key = true;
	mappings[EDITOR_INPUT_TRANSLATION_TOOL].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_ROTATION_TOOL].first.key = ECS_KEY_2;
	mappings[EDITOR_INPUT_ROTATION_TOOL].first.is_key = true;
	mappings[EDITOR_INPUT_ROTATION_TOOL].first.state = ECS_BUTTON_PRESSED;

	mappings[EDITOR_INPUT_SCALE_TOOL].first.key = ECS_KEY_3;
	mappings[EDITOR_INPUT_SCALE_TOOL].first.is_key = true;
	mappings[EDITOR_INPUT_SCALE_TOOL].first.state = ECS_BUTTON_PRESSED;

	for (size_t index = 0; index < EDITOR_INPUT_MAPPING_COUNT; index++) {
		editor_state->input_mapping.ChangeMapping(index, mappings[index]);
	}
}