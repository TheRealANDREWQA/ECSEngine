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
#include "../Assets/AssetManagement.h"

ECS_TOOLS;

#define CIRCLE_RADIUS 5.0f

struct PrefabDragCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	// This is the entity created inside that sandbox
	Entity entity;

	// This can be updated from the main thread to
	// Signal to the background thread to terminate itself
	std::atomic<bool> cancel_call;
	bool read_success;
	std::atomic<LOAD_EDITOR_ASSETS_STATE> load_assets_success;
	MemoryManager temporary_allocator;
	EntityManager temporary_entity_manager;
	AssetDatabaseReference database_reference;
};

static void DeallocatePrefabDragCallbackData(PrefabDragCallbackData* data) {
	if (data->read_success) {
		// Remove the assets from the database
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
			data->database_reference.ForEachAssetDuplicates(current_type, [data, current_type](unsigned int handle) {
				DecrementAssetReference(data->editor_state, handle, current_type);
			});
		}
	}
	data->temporary_allocator.Free();
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
			active_entity_manager->AddEntityManager(&data->temporary_entity_manager, created_entity);
			// Use the increment main database as well since the DeleteEntity when the drag
			// Exists a window will decrement the reference counts from the main database as well
			sandbox->database.AddOther(&data->database_reference, true);

			set_prefab_position(sandbox_index);
			data->sandbox_index = sandbox_index;
		};

		if (*region_name == SCENE_UI_DRAG_NAME) {
			// Determine which scene window we are hovering
			unsigned int sandbox_index = SceneUITargetSandbox(editor_state, window_index);
			ECS_ASSERT(sandbox_index != -1);

			if (data->sandbox_index == -1) {
				// We did not hover a scene before and now we do, insert this prefab into the world
				add_prefab_to_sandbox(sandbox_index);
			}
			else {
				if (data->sandbox_index == sandbox_index) {
					// The same sandbox is hovered
					set_prefab_position(sandbox_index);
				}
				else {
					// We hovered a different sandbox - delete the old entity, which removes the
					// asset handles as well and add it to the new sandbox
					DeleteSandboxEntity(editor_state, data->sandbox_index, data->entity);
					add_prefab_to_sandbox(sandbox_index);
				}
			}
		}
		else if (*region_name == Stream<char>()) {
			DeleteSandboxEntity(editor_state, data->sandbox_index, data->entity);
			data->sandbox_index = -1;
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
		callback_data.cancel_call.store(false, ECS_RELAXED);

		// We need to use the allocator after it was allocated since the entity manager
		// And the database reference will reference its address
		PrefabDragCallbackData* allocated_data = (PrefabDragCallbackData*)editor_state->ui_system->StartDragDrop(
			{ FileExplorerPrefabDragCallback, &callback_data, sizeof(callback_data) }, 
			PREFAB_DRAG_NAME, 
			true, 
			true
		);
		allocated_data->temporary_allocator = MemoryManager(ECS_MB * 500, ECS_KB * 4, ECS_GB, { nullptr });
		allocated_data->database_reference = AssetDatabaseReference(editor_state->asset_database, GetAllocatorPolymorphic(&allocated_data->temporary_allocator));
		EntityPool* allocated_entity_pool = (EntityPool*)allocated_data->temporary_allocator.Allocate(sizeof(EntityPool));
		*allocated_entity_pool = EntityPool(&allocated_data->temporary_allocator, 4);
		EntityManagerDescriptor manager_descriptor;
		manager_descriptor.deferred_action_capacity = 0;
		manager_descriptor.entity_pool = allocated_entity_pool;
		manager_descriptor.memory_manager = &allocated_data->temporary_allocator;
		allocated_data->temporary_entity_manager = EntityManager(manager_descriptor);

		allocated_data->read_success = ReadPrefabFile(
			editor_state,
			&allocated_data->temporary_entity_manager,
			&allocated_data->database_reference,
			editor_state->file_explorer_data->selected_files[0],
			&allocated_data->entity
		);
		if (allocated_data->read_success) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
			Stream<Stream<unsigned int>> handles = allocated_data->database_reference.GetUniqueHandles(GetAllocatorPolymorphic(&stack_allocator));
			// We need to filter the already loaded assets
			handles = FilterUnloadedAssets(editor_state, handles);

			// We need to load its assets
			LoadEditorAssetsOptionalData optional_data;
			optional_data.callback = PreloadAssetsFinishEvent;
			optional_data.callback_data = allocated_data;
			optional_data.callback_data_size = 0;
			optional_data.state = &allocated_data->load_assets_success;
			optional_data.update_link_entity_manager = &allocated_data->temporary_entity_manager;
			LoadEditorAssets(editor_state, handles, &optional_data);
		}
		else {
			Stream<wchar_t> stem = PathStem(editor_state->file_explorer_data->selected_files[0]);
			ECS_FORMAT_TEMP_STRING(message, "Failed to load prefab {#} while dragging", stem);
			EditorSetConsoleError(message);
		}
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
				if (callback_data->entity.IsValid() && callback_data->sandbox_index != -1) {
					ChangeSandboxSelectedEntities(editor_state, callback_data->sandbox_index, { &callback_data->entity, 1 });
					ChangeInspectorEntitySelection(editor_state, callback_data->sandbox_index);
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