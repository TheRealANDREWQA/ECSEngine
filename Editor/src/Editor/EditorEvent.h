#pragma once
#include "EditorEventDef.h"
#include "ECSEngineContainers.h"

// Single threaded. It returns the pointer of data allocated or given if the event_data_size is 0
// Cannot place these inside a Editor namespace because it will conflict with the Editor class in Editor.cpp
void* EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size = 0);

// Single threaded.
// It will take the pointer as is but will set the event data size different from 0
// in order to be deallocated; useful for example when the data is coallesced from the editor
// allocator and it must still be deallocated
void EditorAddEventWithPointer(EditorState* editor_state, EditorEventFunction function, void* event_data);

// Returns the last event data
void* EditorEventLastData(const EditorState* editor_state);

// Returns true if it finds an event with the given function
bool EditorHasEvent(const EditorState* editor_state, EditorEventFunction function);

// Returns the data for an event if it finds the givne function else nullptr
void* EditorGetEventData(const EditorState* editor_state, EditorEventFunction function);

// Fills in the stream with the data pointers for all events currently stored with the given function
void EditorGetEventTypeData(const EditorState* editor_state, EditorEventFunction function, ECSEngine::CapacityStream<void*>* data);