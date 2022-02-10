#include "editorpch.h"
#include "EditorEvent.h"
#include "EditorState.h"

using namespace ECSEngine;
ECS_TOOLS;
ECS_CONTAINERS;

#define WAIT_FOR_QUEUE_SPACE_SLEEP_TICK 5

void AllocateMemory(EditorState* editor_state, EditorEvent& editor_event, void* data, size_t data_size) {
	EDITOR_STATE(editor_state);

	editor_event.data = function::AllocateMemory(editor_allocator, data, data_size);
	editor_event.data_size = data_size;
}

void DeallocateMemory(EditorState* _editor_state, EditorEvent editor_event) {
	EDITOR_STATE(_editor_state);

	if (editor_event.data_size > 0) {
		editor_allocator->Deallocate(editor_event.data);
	}
}

void* AllocateErrorMessage(EditorState* editor_state, Stream<char> error_message) {
	EDITOR_STATE(editor_state);

	size_t total_size = sizeof(error_message) + error_message.size;
	void* allocation = editor_allocator->Allocate(total_size);
	Stream<char>* new_error_message = (Stream<char>*)allocation;
	new_error_message->size = 0;
	new_error_message->buffer = (char*)function::OffsetPointer(allocation, sizeof(error_message));
	new_error_message->Copy(error_message);

	return allocation;
}

void EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size) {
	EDITOR_STATE(editor_state);

	EditorEvent editor_event;
	editor_event.function = function;
	AllocateMemory(editor_state, editor_event, event_data, event_data_size);
	editor_event.data_size = event_data_size;

	while (editor_state->event_queue.GetSize() == editor_state->event_queue.GetCapacity()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_FOR_QUEUE_SPACE_SLEEP_TICK));
	}
	editor_state->event_queue.Push(editor_event);
}

void EditorAddEventWithPointer(EditorState* editor_state, EditorEventFunction function, void* event_data) {
	EDITOR_STATE(editor_state);

	EditorEvent editor_event;
	editor_event.data_size = 1;
	editor_event.data = event_data;
	editor_event.function = function;

	while (editor_state->event_queue.GetSize() == editor_state->event_queue.GetCapacity()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_FOR_QUEUE_SPACE_SLEEP_TICK));
	}
	editor_state->event_queue.Push(editor_event);
}

EDITOR_EVENT(EditorSkip) {}

EDITOR_EVENT(EditorError) {
	EDITOR_STATE(editor_state);

	Stream<char>* error_message = (Stream<char>*)_data;
	CreateErrorMessageWindow(ui_system, *error_message);
}

void EditorSetError(EditorState* editor_state, Stream<char> error_message) {
	EDITOR_STATE(editor_state);

	void* allocation = AllocateErrorMessage(editor_state, error_message);
	EditorAddEventWithPointer(editor_state, EditorError, allocation);
}

EDITOR_EVENT(EditorConsoleError)
{
	EDITOR_STATE(editor_state);

	Stream<char>* error_message = (Stream<char>*)_data;
	console->Error(*error_message, EDITOR_CONSOLE_SYSTEM_INDEX);
}

EDITOR_EVENT(EditorConsoleWarn) {
	EDITOR_STATE(editor_state);

	Stream<char>* error_message = (Stream<char>*)_data;
	console->Warn(*error_message, EDITOR_CONSOLE_SYSTEM_INDEX);
}

EDITOR_EVENT(EditorConsoleInfo) {
	EDITOR_STATE(editor_state);

	Stream<char>* error_message = (Stream<char>*)_data;
	console->Info(*error_message, EDITOR_CONSOLE_SYSTEM_INDEX);
}

EDITOR_EVENT(EditorConsoleTrace) {
	EDITOR_STATE(editor_state);

	Stream<char>* error_message = (Stream<char>*)_data;
	console->Trace(*error_message, EDITOR_CONSOLE_SYSTEM_INDEX);
}

void EditorSetConsoleError(EditorState* editor_state, Stream<char> error_message) {
	EDITOR_STATE(editor_state);

	void* allocation = AllocateErrorMessage(editor_state, error_message);
	EditorAddEventWithPointer(editor_state, EditorConsoleError, allocation);
}

void EditorSetConsoleWarn(EditorState* editor_state, Stream<char> error_message) {
	EDITOR_STATE(editor_state);

	void* allocation = AllocateErrorMessage(editor_state, error_message);
	EditorAddEventWithPointer(editor_state, EditorConsoleWarn, allocation);
}

void EditorSetConsoleInfo(EditorState* editor_state, Stream<char> error_message) {
	EDITOR_STATE(editor_state);

	void* allocation = AllocateErrorMessage(editor_state, error_message);
	EditorAddEventWithPointer(editor_state, EditorConsoleInfo, allocation);
}

void EditorSetConsoleTrace(EditorState* editor_state, Stream<char> error_message) {
	EDITOR_STATE(editor_state);

	void* allocation = AllocateErrorMessage(editor_state, error_message);
	EditorAddEventWithPointer(editor_state, EditorConsoleTrace, allocation);
}