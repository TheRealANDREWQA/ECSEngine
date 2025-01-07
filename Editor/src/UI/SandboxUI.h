#pragma once

struct EditorState;

// Duplicates all the sandbox related UI windows from a given source sandbox for a destination
// Sandbox, with the option of splitting the dockspace region, or adding it in the same region
void DuplicateSandboxUIForDifferentSandbox(
	EditorState* editor_state, 
	unsigned int source_sandbox_index, 
	unsigned int destination_sandbox_index, 
	bool split_region = true
);

// Destroys all windows that are directly associated with the sandbox (i.e. the Scene and Game windows)
void DestroySandboxWindows(EditorState* editor_state, unsigned int sandbox_index);

// Destroys all windows that are indirectly associated with the given sandbox (i.e. the entities and inspector windows)
void DestroySandboxAuxiliaryWindows(EditorState* editor_state, unsigned int sandbox_index);