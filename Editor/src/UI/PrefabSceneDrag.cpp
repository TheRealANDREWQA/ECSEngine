#include "editorpch.h"
#include "ECSEngineUI.h"
#include "ECSEngineComponents.h"
#include "../Editor/EditorState.h"
#include "../Assets/Prefab.h"
#include "DragTargets.h"
#include "Scene.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/Sandbox.h"

ECS_TOOLS;

#define CIRCLE_RADIUS 5.0f

struct PrefabDragCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	// This is the entity created inside that sandbox
	Entity entity;

	MemoryManager temporary_allocator;
	EntityManager* temporary_entity_manager;
};

static void FileExplorerPrefabDragCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	PrefabDragCallbackData* data = (PrefabDragCallbackData*)_data;
	Stream<char>* region_name = (Stream<char>*)_additional_data;

	EditorState* editor_state = data->editor_state;
	if (*region_name == SCENE_UI_DRAG_NAME) {
		// Determine which scene window we are hovering
		unsigned int sandbox_index = SceneUITargetSandbox(editor_state, window_index);
		ECS_ASSERT(sandbox_index != -1);

		if (data->sandbox_index == -1) {
			// We did not hover a scene before and now we do, insert this prefab into the world
			bool success = AddPrefabToSandbox(editor_state, sandbox_index, editor_state->file_explorer_data->selected_files[0], &data->entity);
			if (!success) {
				Stream<wchar_t> stem = PathStem(editor_state->file_explorer_data->selected_files[0]);
				ECS_FORMAT_TEMP_STRING(message, "Failed to add prefab {#} to sandbox {#}", stem, sandbox_index);
				EditorSetConsoleError(message);
				data->entity = -1;
			}
			else {
				Translation* entity_translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, data->entity);
				if (entity_translation != nullptr) {
					Camera scene_camera = GetSandboxCamera(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
					float3 camera_forward = scene_camera.GetForwardVector().AsFloat3Low();
					entity_translation->value = scene_camera.translation + camera_forward * CIRCLE_RADIUS;
				}
				// Increment this since the prefab assets push an event and the render sandbox viewports
				// Might not catch that and would render with the current state
				EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
				RenderSandboxViewports(editor_state, sandbox_index);
				EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
			}
			// Update the sandbox index even when we failed. Most likely the file
			// Is corrupt and having to retry every single time while it is being
			// hovered won't bring any benefit
			data->sandbox_index = sandbox_index;
		}
		else {
			if (data->sandbox_index == sandbox_index) {
				if (data->entity.IsValid()) {
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
				}
			}
		}
	}
	else if (*region_name == Stream<char>()) {
		DeleteSandboxEntity(editor_state, data->sandbox_index, data->entity);
		data->sandbox_index = -1;
		data->entity = -1;
	}
}

ECS_THREAD_TASK(PreloadPrefabTask) {
	PrefabDragCallbackData* data = (PrefabDragCallbackData*)_data;
	//ReadPrefabFile(data->editor_state, )
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
		callback_data.sandbox_index = -1;
		callback_data.entity = -1;
		callback_data.temporary_allocator = MemoryManager(ECS_MB * 500, ECS_KB * 4, ECS_GB, { nullptr });
		callback_data.temporary_entity_manager = (EntityManager*)callback_data.temporary_allocator.Allocate(sizeof(EntityManager));

		editor_state->ui_system->StartDragDrop({ FileExplorerPrefabDragCallback, &callback_data, sizeof(callback_data) }, PREFAB_DRAG_NAME, true, true);
	}
}

void PrefabEndDrag(EditorState* editor_state) {
	if (IsPrefabFileSelected(editor_state)) {
		editor_state->ui_system->EndDragDrop(PREFAB_DRAG_NAME);
	}
}