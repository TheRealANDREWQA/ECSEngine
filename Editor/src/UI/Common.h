#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

// Destroys all windows with an index greater or equal than the given value until a threshold
void DestroyIndexedWindows(EditorState* editor_state, ECSEngine::Stream<char> window_root, unsigned int starting_value, unsigned int max_count);

// Destroys all windows that are directly associated with the sandbox (i.e. the Scene and Game windows)
void DestroySandboxWindows(EditorState* editor_state, unsigned int sandbox_index);