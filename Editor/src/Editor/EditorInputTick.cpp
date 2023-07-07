#include "editorpch.h"
#include "EditorInputTick.h"
#include "EditorState.h"

using namespace ECSEngine;

void TickEditorInput(EditorState* editor_state) {
	Mouse* mouse = editor_state->Mouse();
	Keyboard* keyboard = editor_state->Keyboard();

	unsigned int sandbox_count = GetSandboxCount(editor_state);
	if (sandbox_count > 0) {
		EditorSandbox* sandbox = GetSandbox(editor_state, 0);
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
			RenderSandbox(editor_state, 0, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
		}
	}
}