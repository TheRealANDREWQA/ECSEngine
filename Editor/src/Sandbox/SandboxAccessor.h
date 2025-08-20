#pragma once
#include "SandboxTypes.h"
#include "SandboxCount.h"
#include "../Editor/EditorState.h"

// -------------------------------------------------------------------------------------------------------------

// When it is not playing it returns the scene entities manager, when playing the runtime one
const ECSEngine::EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_handle);

// When it is not playing it returns the scene entities manager, when playing the runtime one
ECSEngine::EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_handle);

// -------------------------------------------------------------------------------------------------------------

// Returns the index of the sandbox that is active, i.e. the Game window is the focused dockspace region
// Returns -1 if there is no sandbox currently in focus
unsigned int GetActiveSandbox(const EditorState* editor_state, bool include_temporary_sandboxes = false);

// Returns the index of the sandbox that is active, i.e. the Game/Scene window is the focused dockspace region
// Returns -1 if there is no sandbox currently in focus
unsigned int GetActiveSandboxIncludeScene(const EditorState* editor_state, bool include_temporary_sandboxes = false);

// -------------------------------------------------------------------------------------------------------------

// Returns the index of the sandbox that is currently hovered by the mouse, but only for the Game windows.
// Returns -1 if there is no Game window hovered
unsigned int GetHoveredSandbox(const EditorState* editor_state);

// Returns the index of the sandbox that is currently hovered by the mouse, including both Game and Scene windows.
// Returns -1 if there is no such window hovered
unsigned int GetHoveredSandboxIncludeScene(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_handle) {
	return &editor_state->sandboxes[sandbox_handle];
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE const EditorSandbox* GetSandbox(const EditorState* editor_state, unsigned int sandbox_handle) {
	return &editor_state->sandboxes[sandbox_handle];
}

// -------------------------------------------------------------------------------------------------------------

// Returns in which state the sandbox is currently in
EDITOR_SANDBOX_STATE GetSandboxState(const EditorState* editor_state, unsigned int sandbox_handle);

// -------------------------------------------------------------------------------------------------------------

// Returns which viewport is currently displayed by the scene window
EDITOR_SANDBOX_VIEWPORT GetSandboxActiveViewport(const EditorState* editor_state, unsigned int sandbox_handle);

// -------------------------------------------------------------------------------------------------------------

// Returns the viewport of the sandbox if the given viewport is COUNT or the viewport again else
EDITOR_SANDBOX_VIEWPORT GetSandboxViewportOverride(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::EntityManager* GetSandboxEntityManager(
	EditorState* editor_state,
	unsigned int sandbox_handle,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

const ECSEngine::EntityManager* GetSandboxEntityManager(
	const EditorState* editor_state,
	unsigned int sandbox_handle,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE unsigned int GetSandboxCount(const EditorState* editor_state, bool exclude_temporary_sandboxes = false) {
	return exclude_temporary_sandboxes ? editor_state->sandboxes.set.size - editor_state->sandboxes_temporary_count : editor_state->sandboxes.set.size;
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE unsigned int GetSandboxTemporaryCount(const EditorState* editor_state) {
	return editor_state->sandboxes_temporary_count;
}

// -------------------------------------------------------------------------------------------------------------

ECSEngine::WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_handle);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE bool IsSandboxHandleValid(const EditorState* editor_state, unsigned int sandbox_handle) {
	return editor_state->sandboxes.Exists(sandbox_handle);
}

// -------------------------------------------------------------------------------------------------------------

// Returns true if at least a task/process requested the sandbox to be locked
bool IsSandboxLocked(const EditorState* editor_state, unsigned int sandbox_handle);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxViewportRendering(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE bool IsSandboxTemporary(const EditorState* editor_state, unsigned int sandbox_handle) {
	return editor_state->sandboxes.Exists(sandbox_handle) && GetSandbox(editor_state, sandbox_handle)->is_temporary;
}

// -------------------------------------------------------------------------------------------------------------

const ECSEngine::EntityManager* RuntimeSandboxEntityManager(const EditorState* editor_state, unsigned int sandbox_handle);

// -------------------------------------------------------------------------------------------------------------

bool DoesSandboxRecord(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type);

// -------------------------------------------------------------------------------------------------------------

bool DoesSandboxReplay(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type);

bool DoesSandboxReplayDriveDeltaTime(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE bool CanSandboxRecord(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type) {
	return !DoesSandboxReplay(editor_state, sandbox_handle, recording_type);
}

ECS_INLINE bool CanSandboxReplay(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type) {
	return !DoesSandboxRecord(editor_state, sandbox_handle, recording_type);
}

// -------------------------------------------------------------------------------------------------------------