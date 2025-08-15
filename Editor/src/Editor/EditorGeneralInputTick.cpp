#include "editorpch.h"
#include "EditorGeneralInputTick.h"
#include "EditorInputMapping.h"
#include "EditorState.h"
#include "../UI/CreateScene.h"
#include "../Sandbox/SandboxAccessor.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/Sandbox.h"
#include "../UI/Inspector.h"
#include "../UI/SandboxUI.h"

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
		unsigned int active_sandbox = GetActiveSandbox(editor_state, true);
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
		unsigned int active_sandbox = GetActiveSandbox(editor_state, true);
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
		unsigned int active_sandbox = GetActiveSandbox(editor_state, true);
		if (active_sandbox != -1) {
			if (GetSandboxState(editor_state, active_sandbox) == EDITOR_SANDBOX_PAUSED) {
				RunSandboxWorld(editor_state, active_sandbox, true, true);
			}
		}
	}

	const Keyboard* keyboard = editor_state->Keyboard();
	// Handle sandbox keyboard focus
	if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
		SandboxAction(editor_state, -1, [editor_state, keyboard](unsigned int sandbox_handle) {
			if (keyboard->IsPressed((ECS_KEY)(ECS_KEY_1 + sandbox_handle))) {
				unsigned int active_sandbox = GetActiveSandbox(editor_state, true);
				// If the active sandbox is changed, defocus the previous sandbox
				if (active_sandbox != -1 && active_sandbox != sandbox_handle) {
					DefocusUIOnSandbox(editor_state, active_sandbox);
				}

				// Change the focus to this sandbox
				FocusUIOnSandbox(editor_state, sandbox_handle);
			}
		});
	}

	// Check the sandbox settings shortcut
	bool is_open_settings_mouse = input_mapping.IsTriggered(EDITOR_INPUT_OPEN_SANDBOX_SETTINGS_WITH_MOUSE);
	if (input_mapping.IsTriggered(EDITOR_INPUT_OPEN_SANDBOX_SETTINGS) || is_open_settings_mouse) {
		// Give priority to the hovered sandbox when using the mouse
		if (is_open_settings_mouse) {
			unsigned int active_sandbox = GetHoveredSandboxIncludeScene(editor_state);
			if (active_sandbox != -1) {
				ChangeInspectorToSandboxSettings(editor_state, -1, active_sandbox);
			}
			else {
				active_sandbox = GetActiveSandboxIncludeScene(editor_state);
				if (active_sandbox != -1) {
					ChangeInspectorToSandboxSettings(editor_state, -1, active_sandbox);
				}
			}
		}
		else {
			unsigned int active_sandbox = GetActiveSandboxIncludeScene(editor_state);
			if (active_sandbox != -1) {
				ChangeInspectorToSandboxSettings(editor_state, -1, active_sandbox);
			}
		}
	}

	// Check the sandbox duplicate shortcut
	if (input_mapping.IsTriggered(EDITOR_INPUT_DUPLICATE_SANDBOX)) {
		unsigned int active_sandbox = GetActiveSandboxIncludeScene(editor_state);
		if (active_sandbox != -1) {
			// This function will create only the backend sandbox, without any support UI
			unsigned int duplicated_sandbox = DuplicateSandbox(editor_state, active_sandbox);
			DuplicateSandboxUIForDifferentSandbox(editor_state, active_sandbox, duplicated_sandbox);
		}
	}

	// Check the sandbox close shortcut
	if (input_mapping.IsTriggered(EDITOR_INPUT_CLOSE_SANDBOX)) {
		unsigned int active_sandbox = GetActiveSandboxIncludeScene(editor_state);
		if (active_sandbox != -1) {
			// Destroy the auxiliary windows before destroying the sandbox itself
			DestroySandboxAuxiliaryWindows(editor_state, active_sandbox);
			DestroySandbox(editor_state, active_sandbox);
		}
	}

	if (input_mapping.IsTriggered(EDITOR_INPUT_MAKE_CURSOR_VISIBLE)) {
		// Besides making the cursor visible and disabling the raw input, don't forget to remove
		// The pause unpause editor state flags. Perform these actions only if the cursor is currently
		// Not visible, as clearing the pause unpause flags can mess up the state if it is already visible
		if (!editor_state->Mouse()->IsVisible()) {
			editor_state->Mouse()->SetCursorVisibility(true);
			editor_state->Mouse()->DisableRawInput();

			if (EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN)) {
				EditorStateClearFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN);
			}
			if (EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT)) {
				EditorStateClearFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT);
			}
		}
	}
}