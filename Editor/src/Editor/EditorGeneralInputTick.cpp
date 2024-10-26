#include "editorpch.h"
#include "EditorGeneralInputTick.h"
#include "EditorInputMapping.h"
#include "EditorState.h"
#include "../UI/CreateScene.h"
#include "../Sandbox/SandboxAccessor.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/Sandbox.h"
#include "../UI/Inspector.h"

void TickEditorGeneralInput(EditorState* editor_state) {
	const InputMapping& input_mapping = editor_state->input_mapping;

	if (input_mapping.IsTriggered(EDITOR_INPUT_SAVE_PROJECT)) {
		// Determine which sandboxes are dirty. If there is a single one dirty,
		// Then save it directly without a prompt. Otherwise use the SaveScenePopUp
		// to inform the user about which sandbox to save
		ECS_STACK_CAPACITY_STREAM(unsigned int, dirty_sandboxes, 512);
		// Exclude temporary sandboxes from consideration
		unsigned int sandbox_count = GetSandboxCount(editor_state, true);
		for (unsigned int index = 0; index < sandbox_count; index++) {
			const EditorSandbox* sandbox = GetSandbox(editor_state, index);
			// Exclude running or paused sandboxes - consider only scene sandboxes
			if (sandbox->is_scene_dirty && sandbox->run_state == EDITOR_SANDBOX_SCENE) {
				dirty_sandboxes.AddAssert(index);
			}
		}

		if (dirty_sandboxes.size == 1) {
			// Save it directly
			bool success = SaveSandboxScene(editor_state, dirty_sandboxes[0]);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(message, "Failed to save scene for sandbox {#}", dirty_sandboxes[0]);
				EditorSetConsoleError(message);
			}
		}
		else {
			CreateSaveScenePopUp(editor_state, dirty_sandboxes);
		}
	}

	if (input_mapping.IsTriggered(EDITOR_INPUT_PLAY_SIMULATIONS)) {
		StartEndSandboxWorld(editor_state);
	}

	if (input_mapping.IsTriggered(EDITOR_INPUT_PAUSE_SIMULATIONS)) {
		PauseUnpauseSandboxWorlds(editor_state);
	}

	if (input_mapping.IsTriggered(EDITOR_INPUT_STEP_SIMULATIONS)) {
		RunSandboxWorlds(editor_state, true);
	}

	if (input_mapping.IsTriggered(EDITOR_INPUT_PLAY_CURRENT_SANDBOX)) {
		unsigned int active_sandbox = GetActiveSandbox(editor_state);
		if (active_sandbox != -1) {
			if (GetSandboxState(editor_state, active_sandbox) == EDITOR_SANDBOX_SCENE) {
				StartSandboxWorld(editor_state, active_sandbox);
			}
			else {
				EndSandboxWorldSimulation(editor_state, active_sandbox);
			}
		}
	}

	if (input_mapping.IsTriggered(EDITOR_INPUT_PAUSE_CURRENT_SANDBOX)) {
		unsigned int active_sandbox = GetActiveSandbox(editor_state);
		if (active_sandbox != -1) {
			if (GetSandboxState(editor_state, active_sandbox) == EDITOR_SANDBOX_RUNNING) {
				PauseSandboxWorld(editor_state, active_sandbox);
			}
			else if (GetSandboxState(editor_state, active_sandbox) == EDITOR_SANDBOX_PAUSED) {
				StartSandboxWorld(editor_state, active_sandbox);
			}
		}
	}

	if (input_mapping.IsTriggered(EDITOR_INPUT_STEP_CURRENT_SANDBOX)) {
		unsigned int active_sandbox = GetActiveSandbox(editor_state);
		if (active_sandbox != -1) {
			if (GetSandboxState(editor_state, active_sandbox) == EDITOR_SANDBOX_PAUSED) {
				RunSandboxWorld(editor_state, active_sandbox, true, true);
			}
		}
	}

	const Keyboard* keyboard = editor_state->Keyboard();
	// Handle sandbox keyboard focus
	if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
		SandboxAction(editor_state, -1, [editor_state, keyboard](unsigned int sandbox_index) {
			if (keyboard->IsPressed((ECS_KEY)(ECS_KEY_1 + sandbox_index))) {
				// Change the focus to this sandbox
				FocusUIOnSandbox(editor_state, sandbox_index);
			}
		});
	}

	// Check the sandbox settings shortcut
	bool is_open_settings_mouse = input_mapping.IsTriggered(EDITOR_INPUT_OPEN_SANDBOX_SETTINGS_WITH_MOUSE);
	if (input_mapping.IsTriggered(EDITOR_INPUT_OPEN_SANDBOX_SETTINGS) || is_open_settings_mouse) {
		unsigned int active_sandbox = GetActiveSandboxIncludeScene(editor_state);
		if (active_sandbox != -1) {
			ChangeInspectorToSandboxSettings(editor_state, -1, active_sandbox);
		}
		else if (is_open_settings_mouse) {
			// If the mouse shortcut was the one selected, check the hovered sandbox as well
			active_sandbox = GetHoveredSandboxIncludeScene(editor_state);
			if (active_sandbox != -1) {
				ChangeInspectorToSandboxSettings(editor_state, -1, active_sandbox);
			}
		}
	}
}