#pragma once
#include "SandboxTypes.h"
#include "../Editor/EditorState.h"

// -------------------------------------------------------------------------------------------------------------

// When it is not playing it returns the scene entities manager, when playing the runtime one
const ECSEngine::EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_index);

// When it is not playing it returns the scene entities manager, when playing the runtime one
ECSEngine::EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns the index of the sandbox that is active, i.e. the Game window is the focused dockspace region
// Returns -1 if there is no sandbox currently in focus
unsigned int GetActiveSandbox(const EditorState* editor_state);

// Returns the index of the sandbox that is active, i.e. the Game/Scene window is the focused dockspace region
// Returns -1 if there is no sandbox currently in focus
unsigned int GetActiveSandboxIncludeScene(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Returns the index of the sandbox that is currently hovered by the mouse, but only for the Game windows.
// Returns -1 if there is no Game window hovered
unsigned int GetHoveredSandbox(const EditorState* editor_state);

// Returns the index of the sandbox that is currently hovered by the mouse, including both Game and Scene windows.
// Returns -1 if there is no such window hovered
unsigned int GetHoveredSandboxIncludeScene(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_index) {
	return editor_state->sandboxes.buffer + sandbox_index;
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE const EditorSandbox* GetSandbox(const EditorState* editor_state, unsigned int sandbox_index) {
	return editor_state->sandboxes.buffer + sandbox_index;
}

// -------------------------------------------------------------------------------------------------------------

// Returns in which state the sandbox is currently in
EDITOR_SANDBOX_STATE GetSandboxState(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns which viewport is currently displayed by the scene window
EDITOR_SANDBOX_VIEWPORT GetSandboxActiveViewport(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns the viewport of the sandbox if the given viewport is COUNT or the viewport again else
EDITOR_SANDBOX_VIEWPORT GetSandboxViewportOverride(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::EntityManager* GetSandboxEntityManager(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

const ECSEngine::EntityManager* GetSandboxEntityManager(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE unsigned int GetSandboxCount(const EditorState* editor_state, bool exclude_temporary_sandboxes = false) {
	return exclude_temporary_sandboxes ? editor_state->sandboxes.size - editor_state->sandboxes_temporary_count : editor_state->sandboxes.size;
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE unsigned int GetSandboxTemporaryCount(const EditorState* editor_state) {
	return editor_state->sandboxes_temporary_count;
}

// -------------------------------------------------------------------------------------------------------------

ECSEngine::WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if at least a task/process requested the sandbox to be locked
bool IsSandboxLocked(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxIndexValid(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxViewportRendering(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE bool IsSandboxTemporary(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandboxCount(editor_state, true) <= sandbox_index && sandbox_index < GetSandboxCount(editor_state);
}

// -------------------------------------------------------------------------------------------------------------

const ECSEngine::EntityManager* RuntimeSandboxEntityManager(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE bool DoesSandboxRecordInput(const EditorState* editor_state, unsigned int sandbox_index) {
	return HasFlag(GetSandbox(editor_state, sandbox_index)->flags, EDITOR_SANDBOX_FLAG_RECORD_INPUT);
}

ECS_INLINE bool DoesSandboxRecordState(const EditorState* editor_state, unsigned int sandbox_index) {
	return HasFlag(GetSandbox(editor_state, sandbox_index)->flags, EDITOR_SANDBOX_FLAG_RECORD_STATE);
}

ECS_INLINE bool DoesSandboxReplayInput(const EditorState* editor_state, unsigned int sandbox_index) {
	return HasFlag(GetSandbox(editor_state, sandbox_index)->flags, EDITOR_SANDBOX_FLAG_REPLAY_INPUT);
}

ECS_INLINE bool DoesSandboxReplayState(const EditorState* editor_state, unsigned int sandbox_index) {
	return HasFlag(GetSandbox(editor_state, sandbox_index)->flags, EDITOR_SANDBOX_FLAG_REPLAY_STATE);
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE bool CanSandboxRecordInput(const EditorState* editor_state, unsigned int sandbox_index) {
	return !DoesSandboxReplayInput(editor_state, sandbox_index);
}

ECS_INLINE bool CanSandboxRecordState(const EditorState* editor_state, unsigned int sandbox_index) {
	return !DoesSandboxReplayState(editor_state, sandbox_index);
}

ECS_INLINE bool CanSandboxReplayInput(const EditorState* editor_state, unsigned int sandbox_index) {
	return !DoesSandboxRecordInput(editor_state, sandbox_index);
}

ECS_INLINE bool CanSandboxReplayState(const EditorState* editor_state, unsigned int sandbox_index) {
	return !DoesSandboxRecordState(editor_state, sandbox_index);
}

// -------------------------------------------------------------------------------------------------------------