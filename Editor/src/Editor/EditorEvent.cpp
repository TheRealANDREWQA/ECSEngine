#include "editorpch.h"
#include "EditorEvent.h"
#include "EditorState.h"

using namespace ECSEngine;
ECS_TOOLS;

#define WAIT_FOR_QUEUE_SPACE_SLEEP_TICK 5

void AllocateMemory(EditorState* editor_state, EditorEvent& editor_event, void* data, size_t data_size) {
	editor_event.data = function::CopyNonZero(editor_state->MultithreadedEditorAllocator(), data, data_size);
	editor_event.data_size = data_size;
}

void DeallocateMemory(EditorState* editor_state, EditorEvent editor_event) {
	if (editor_event.data_size > 0) {
		Deallocate(editor_state->MultithreadedEditorAllocator(), editor_event.data);
	}
}

void* EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size) {
	EditorEvent editor_event;
	editor_event.function = function;
	AllocateMemory(editor_state, editor_event, event_data, event_data_size);
	editor_event.data_size = event_data_size;

	editor_state->event_queue.Push(editor_event);
	return editor_event.data;
}

void* EditorEventLastData(EditorState* editor_state)
{
	EditorEvent event_;
	if (editor_state->event_queue.Peek(event_)) {
		return event_.data;
	}
	return nullptr;
}

bool EditorHasEvent(const EditorState* editor_state, EditorEventFunction function)
{
	return editor_state->event_queue.ForEach<true>([function](EditorEvent event_) {
		return event_.function == function;
	});
}

void* EditorGetEventData(const EditorState* editor_state, EditorEventFunction function)
{
	void* ptr = nullptr;
	editor_state->event_queue.ForEach<true>([&ptr, function](EditorEvent event_) {
		if (event_.function == function) {
			ptr = event_.data;
			return true;
		}
		return false;
	});
	return ptr;
}

void EditorGetEventTypeData(const EditorState* editor_state, EditorEventFunction function, ECSEngine::CapacityStream<void*>* data)
{
	editor_state->event_queue.ForEach([data, function](EditorEvent event_) {
		if (event_.function == function) {
			data->AddAssert(event_.data);
		}
	});
}
