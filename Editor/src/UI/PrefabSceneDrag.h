#pragma once

struct EditorState;

bool IsPrefabFileSelected(const EditorState* editor_state);

// This will start the prefab drag only if there is a single selected file
// And it is of prefab type
void PrefabStartDrag(EditorState* editor_state);

// This will end the prefab drag only if there is a single selected file
// And it is of prefab type
void PrefabEndDrag(EditorState* editor_state);