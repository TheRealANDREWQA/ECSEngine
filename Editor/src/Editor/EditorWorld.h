#pragma once
#include "editorpch.h"

struct EditorState;

void EditorStateInitializeWorld(EditorState* editor_state, unsigned int index);

void EditorStateReleaseWorld(EditorState* editor_state, unsigned int index);

void EditorStateCopyWorld(EditorState* editor_state, unsigned int destination_index, unsigned int source_index);

// Changes the index of the active world to the next world and copies the data from the scene world into
// the world which has been active up until now. Makes sure that there is at least one scene world ready
void EditorStateNextWorld(EditorState* editor_state);

void EditorStateSimulateWorld(ECSEngine::World* world);