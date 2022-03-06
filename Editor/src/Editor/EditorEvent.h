#pragma once
#include "editorpch.h"
#include "ECSEngineUI.h"

struct EditorState;

typedef void (*EditorEventFunction)(EditorState* editor_state, void* data);

#define EDITOR_EVENT(name) void name(EditorState* editor_state, void* _data)

// Cannot place these inside a Editor namespace because it will conflict with the Editor class in Editor.cpp

void EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size = 0);

// It will take the pointer as is but will set the event data size different from 0
// in order to be deallocated; useful for example when the data is coallesced from the editor
// allocator and it must still be deallocated
void EditorAddEventWithPointer(EditorState* editor_state, EditorEventFunction function, void* event_data);