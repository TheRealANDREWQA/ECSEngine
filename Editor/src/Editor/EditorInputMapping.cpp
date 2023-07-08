#include "editorpch.h"
#include "EditorInputMapping.h"
#include "EditorState.h"
#include "../UI/HelperWindows.h"

using namespace ECSEngine;

void InitializeInputMapping(EditorState* editor_state) {
	editor_state->input_mapping.Initialize(editor_state->EditorAllocator(), EDITOR_INPUT_MAPPING_COUNT);

	InputMappingElement mappings[EDITOR_INPUT_MAPPING_COUNT];
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

	for (size_t index = 0; index < EDITOR_INPUT_MAPPING_COUNT; index++) {
		editor_state->input_mapping.ChangeMapping(index, mappings[index]);
	}
}

void TickEditorInput(EditorState* editor_state) {
	Mouse* mouse = editor_state->Mouse();
	Keyboard* keyboard = editor_state->Keyboard();

	unsigned int sandbox_count = GetSandboxCount(editor_state);
	if (sandbox_count > 0) {
		unsigned int active_sandbox = GetActiveWindowSandbox(editor_state);
		if (active_sandbox != -1) {
			EditorSandbox* sandbox = GetSandbox(editor_state, active_sandbox);
			ECS_TRANSFORM_TOOL current_tool = sandbox->transform_tool;
			if (keyboard->IsPressed(ECS_KEY_1)) {
				sandbox->transform_tool = ECS_TRANSFORM_TRANSLATION;
			}
			else if (keyboard->IsPressed(ECS_KEY_2)) {
				sandbox->transform_tool = ECS_TRANSFORM_ROTATION;
			}
			else if (keyboard->IsPressed(ECS_KEY_3)) {
				sandbox->transform_tool = ECS_TRANSFORM_SCALE;
			}

			if (current_tool != sandbox->transform_tool) {
				RenderSandbox(editor_state, active_sandbox, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
			}
		}
	}
}