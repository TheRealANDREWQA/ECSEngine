#include "editorpch.h"
#include "EditorGeneralInputTick.h"
#include "EditorInputMapping.h"
#include "EditorState.h"
#include "../UI/CreateScene.h"
#include "../Sandbox/SandboxAccessor.h"
#include "../Sandbox/SandboxScene.h"

void TickEditorGeneralInput(EditorState* editor_state) {
	if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_SAVE_PROJECT)) {
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
}