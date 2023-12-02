#pragma once
#include "SandboxAccessor.h"
#include "ECSEngineRendering.h"

struct EditorState;

// -------------------------------------------------------------------------------------------------------------

// The new scene needs to be the relative path from the assets folder. A special case is new_scene { nullptr, 0 } which means reset
// Returns true if the scene could be loaded with success. It will deserialize into a temporary entity manager and asset database
// If that succeeds, then it will copy into the sandbox allocator.
bool ChangeSandboxScenePath(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> new_scene);

// -------------------------------------------------------------------------------------------------------------

// This function works only for temporary sandboxes - it will just change the path without loading or unloading data
void ChangeTemporarySandboxScenePath(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> new_scene);

// -------------------------------------------------------------------------------------------------------------

// Clears the entity manager and empties the asset database reference
void ClearSandboxScene(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Fills in the absolute path of the scene given the filename
void GetScenePath(const EditorState* editor_state, ECSEngine::Stream<wchar_t> scene_path, ECSEngine::CapacityStream<wchar_t>& absolute_path);

// -------------------------------------------------------------------------------------------------------------

// Fills in the absolute path of the sandbox scene path
void GetSandboxScenePath(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::CapacityStream<wchar_t>& path
);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::OrientedPoint GetSandboxCameraPoint(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::Camera GetSandboxCamera(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

float GetSandboxViewportAspectRatio(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

// Records the current position of the camera into the given slot
void RegisterSandboxCameraTransform(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int camera_index,
	EDITOR_SANDBOX_VIEWPORT viewport,
	bool disable_file_write = false
);

// -------------------------------------------------------------------------------------------------------------

void RotateSandboxCamera(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::float3 rotation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write = false
);

// -------------------------------------------------------------------------------------------------------------

// Defaults the values for the camera and saved positions
void ResetSandboxCameras(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void ResetSandboxTransformToolSelectedAxes(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it managed to save the sandbox scene, else false.
// If it succeeds it updates the is_dirty flag to be false.
bool SaveSandboxScene(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Determines the aspect ration of the camera given size of the textures
void SetSandboxCameraAspectRatio(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

void SetSandboxCameraTranslation(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::float3 translation, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	bool disable_file_write = false
);

// -------------------------------------------------------------------------------------------------------------

void SetSandboxCameraRotation(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::float3 rotation,
	EDITOR_SANDBOX_VIEWPORT viewport,
	bool disable_file_write = false
);

// -------------------------------------------------------------------------------------------------------------

void TranslateSandboxCamera(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::float3 translation, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	bool disable_file_write = false
);

// -------------------------------------------------------------------------------------------------------------
