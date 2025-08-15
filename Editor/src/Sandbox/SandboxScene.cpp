#include "editorpch.h"
#include "SandboxScene.h"
#include "Sandbox.h"
#include "SandboxEntityOperations.h"
#include "SandboxFile.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorScene.h"
#include "../Editor/EditorState.h"
#include "../UI/Game.h"
#include "../UI/Scene.h"

using namespace ECSEngine;

// -----------------------------------------------------------------------------------------------------------------------------

void CopySandboxScenePath(EditorState* editor_state, unsigned int source_sandbox_index, unsigned int destination_sandbox_index) {
	EditorSandbox* destination_sandbox = GetSandbox(editor_state, destination_sandbox_index);
	const EditorSandbox* source_sandbox = GetSandbox(editor_state, source_sandbox_index);

	ClearSandboxScene(editor_state, destination_sandbox_index);
	destination_sandbox->scene_path.size = 0;

	Stream<wchar_t> source_sandbox_path = source_sandbox->scene_path;
	if (source_sandbox_path.size == 0) {
		// Setting the components needs to be done right before exiting
		editor_state->editor_components.SetManagerComponents(editor_state, destination_sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		return;
	}

	// Copy the scene path now
	destination_sandbox->scene_path.CopyOther(source_sandbox_path);
	destination_sandbox->is_scene_dirty = false;

	// Copy the entities now
	destination_sandbox->scene_entities.CopyOther(&source_sandbox->scene_entities);
	// Update the sandbox asset references
	CopySandboxAssetReferences(editor_state, source_sandbox_index, destination_sandbox_index);

	editor_state->editor_components.SetManagerComponents(editor_state, destination_sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
}

// -----------------------------------------------------------------------------------------------------------------------------

struct ChangeSceneRenderViewportsEventData {
	unsigned int sandbox_handle;
};

static EDITOR_EVENT(ChangeSceneRenderViewportsEvent) {
	ChangeSceneRenderViewportsEventData* data = (ChangeSceneRenderViewportsEventData*)_data;

	// Trigger a rerender of the viewport
	RenderSandboxViewports(editor_state, data->sandbox_handle);
	return false;
}

bool ChangeSandboxScenePath(EditorState* editor_state, unsigned int sandbox_handle, Stream<wchar_t> new_scene)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	ClearSandboxScene(editor_state, sandbox_handle);
	sandbox->scene_path.size = 0;

	if (new_scene.size == 0) {
		// Setting the components needs to be done right before exiting
		editor_state->editor_components.SetManagerComponents(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		return true;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetScenePath(editor_state, new_scene, absolute_path);

	bool success = LoadEditorSceneCore(editor_state, sandbox_handle, absolute_path);
	if (success) {
		// Copy the contents
		sandbox->scene_path.CopyOther(new_scene);
		sandbox->is_scene_dirty = false;

		// Load the assets and trigger a re-render of the viewports
		ChangeSceneRenderViewportsEventData event_data = { sandbox_handle };
		LoadEditorAssetsOptionalData load_optional_data;
		load_optional_data.callback = ChangeSceneRenderViewportsEvent;
		load_optional_data.callback_data = &event_data;
		load_optional_data.callback_data_size = sizeof(event_data);
		LoadSandboxAssets(editor_state, sandbox_handle, &load_optional_data);
	}

	// If the load failed, the scene will be reset with an empty value
	editor_state->editor_components.SetManagerComponents(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearSandboxScene(EditorState* editor_state, unsigned int sandbox_handle)
{
	// Unload the assets currently used by this sandbox and reset the scene entities
	UnloadSandboxAssets(editor_state, sandbox_handle);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->scene_entities.Reset();
	sandbox->database.Reset();
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_handle)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->sandbox_world.entity_manager->CopyOther(&sandbox->scene_entities);
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySandboxRuntimeWorldFromOther(EditorState* editor_state, unsigned int source_sandbox_index, unsigned int destination_sandbox_index)
{
	const EditorSandbox* source_sandbox = GetSandbox(editor_state, source_sandbox_index);
	EditorSandbox* destination_sandbox = GetSandbox(editor_state, destination_sandbox_index);

	// Copy the world
	CopyWorld(&destination_sandbox->sandbox_world, &source_sandbox->sandbox_world);
	
	// We shouldn't change the sandbox asset references. The asset snapshot will take care of that.
	// Just clear the asset snapshot
	destination_sandbox->runtime_asset_handle_snapshot.allocator.Clear();
	destination_sandbox->runtime_asset_handle_snapshot.handle_count = 0;
	destination_sandbox->runtime_asset_handle_snapshot.handle_capacity = 0;

	// Re-render the sandbox viewports for the destination sandbox
	RenderSandboxViewports(editor_state, destination_sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool DefocusUIOnSandbox(EditorState* editor_state, unsigned int sandbox_handle) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	if (sandbox->before_play_active_window_in_border.size == 0) {
		return false;
	}

	unsigned int previously_focused_window = editor_state->ui_system->GetWindowFromName(sandbox->before_play_active_window_in_border);
	// Deallocate the window name for all branches
	sandbox->before_play_active_window_in_border.DeallocateWithReset(sandbox->GlobalMemoryManager());
	if (previously_focused_window == -1) {
		// We can also deallocate this name now
		return false;
	}

	unsigned int game_ui_index = GetGameUIWindowIndex(editor_state, sandbox_handle);
	if (game_ui_index != -1) {
		if (game_ui_index != previously_focused_window) {
			// Check to see if they are in the same dockspace region
			if (editor_state->ui_system->IsWindowInTheSameDockspaceRegion(game_ui_index, previously_focused_window)) {
				// We can change the focus back
				editor_state->ui_system->SetActiveWindowInBorder(previously_focused_window);
				return true;
			}
		}

		return false;
	}

	unsigned int scene_ui_index = GetGameUIWindowIndex(editor_state, sandbox_handle);
	if (scene_ui_index != -1) {
		if (scene_ui_index != previously_focused_window) {
			// Check to see if they are in the same dockspace region
			if (editor_state->ui_system->IsWindowInTheSameDockspaceRegion(scene_ui_index, previously_focused_window)) {
				// We can change the focus back
				editor_state->ui_system->SetActiveWindowInBorder(previously_focused_window);
				return true;
			}
		}
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool FocusUIOnSandbox(EditorState* editor_state, unsigned int sandbox_handle) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	unsigned int game_ui_index = GetGameUIWindowIndex(editor_state, sandbox_handle);
	if (game_ui_index != -1) {
		if (editor_state->ui_system->GetActiveWindow() != game_ui_index) {
			// Set the previously active window in the same border as this game window
			unsigned int previous_active_border_window = editor_state->ui_system->GetActiveWindowInsideBorderForWindow(game_ui_index);
			sandbox->before_play_active_window_in_border.DeallocateWithReset(sandbox->GlobalMemoryManager());
			// Allocate the buffer from the sandbox' global memory manager
			sandbox->before_play_active_window_in_border = editor_state->ui_system->GetWindowName(previous_active_border_window).Copy(sandbox->GlobalMemoryManager());

			editor_state->ui_system->SetActiveWindow(game_ui_index);
			return true;
		}
	}
	else {
		// Check the scene window
		unsigned int scene_ui_index = GetSceneUIWindowIndex(editor_state, sandbox_handle);
		if (scene_ui_index != -1) {
			if (editor_state->ui_system->GetActiveWindow() != scene_ui_index) {
				// Set the previously active window in the same border as this game window
				unsigned int previous_active_border_window = editor_state->ui_system->GetActiveWindowInsideBorderForWindow(game_ui_index);
				sandbox->before_play_active_window_in_border.DeallocateWithReset(sandbox->GlobalMemoryManager());
				// Allocate the buffer from the sandbox' global memory manager
				sandbox->before_play_active_window_in_border = editor_state->ui_system->GetWindowName(previous_active_border_window).Copy(sandbox->GlobalMemoryManager());

				editor_state->ui_system->SetActiveWindow(scene_ui_index);
			}
			return true;
		}
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetScenePath(const EditorState* editor_state, Stream<wchar_t> scene_path, CapacityStream<wchar_t>& absolute_path) {
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	unsigned int offset = absolute_path.AddStream(scene_path);
	Stream<wchar_t> replace_path = absolute_path.SliceAt(offset);
	ReplaceCharacter(replace_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
	absolute_path[absolute_path.size] = L'\0';
	absolute_path.AssertCapacity();
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxScenePath(const EditorState* editor_state, unsigned int sandbox_handle, CapacityStream<wchar_t>& path)
{
	Stream<wchar_t> scene_path = GetSandbox(editor_state, sandbox_handle)->scene_path;
	if (scene_path.size > 0) {
		GetScenePath(editor_state, scene_path, path);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

OrientedPoint GetSandboxCameraPoint(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	return { sandbox->camera_parameters[viewport].translation, sandbox->camera_parameters[viewport].rotation };
}

// -----------------------------------------------------------------------------------------------------------------------------

Camera GetSandboxCamera(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return GetSandbox(editor_state, sandbox_handle)->camera_parameters[viewport];
}

// -----------------------------------------------------------------------------------------------------------------------------

float GetSandboxViewportAspectRatio(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	ResourceView view = sandbox->viewport_render_destination[viewport].output_view;

	uint2 dimensions = GetTextureDimensions(view.AsTexture2D());
	return (float)dimensions.x / (float)dimensions.y;
}

// -----------------------------------------------------------------------------------------------------------------------------

void RenameSandboxScenePath(EditorState* editor_state, unsigned int sandbox_handle, Stream<wchar_t> new_scene, bool absolute_path)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->scene_path.size = 0;
	if (absolute_path) {
		GetScenePath(editor_state, new_scene, sandbox->scene_path);
	}
	else {
		sandbox->scene_path.CopyOther(new_scene);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void RegisterSandboxCameraTransform(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	unsigned int camera_index, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	ECS_ASSERT(camera_index < std::size(sandbox->camera_saved_orientations));

	EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_handle);
	if (sandbox_state == EDITOR_SANDBOX_SCENE) {
		sandbox->camera_saved_orientations[camera_index] = { 
			sandbox->camera_parameters[viewport].translation,
			sandbox->camera_parameters[viewport].rotation 
		};

		if (!disable_file_write) {
			SaveEditorSandboxFile(editor_state);
		}
	}
	else {
		// TODO: Implement the runtime version
		ECS_ASSERT(false);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void RotateSandboxCamera(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	float3 rotation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	// TODO: Sometimes, weird large rotations are added, investigate why
	if (fabsf(rotation.x) > 100.0f || fabsf(rotation.y) > 100.0f || fabsf(rotation.z) > 100.0f) {
		__debugbreak();
	}
	sandbox->camera_parameters[viewport].rotation += rotation;

	// Also write to the sandbox file this change such that we preserve this change - but only for the scene case
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE && !disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ResetSandboxCameras(EditorState* editor_state, unsigned int sandbox_handle)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	for (size_t index = 0; index < EDITOR_SANDBOX_VIEWPORT_COUNT; index++) {
		sandbox->camera_parameters[index].Default();
	}
	for (size_t index = 0; index < EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT; index++) {
		sandbox->camera_saved_orientations[index].ToOrigin();
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ResetSandboxTransformToolSelectedAxes(
	EditorState* editor_state,
	unsigned int sandbox_handle
) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	memset(sandbox->transform_tool_selected, 0, sizeof(sandbox->transform_tool_selected));
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveSandboxScene(EditorState* editor_state, unsigned int sandbox_handle)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	GetSandboxScenePath(editor_state, sandbox_handle, absolute_path);
	bool success = SaveEditorScene(editor_state, sandbox_handle, absolute_path);
	if (success) {
		sandbox->is_scene_dirty = false;
	}
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxCameraAspectRatio(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	float aspect_ratio = GetSandboxViewportAspectRatio(editor_state, sandbox_handle, viewport);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->camera_parameters[viewport].aspect_ratio = aspect_ratio;
	
	if (viewport == EDITOR_SANDBOX_VIEWPORT_RUNTIME) {
		// Update both camera components - if they are present
		CameraComponent* scene_component = GetSandboxGlobalComponent<CameraComponent>(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		if (scene_component != nullptr) {
			scene_component->value.aspect_ratio = aspect_ratio;
		}

		CameraComponent* runtime_component = GetSandboxGlobalComponent<CameraComponent>(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
		if (runtime_component != nullptr) {
			runtime_component->value.aspect_ratio = aspect_ratio;
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxCameraTranslation(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	float3 translation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->camera_parameters[viewport].translation = translation;

	if (!disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxCameraRotation(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	ECSEngine::float3 rotation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->camera_parameters[viewport].rotation = rotation;

	if (!disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TranslateSandboxCamera(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	float3 translation, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->camera_parameters[viewport].translation += translation;

	// Also write the sandbox file in order to have this value preserved - but only for the scene viewport
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE && !disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------