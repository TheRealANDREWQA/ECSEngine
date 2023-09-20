#include "editorpch.h"
#include "SandboxAccessor.h"
#include "../Editor/EditorState.h"

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------------------------

const EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_index)
{
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_index);
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (state == EDITOR_SANDBOX_SCENE) {
		return &sandbox->scene_entities;
	}
	return sandbox->sandbox_world.entity_manager;
}

// ------------------------------------------------------------------------------------------------------------------------------

EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_index) {
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_index);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (state == EDITOR_SANDBOX_SCENE) {
		return &sandbox->scene_entities;
	}
	return sandbox->sandbox_world.entity_manager;
}

// -----------------------------------------------------------------------------------------------------------------------------

// Make this inline such that the functions in this translation unit will have this inlined
EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_index) {
	return editor_state->sandboxes.buffer + sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

const EditorSandbox* GetSandbox(const EditorState* editor_state, unsigned int sandbox_index) {
	return editor_state->sandboxes.buffer + sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_STATE GetSandboxState(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->run_state;
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_VIEWPORT GetSandboxActiveViewport(const EditorState* editor_state, unsigned int sandbox_index)
{
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_index);
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

EDITOR_SANDBOX_VIEWPORT GetSandboxViewportOverride(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return viewport == EDITOR_SANDBOX_VIEWPORT_COUNT ? GetSandboxActiveViewport(editor_state, sandbox_index) : viewport;
}

// ------------------------------------------------------------------------------------------------------------------------------

EntityManager* GetSandboxEntityManager(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	switch (viewport) {
	case EDITOR_SANDBOX_VIEWPORT_SCENE:
		return &sandbox->scene_entities;
	case EDITOR_SANDBOX_VIEWPORT_RUNTIME:
		return sandbox->sandbox_world.entity_manager;
	case EDITOR_SANDBOX_VIEWPORT_COUNT:
		return ActiveEntityManager(editor_state, sandbox_index);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

const EntityManager* GetSandboxEntityManager(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	switch (viewport) {
	case EDITOR_SANDBOX_VIEWPORT_SCENE:
		return &sandbox->scene_entities;
	case EDITOR_SANDBOX_VIEWPORT_RUNTIME:
		return sandbox->sandbox_world.entity_manager;
	case EDITOR_SANDBOX_VIEWPORT_COUNT:
		return ActiveEntityManager(editor_state, sandbox_index);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetSandboxCount(const EditorState* editor_state)
{
	return editor_state->sandboxes.size;
}

// -----------------------------------------------------------------------------------------------------------------------------

WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_index)
{
	return &GetSandbox(editor_state, sandbox_index)->runtime_descriptor;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxLocked(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->locked_count.load(ECS_RELAXED) > 0;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxIndexValid(const EditorState* editor_state, unsigned int sandbox_index)
{
	return sandbox_index < GetSandboxCount(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxViewportRendering(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport];
}

// -----------------------------------------------------------------------------------------------------------------------------

const EntityManager* RuntimeSandboxEntityManager(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->sandbox_world.entity_manager;
}

// -----------------------------------------------------------------------------------------------------------------------------