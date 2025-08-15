#include "editorpch.h"
#include "SandboxAccessor.h"
#include "../UI/Game.h"
#include "../UI/Scene.h"
#include "SandboxReplay.h"
#include "SandboxRecording.h"

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------------------------

const EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_handle)
{
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_handle);
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	if (state == EDITOR_SANDBOX_SCENE) {
		return &sandbox->scene_entities;
	}
	return sandbox->sandbox_world.entity_manager;
}

// ------------------------------------------------------------------------------------------------------------------------------

EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_handle) {
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_handle);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	if (state == EDITOR_SANDBOX_SCENE) {
		return &sandbox->scene_entities;
	}
	return sandbox->sandbox_world.entity_manager;
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetActiveSandbox(const EditorState* editor_state, bool include_temporary_sandboxes) {
	unsigned int sandbox_handle = GameUITargetSandbox(editor_state, editor_state->ui_system->GetActiveWindow());
	if (sandbox_handle != -1) {
		if (include_temporary_sandboxes && IsSandboxTemporary(editor_state, sandbox_handle)) {
			sandbox_handle = -1;
		}
	}
	return sandbox_handle;
}

unsigned int GetActiveSandboxIncludeScene(const EditorState* editor_state, bool include_temporary_sandboxes) {
	unsigned int active_sandbox = GetActiveSandbox(editor_state, include_temporary_sandboxes);
	if (active_sandbox == -1) {
		active_sandbox = SceneUITargetSandbox(editor_state, editor_state->ui_system->GetActiveWindow());
		if (active_sandbox != -1) {
			if (include_temporary_sandboxes && IsSandboxTemporary(editor_state, active_sandbox)) {
				active_sandbox = -1;
			}
		}
	}
	return active_sandbox;
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetHoveredSandbox(const EditorState* editor_state) {
	return GameUITargetSandbox(editor_state, editor_state->ui_system->GetHoveredWindow());
}

unsigned int GetHoveredSandboxIncludeScene(const EditorState* editor_state) {
	unsigned int hovered_window = editor_state->ui_system->GetHoveredWindow();
	unsigned int hovered_sandbox = GameUITargetSandbox(editor_state, hovered_window);
	if (hovered_sandbox == -1) {
		hovered_sandbox = SceneUITargetSandbox(editor_state, hovered_window);
	}
	return hovered_sandbox;
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_STATE GetSandboxState(const EditorState* editor_state, unsigned int sandbox_handle)
{
	return GetSandbox(editor_state, sandbox_handle)->run_state;
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_VIEWPORT GetSandboxActiveViewport(const EditorState* editor_state, unsigned int sandbox_handle)
{
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_handle);
	if (state == EDITOR_SANDBOX_SCENE) {
		return EDITOR_SANDBOX_VIEWPORT_SCENE;
	}
	else if (state == EDITOR_SANDBOX_PAUSED || state == EDITOR_SANDBOX_RUNNING) {
		return EDITOR_SANDBOX_VIEWPORT_RUNTIME;
	}
	else {
		ECS_ASSERT(false);
		return EDITOR_SANDBOX_VIEWPORT_COUNT;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_VIEWPORT GetSandboxViewportOverride(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return viewport == EDITOR_SANDBOX_VIEWPORT_COUNT ? GetSandboxActiveViewport(editor_state, sandbox_handle) : viewport;
}

// ------------------------------------------------------------------------------------------------------------------------------

EntityManager* GetSandboxEntityManager(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	switch (viewport) {
	case EDITOR_SANDBOX_VIEWPORT_SCENE:
		return &sandbox->scene_entities;
	case EDITOR_SANDBOX_VIEWPORT_RUNTIME:
		return sandbox->sandbox_world.entity_manager;
	case EDITOR_SANDBOX_VIEWPORT_COUNT:
		return ActiveEntityManager(editor_state, sandbox_handle);
	}

	ECS_ASSERT(false, "Invalid viewport value!");
	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

const EntityManager* GetSandboxEntityManager(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	switch (viewport) {
	case EDITOR_SANDBOX_VIEWPORT_SCENE:
		return &sandbox->scene_entities;
	case EDITOR_SANDBOX_VIEWPORT_RUNTIME:
		return sandbox->sandbox_world.entity_manager;
	case EDITOR_SANDBOX_VIEWPORT_COUNT:
		return ActiveEntityManager(editor_state, sandbox_handle);
	}

	ECS_ASSERT(false, "Invalid viewport value!");
	return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------

WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_handle)
{
	return &GetSandbox(editor_state, sandbox_handle)->runtime_descriptor;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxLocked(const EditorState* editor_state, unsigned int sandbox_handle)
{
	return GetSandbox(editor_state, sandbox_handle)->locked_count.load(ECS_RELAXED) > 0;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxIndexValid(const EditorState* editor_state, unsigned int sandbox_handle)
{
	return sandbox_handle < GetSandboxCount(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxViewportRendering(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return GetSandbox(editor_state, sandbox_handle)->viewport_enable_rendering[viewport];
}

// -----------------------------------------------------------------------------------------------------------------------------

const EntityManager* RuntimeSandboxEntityManager(const EditorState* editor_state, unsigned int sandbox_handle)
{
	return GetSandbox(editor_state, sandbox_handle)->sandbox_world.entity_manager;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool DoesSandboxRecord(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type) {
	// It is safe to cast editor_state, since the function does not change the values, only returns mutable pointers that we are not changing
	SandboxRecordingInfo recording_info = GetSandboxRecordingInfo((EditorState*)editor_state, sandbox_handle, recording_type);
	return HasFlag(GetSandbox(editor_state, sandbox_handle)->flags, recording_info.flag);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool DoesSandboxReplay(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type) {
	// It is safe to cast editor_state, since the function does not change the values, only returns mutable pointers that we are not changing
	SandboxReplayInfo replay_info = GetSandboxReplayInfo((EditorState*)editor_state, sandbox_handle, recording_type);
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	return HasFlag(sandbox->flags, replay_info.flag) && !replay_info.replay->delta_reader.IsFailed() && !replay_info.replay->delta_reader.IsFinished();
}

bool DoesSandboxReplayDriveDeltaTime(const EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE recording_type) {
	// It is safe to cast editor_state, since the function does not change the values, only returns mutable pointers that we are not changing
	SandboxReplayInfo replay_info = GetSandboxReplayInfo((EditorState*)editor_state, sandbox_handle, recording_type);
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	return HasFlag(sandbox->flags, replay_info.flag) && !replay_info.replay->delta_reader.IsFailed() && !replay_info.replay->delta_reader.IsFinished() && replay_info.replay->is_driving_delta_time;
}

// -----------------------------------------------------------------------------------------------------------------------------