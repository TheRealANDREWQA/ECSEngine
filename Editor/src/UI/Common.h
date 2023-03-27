#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

// Destroys all windows with an index greater or equal than the given value until a threshold
void DestroyIndexedWindows(EditorState* editor_state, ECSEngine::Stream<char> window_root, unsigned int starting_value, unsigned int max_count);