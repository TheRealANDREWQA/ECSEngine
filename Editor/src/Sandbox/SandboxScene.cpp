#include "editorpch.h"
#include "SandboxScene.h"
#include "Sandbox.h"
#include "SandboxEntityOperations.h"
#include "SandboxFile.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorScene.h"
#include "../Editor/EditorState.h"

using namespace ECSEngine;

// -----------------------------------------------------------------------------------------------------------------------------

struct ChangeSceneRenderViewportsEventData {
	unsigned int sandbox_index;
};

EDITOR_EVENT(ChangeSceneRenderViewportsEvent) {
	ChangeSceneRenderViewportsEventData* data = (ChangeSceneRenderViewportsEventData*)_data;

	// Trigger a rerender of the viewport
	RenderSandboxViewports(editor_state, data->sandbox_index);
	return false;
}

bool ChangeSandboxScenePath(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> new_scene)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	ClearSandboxScene(editor_state, sandbox_index);
	sandbox->scene_path.size = 0;

	if (new_scene.size == 0) {
		// Setting the components needs to be done right before exiting
		editor_state->editor_components.SetManagerComponents(&sandbox->scene_entities);
		return true;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetScenePath(editor_state, new_scene, absolute_path);

	bool success = LoadEditorSceneCore(editor_state, sandbox_index, absolute_path);
	if (success) {
		// Copy the contents
		sandbox->scene_path.CopyOther(new_scene);
		sandbox->is_scene_dirty = false;

		// Load the assets and trigger a re-render of the viewports
		ChangeSceneRenderViewportsEventData event_data = { sandbox_index };
		LoadEditorAssetsOptionalData load_optional_data;
		load_optional_data.callback = ChangeSceneRenderViewportsEvent;
		load_optional_data.callback_data = &event_data;
		load_optional_data.callback_data_size = sizeof(event_data);
		LoadSandboxAssets(editor_state, sandbox_index, &load_optional_data);
	}

	// If the load failed, the scene will be reset with an empty value
	editor_state->editor_components.SetManagerComponents(&sandbox->scene_entities);
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ChangeTemporarySandboxScenePath(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> new_scene)
{
	ECS_ASSERT(IsSandboxTemporary(editor_state, sandbox_index));
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->scene_path.size = 0;
	GetScenePath(editor_state, new_scene, sandbox->scene_path);
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearSandboxScene(EditorState* editor_state, unsigned int sandbox_index)
{
	// Unload the assets currently used by this sandbox and reset the scene entities
	UnloadSandboxAssets(editor_state, sandbox_index);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->scene_entities.Reset();
	sandbox->database.Reset();
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->sandbox_world.entity_manager->CopyOther(&sandbox->scene_entities);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetScenePath(const EditorState* editor_state, Stream<wchar_t> scene_path, CapacityStream<wchar_t>& absolute_path) {
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStream(scene_path);
	absolute_path[absolute_path.size] = L'\0';
	absolute_path.AssertCapacity();
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxScenePath(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<wchar_t>& path)
{
	Stream<wchar_t> scene_path = GetSandbox(editor_state, sandbox_index)->scene_path;
	if (scene_path.size > 0) {
		GetScenePath(editor_state, scene_path, path);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

OrientedPoint GetSandboxCameraPoint(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return { sandbox->camera_parameters[viewport].translation, sandbox->camera_parameters[viewport].rotation };
}

// -----------------------------------------------------------------------------------------------------------------------------

Camera GetSandboxCamera(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return GetSandbox(editor_state, sandbox_index)->camera_parameters[viewport];
}

// -----------------------------------------------------------------------------------------------------------------------------

float GetSandboxViewportAspectRatio(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ResourceView view = sandbox->viewport_render_destination[viewport].output_view;

	uint2 dimensions = GetTextureDimensions(view.AsTexture2D());
	return (float)dimensions.x / (float)dimensions.y;
}

// -----------------------------------------------------------------------------------------------------------------------------

void RegisterSandboxCameraTransform(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	unsigned int camera_index, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_ASSERT(camera_index < std::size(sandbox->camera_saved_orientations));

	EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
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
	unsigned int sandbox_index, 
	float3 rotation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters[viewport].rotation += rotation;

	// Also write to the sandbox file this change such that we preserve this change - but only for the scene case
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE && !disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ResetSandboxCameras(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
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
	unsigned int sandbox_index
) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	memset(sandbox->transform_tool_selected, 0, sizeof(sandbox->transform_tool_selected));
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveSandboxScene(EditorState* editor_state, unsigned int sandbox_index)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	GetSandboxScenePath(editor_state, sandbox_index, absolute_path);
	bool success = SaveEditorScene(editor_state, &sandbox->scene_entities, &sandbox->database, absolute_path);
	if (success) {
		sandbox->is_scene_dirty = false;
	}
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxCameraAspectRatio(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	float aspect_ratio = GetSandboxViewportAspectRatio(editor_state, sandbox_index, viewport);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters[viewport].aspect_ratio = aspect_ratio;
	
	if (viewport == EDITOR_SANDBOX_VIEWPORT_RUNTIME) {
		// Update both camera components - if they are present
		CameraComponent* scene_component = GetSandboxGlobalComponent<CameraComponent>(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		if (scene_component != nullptr) {
			scene_component->value.aspect_ratio = aspect_ratio;
		}

		CameraComponent* runtime_component = GetSandboxGlobalComponent<CameraComponent>(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
		if (runtime_component != nullptr) {
			runtime_component->value.aspect_ratio = aspect_ratio;
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxCameraTranslation(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	float3 translation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters[viewport].translation = translation;

	if (!disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxCameraRotation(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::float3 rotation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters[viewport].rotation = rotation;

	if (!disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TranslateSandboxCamera(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	float3 translation, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	bool disable_file_write
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters[viewport].translation += translation;

	// Also write the sandbox file in order to have this value preserved - but only for the scene viewport
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE && !disable_file_write) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------