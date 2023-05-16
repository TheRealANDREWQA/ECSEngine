#include "editorpch.h"
#include "EditorEvent.h"
#include "EditorState.h"

using namespace ECSEngine;
ECS_TOOLS;

#define WAIT_FOR_QUEUE_SPACE_SLEEP_TICK 5

void AllocateMemory(EditorState* editor_state, EditorEvent& editor_event, void* data, size_t data_size) {
	editor_event.data = function::CopyNonZero(editor_state->EditorAllocator(), data, data_size);
	editor_event.data_size = data_size;
}

void DeallocateMemory(EditorState* editor_state, EditorEvent editor_event) {
	if (editor_event.data_size > 0) {
		Deallocate(editor_state->EditorAllocator(), editor_event.data);
	}
}

void* AllocateErrorMessage(EditorState* editor_state, Stream<char> error_message) {
	size_t total_size = sizeof(error_message) + error_message.size;
	void* allocation = Allocate(editor_state->EditorAllocator(), total_size);
	Stream<char>* new_error_message = (Stream<char>*)allocation;
	new_error_message->size = 0;
	new_error_message->buffer = (char*)function::OffsetPointer(allocation, sizeof(error_message));
	new_error_message->Copy(error_message);

	return allocation;
}

void* EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size) {
	EditorEvent editor_event;
	editor_event.function = function;
	AllocateMemory(editor_state, editor_event, event_data, event_data_size);
	editor_event.data_size = event_data_size;

	while (editor_state->event_queue.GetSize() == editor_state->event_queue.GetCapacity()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_FOR_QUEUE_SPACE_SLEEP_TICK));
	}
	editor_state->event_queue.Push(editor_event);

	return editor_event.data;
}

void EditorAddEventWithPointer(EditorState* editor_state, EditorEventFunction function, void* event_data) {
	EditorEvent editor_event;
	editor_event.data_size = 1;
	editor_event.data = event_data;
	editor_event.function = function;

	while (editor_state->event_queue.GetSize() == editor_state->event_queue.GetCapacity()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_FOR_QUEUE_SPACE_SLEEP_TICK));
	}
	editor_state->event_queue.Push(editor_event);
}

void* EditorEventLastData(const EditorState* editor_state)
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
