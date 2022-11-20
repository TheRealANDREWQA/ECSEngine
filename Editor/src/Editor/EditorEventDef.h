#pragma once
#include <stdint.h>

struct EditorState;

typedef bool (*EditorEventFunction)(EditorState* editor_state, void* data);

// Return true if you want the event to be pushed again
#define EDITOR_EVENT(name) bool name(EditorState* editor_state, void* _data)

struct EditorEvent {
	EditorEventFunction function;
	void* data;
	size_t data_size;
};