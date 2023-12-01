#include "editorpch.h"
#include "ECSEngineUI.h"
#include "ECSEngineComponents.h"
#include "../Editor/EditorState.h"
#include "../Assets/PrefabFile.h"
#include "DragTargets.h"
#include "Scene.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/Sandbox.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Assets/AssetManagement.h"

ECS_TOOLS;

#define CIRCLE_RADIUS 5.0f

struct PrefabDragCallbackData {
	EditorState* editor_state;
	unsigned int hovered_sandbox_index;
	// This is the entity created inside that sandbox
	Entity entity;

	// This can be updated from the main thread to
	// Signal to the background thread to terminate itself
	std::atomic<bool> cancel_call;
	std::atomic<LOAD_EDITOR_ASSETS_STATE> load_assets_success;
	bool read_success;
	// This sandbox is a temporary sandbox where the asset load is happening
	unsigned int load_sandbox;
};

static void DeallocatePrefabDragCallbackData(PrefabDragCallbackData* data) {
	DestroySandbox(data->editor_state, data->load_sandbox, false, true);
}

static void FileExplorerPrefabDragCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	PrefabDragCallbackData* data = (PrefabDragCallbackData*)_data;
	Stream<char>* region_name = (Stream<char>*)_additional_data;

	EditorState* editor_state = data->editor_state;
	if (data->read_success && data->load_assets_success.load(ECS_RELAXED) == LOAD_EDITOR_ASSETS_SUCCESS) {
		auto set_prefab_position = [&](unsigned int sandbox_index) {
			Translation* entity_translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, data->entity);
			if (entity_translation != nullptr) {
				Camera scene_camera = GetSandboxCamera(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
				uint2 window_dimensions = system->GetWindowTexelSize(window_index);
				int2 window_pixel_positions = system->GetWindowTexelPositionEx(window_index, mouse_position);
				float3 direction = ViewportToWorldRayDirection(&scene_camera, window_dimensions, window_pixel_positions);
				entity_translation->value = scene_camera.translation + direction * CIRCLE_RADIUS;
				RenderSandboxViewports(editor_state, sandbox_index);
				system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
			}
		};

		auto add_prefab_to_sandbox = [&](unsigned int sandbox_index) {
			// We need to add the entity and the asset handles into the database reference
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			EntityManager* active_entity_manager = ActiveEntityManager(editor_state, sandbox_index);
			CapacityStream<Entity> _created_entity(&data->entity, 0, 1);
			CapacityStream<Entity>* created_entity = &_created_entity;

			const EditorSandbox* load_sandbox = GetSandbox(editor_state, data->load_sandbox);
			active_entity_manager->AddEntityManager(&load_sandbox->scene_entities, created_entity);
			// Use the increment main database as well since the DeleteEntity when the drag
			// Exists a window will decrement the reference counts from the main database as well
			sandbox->database.AddOther(&load_sandbox->database, true);

			set_prefab_position(sandbox_index);
			data->hovered_sandbox_index = sandbox_index;
		};

		if (*region_name == SCENE_UI_DRAG_NAME) {
			// Determine which scene window we are hovering
			unsigned int sandbox_index = SceneUITargetSandbox(editor_state, window_index);
			ECS_ASSERT(sandbox_index != -1);

			if (data->hovered_sandbox_index == -1) {
				// We did not hover a scene before and now we do, insert this prefab into the world
				add_prefab_to_sandbox(sandbox_index);
			}
			else {
				if (data->hovered_sandbox_index == sandbox_index) {
					// The same sandbox is hovered
					set_prefab_position(sandbox_index);
				}
				else {
					// We hovered a different sandbox - delete the old entity, which removes the
					// asset handles as well and add it to the new sandbox
					DeleteSandboxEntity(editor_state, data->hovered_sandbox_index, data->entity);
					add_prefab_to_sandbox(sandbox_index);
				}
			}
		}
		else if (*region_name == Stream<char>()) {
			DeleteSandboxEntity(editor_state, data->hovered_sandbox_index, data->entity);
			data->hovered_sandbox_index = -1;
			data->entity = -1;
		}
	}
}

EDITOR_EVENT(PreloadAssetsFinishEvent) {
	PrefabDragCallbackData* data = (PrefabDragCallbackData*)_data;
	if (data->cancel_call.load(ECS_RELAXED)) {
		DeallocatePrefabDragCallbackData(data);
		editor_state->ui_system->EndDragDrop(PREFAB_DRAG_NAME);
	}
	else {
		LOAD_EDITOR_ASSETS_STATE load_state = data->load_assets_success.load(ECS_RELAXED);
		data->read_success = load_state == LOAD_EDITOR_ASSETS_SUCCESS;
		if (load_state == LOAD_EDITOR_ASSETS_FAILURE) {
			Stream<wchar_t> stem = PathStem(editor_state->file_explorer_data->selected_files[0]);
			ECS_FORMAT_TEMP_STRING(message, "Failed to load prefab {#} assets while dragging", stem);
			EditorSetConsoleError(message);
		}
	}
	return false;
}

EDITOR_EVENT(LoadPrefabFileDeferred) {
	PrefabDragCallbackData* data = (PrefabDragCallbackData*)_data;

	if (!data->cancel_call) {
		if (IsSandboxLocked(data->editor_state, data->load_sandbox)) {
			return true;
		}

		data->read_success = ReadPrefabFile(
			editor_state,
			data->load_sandbox,
			editor_state->file_explorer_data->selected_files[0],
			&data->entity
		);
		if (data->read_success) {
			// We need to load its assets
			LoadEditorAssetsOptionalData optional_data;
			optional_data.callback = PreloadAssetsFinishEvent;
			optional_data.callback_data = data;
			optional_data.callback_data_size = 0;
			optional_data.state = &data->load_assets_success;
			LoadSandboxAssets(editor_state, data->load_sandbox, &optional_data);
		}
		else {
			Stream<wchar_t> stem = PathStem(editor_state->file_explorer_data->selected_files[0]);
			ECS_FORMAT_TEMP_STRING(message, "Failed to load prefab {#} while dragging", stem);
			EditorSetConsoleError(message);
		}
	}
	else {
		DeallocatePrefabDragCallbackData(data);
		editor_state->ui_system->EndDragDrop(PREFAB_DRAG_NAME);
	}
	return false;
}

bool IsPrefabFileSelected(const EditorState* editor_state) {
	const FileExplorerData* explorer_data = editor_state->file_explorer_data;
	if (explorer_data->selected_files.size == 1) {
		Stream<wchar_t> file = explorer_data->selected_files[0];
		if (PathExtension(file) == PREFAB_FILE_EXTENSION) {
			return true;
		}
	}
	return false;
}

void PrefabStartDrag(EditorState* editor_state) {
	if (IsPrefabFileSelected(editor_state)) {
		PrefabDragCallbackData callback_data;
		callback_data.editor_state = editor_state;
		callback_data.hovered_sandbox_index = -1;
		callback_data.entity = -1;
		callback_data.cancel_call.store(false, ECS_RELAXED);
		callback_data.load_sandbox = CreateSandboxTemporary(editor_state);
		callback_data.read_success = false;

		// If the sandbox is locked, then we will have to wait for the initialization
		// To finish. That is signaled by the fact that the sandbox is unlocked. So push
		// An event to monitor that. Also, start the drag drop since the EndDrag
		// Will try to end it
		PrefabDragCallbackData* allocated_data = (PrefabDragCallbackData*)editor_state->ui_system->StartDragDrop(
			{ FileExplorerPrefabDragCallback, &callback_data, sizeof(callback_data) },
			PREFAB_DRAG_NAME,
			true,
			true
		);
		EditorAddEvent(editor_state, LoadPrefabFileDeferred, allocated_data, 0);
	}
}

void PrefabEndDrag(EditorState* editor_state) {
	if (IsPrefabFileSelected(editor_state)) {
		PrefabDragCallbackData* callback_data = (PrefabDragCallbackData*)editor_state->ui_system->GetDragDropData(PREFAB_DRAG_NAME);
		// We need to deallocate this in case the asset load has finished, else we just need to signal
		// To the event to finish that
		if (!callback_data->read_success || callback_data->load_assets_success.load(ECS_RELAXED) != LOAD_EDITOR_ASSETS_PENDING) {
			if (callback_data->read_success) {
				// If we succeeded, and we have a valid entity entry, make it the selected one
				// For that sandbox
				if (callback_data->entity.IsValid() && callback_data->hovered_sandbox_index != -1) {
					ChangeSandboxSelectedEntities(editor_state, callback_data->hovered_sandbox_index, { &callback_data->entity, 1 });
					ChangeInspectorEntitySelection(editor_state, callback_data->hovered_sandbox_index);
				}
			}
			DeallocatePrefabDragCallbackData(callback_data);
			editor_state->ui_system->EndDragDrop(PREFAB_DRAG_NAME);
		}
		else {
			callback_data->cancel_call.store(true, ECS_RELAXED);
		}
	}
}