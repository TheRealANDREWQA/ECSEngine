#include "editorpch.h"
#include "SandboxUI.h"
#include "Game.h"
#include "Scene.h"
#include "Inspector.h"
#include "EntitiesUI.h"
#include "../Editor/EditorState.h"
#include "Common.h"
#include "../Sandbox/SandboxAccessor.h"

void DuplicateSandboxUIForDifferentSandbox(
	EditorState* editor_state,
	unsigned int source_sandbox_handle,
	unsigned int destination_sandbox_handle,
	bool split_region
) {
	// The windows that we need to duplicate are: Scene, Game, Entities (those that target the given sandbox)
	// And the inspector

	auto handle_window_type = [editor_state, split_region](unsigned int target_window_index, auto create_window) {
		if (target_window_index != -1) {
			unsigned int created_window_index = create_window();
			if (split_region) {
				// Determine which dimension is larger, and split along that dimension
				float2 scene_window_scale = editor_state->ui_system->GetWindowScale(target_window_index);
				ECS_UI_BORDER_TYPE split_type = ECS_UI_BORDER_RIGHT;
				if (scene_window_scale.y > scene_window_scale.x) {
					split_type = ECS_UI_BORDER_BOTTOM;
				}
				editor_state->ui_system->SplitDockspaceRegion(target_window_index, split_type, created_window_index);
			}
			else {
				editor_state->ui_system->AddWindowToDockspaceRegion(created_window_index, target_window_index);
			}
		}
	};

	// The scene window
	handle_window_type(GetSceneUIWindowIndex(editor_state, source_sandbox_handle), [&]() {
		return CreateSceneUIWindowOnly(editor_state, destination_sandbox_handle);
	});

	// The game window
	handle_window_type(GetGameUIWindowIndex(editor_state, source_sandbox_handle), [&]() {
		return CreateGameUIWindowOnly(editor_state, destination_sandbox_handle);
	});

	// The entities window
	unsigned int last_entities_index = GetEntitiesUILastWindowIndex(editor_state);
	if (last_entities_index != -1) {
		unsigned int create_window_count = last_entities_index + 1;
		for (unsigned int index = 0; index <= last_entities_index; index++) {
			if (GetEntitiesUITargetSandbox(editor_state, index) == source_sandbox_handle) {
				handle_window_type(GetEntitiesUIWindowIndex(editor_state, index), [&]() {
					unsigned int new_ui_window_index = CreateEntitiesUIWindow(editor_state, create_window_count);
					create_window_count++;
					SetEntitiesUITargetSandbox(editor_state, new_ui_window_index, destination_sandbox_handle);
					return new_ui_window_index;
				});
			}
		}
	}

	// The inspector window
	ECS_STACK_CAPACITY_STREAM(unsigned int, inspector_indices, 128);
	// This call includes both the inspectors that target the source sandbox directly,
	// And those that accept any sandbox
	GetInspectorsForMatchingSandbox(editor_state, source_sandbox_handle, &inspector_indices);

	for (size_t index = 0; index < inspector_indices.size; index++) {
		unsigned int created_inspector_index = -1;
		handle_window_type(GetInspectorUIWindowIndex(editor_state, inspector_indices[index]), [&]() {
			created_inspector_index = CreateInspectorInstance(editor_state);
			return CreateInspectorWindow(editor_state, created_inspector_index);
		});

		// We need to change the target sandbox of the created inspectors, but the set function
		// Will change the focus of the last window created, which requires the dockspace be created
		// For that window. For this reason, perform the set after the function returns
		if (created_inspector_index != -1) {
			SetInspectorMatchingSandbox(editor_state, created_inspector_index, destination_sandbox_handle);
		}
	}
}

void DestroySandboxWindows(EditorState* editor_state, unsigned int sandbox_handle)
{
	Stream<char> sandbox_name = GetSandbox(editor_state, sandbox_handle)->name;
	unsigned int sandbox_display_index = ConvertCharactersToInt(sandbox_name);

	// Destroy only the windows for that sandbox index
	DestroyIndexedWindows(editor_state, GAME_WINDOW_NAME, sandbox_display_index, sandbox_display_index + 1);
	DestroyIndexedWindows(editor_state, SCENE_WINDOW_NAME, sandbox_display_index, sandbox_display_index + 1);
}

void DestroySandboxAuxiliaryWindows(EditorState* editor_state, unsigned int sandbox_handle)
{
	// Destroy the inspectors that match this sandbox
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (GetInspectorMatchingSandbox(editor_state, index) == sandbox_handle) {
			DestroyInspectorInstanceWithUI(editor_state, index);
			index--;
		}
	}

	// Destroy the entities UI windows. Add +1 to get the actual count, works for the -1 case
	unsigned int entities_window_count = GetEntitiesUILastWindowIndex(editor_state) + 1;
	for (unsigned int index = 0; index < entities_window_count; index++) {
		if (GetEntitiesUITargetSandbox(editor_state, index) == sandbox_handle) {
			editor_state->ui_system->DestroyWindowEx(GetEntitiesUIWindowIndex(editor_state, index));
			index--;
		}
	}
}